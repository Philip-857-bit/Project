/**
 * @file main.cpp
 * @brief Smart Fish Feeder ESP32 Main Entry Point
 * 
 * For LILYGO T-A7670 R2 (Main Controller)
 * Build with: pio run -e t-a7670
 * 
 * Dual-core architecture:
 * - Core 0: Communication (MQTT, GSM/4G LTE, WiFi)
 * - Core 1: Sensor reading, feeding control, power management
 */

#ifndef ESP32_CAM  // Only compile for main controller, not ESP32-CAM

#include <Arduino.h>
#include <WiFi.h>
#include <esp_system.h>
#include <esp_sleep.h>
#include <esp_task_wdt.h>

#include "../include/config.h"
#include "managers/DeviceManager.h"
#include "managers/SensorManager.h"
#include "managers/FeedingController.h"
#include "managers/PowerManager.h"
#include "managers/CommunicationManager.h"
#include "storage/NVSStorage.h"

// Task handles for dual-core operation
TaskHandle_t communicationTask = NULL;
TaskHandle_t controlTask = NULL;

// Manager instances
DeviceManager deviceManager;
SensorManager sensorManager;
FeedingController feedingController;
PowerManager powerManager;
CommunicationManager commManager;
NVSStorage nvsStorage;

// Timing variables
unsigned long lastTelemetryTime = 0;
unsigned long lastSensorReadTime = 0;

// Forward declarations
void communicationTaskFunc(void* parameter);
void controlTaskFunc(void* parameter);
void handleWakeupReason();
void enterDeepSleep();
void checkSensorAlerts();

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n========================================");
    Serial.println("Smart Fish Feeder - ESP32-WROVER");
    Serial.printf("Firmware Version: %s\n", FIRMWARE_VERSION);
    Serial.printf("Build: %s %s\n", FIRMWARE_BUILD_DATE, FIRMWARE_BUILD_TIME);
    Serial.println("========================================\n");
    
    // Initialize watchdog timer
    esp_task_wdt_init(WATCHDOG_TIMEOUT_MS / 1000, true);
    esp_task_wdt_add(NULL);
    
    // Handle wake-up reason
    handleWakeupReason();
    
    // Initialize NVS storage
    if (!nvsStorage.begin()) {
        Serial.println("[ERROR] Failed to initialize NVS storage");
    }
    
    // Initialize device manager (loads device ID, certificates)
    if (!deviceManager.begin(&nvsStorage)) {
        Serial.println("[ERROR] Failed to initialize device manager");
        Serial.println("[INFO] Entering provisioning mode...");
        deviceManager.enterProvisioningMode();
    }
    
    Serial.printf("[INFO] Device ID: %s\n", deviceManager.getDeviceID().c_str());
    
    // Initialize power manager
    if (!powerManager.begin()) {
        Serial.println("[ERROR] Failed to initialize power manager");
    }
    powerManager.printStatus();
    
    // Initialize sensor manager
    if (!sensorManager.begin()) {
        Serial.println("[ERROR] Failed to initialize sensor manager");
    }
    
    // Initialize feeding controller
    if (!feedingController.begin(&sensorManager, &nvsStorage)) {
        Serial.println("[ERROR] Failed to initialize feeding controller");
    }
    
    // Initialize communication manager
    if (!commManager.begin(&deviceManager, &nvsStorage)) {
        Serial.println("[ERROR] Failed to initialize communication manager");
    }
    
    // Create communication task on Core 0
    xTaskCreatePinnedToCore(
        communicationTaskFunc,
        "CommTask",
        8192,
        NULL,
        1,
        &communicationTask,
        0  // Core 0
    );
    
    // Create control task on Core 1
    xTaskCreatePinnedToCore(
        controlTaskFunc,
        "ControlTask",
        8192,
        NULL,
        1,
        &controlTask,
        1  // Core 1
    );
    
    Serial.println("[INFO] System initialization complete");
}

void loop() {
    // Main loop handles watchdog and deep sleep decisions
    esp_task_wdt_reset();
    
    // Check if we should enter deep sleep
    if (powerManager.shouldEnterDeepSleep() && !feedingController.isFeedingActive()) {
        enterDeepSleep();
    }
    
    delay(100);
}

/**
 * Communication task - runs on Core 0
 * Handles MQTT, GSM/WiFi connectivity, and message processing
 */
void communicationTaskFunc(void* parameter) {
    Serial.println("[Core 0] Communication task started");
    
    for (;;) {
        // Maintain connectivity
        commManager.loop();
        
        // Send telemetry at configured interval
        unsigned long now = millis();
        if (now - lastTelemetryTime >= TELEMETRY_INTERVAL_MS) {
            lastTelemetryTime = now;
            
            // Build and send telemetry
            SensorData data = sensorManager.getCurrentData();
            PowerStatus power = powerManager.getStatus();
            
            commManager.sendTelemetry(data, power);
        }
        
        // Process incoming commands
        commManager.processIncomingMessages();
        
        // Flush offline buffer if connected
        if (commManager.isConnected()) {
            commManager.flushOfflineBuffer();
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/**
 * Control task - runs on Core 1
 * Handles sensor reading, feeding control, and power management
 */
void controlTaskFunc(void* parameter) {
    Serial.println("[Core 1] Control task started");
    
    for (;;) {
        unsigned long now = millis();
        
        // Read sensors at configured interval
        if (now - lastSensorReadTime >= SENSOR_READ_INTERVAL_MS) {
            lastSensorReadTime = now;
            sensorManager.update();
            
            // Check for alerts
            checkSensorAlerts();
        }
        
        // Update feeding controller
        feedingController.update();
        
        // Update power manager
        powerManager.update();
        
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

/**
 * Check sensor readings and generate alerts if needed
 */
void checkSensorAlerts() {
    SensorData data = sensorManager.getCurrentData();
    
    // Check dissolved oxygen - emergency stop if critical
    if (data.dissolvedOxygen < DO_EMERGENCY_STOP_MG_L && data.dissolvedOxygen > 0) {
        Serial.println("[ALERT] Critical DO level - emergency feeding stop!");
        feedingController.stopFeeding();
        commManager.sendAlert(AlertType::LOW_OXYGEN, AlertSeverity::SEVERITY_CRITICAL,
            "Dissolved oxygen critically low: " + String(data.dissolvedOxygen) + " mg/L");
    }
    
    // Check temperature bounds
    if (data.temperature < TEMP_MIN_VALID || data.temperature > TEMP_MAX_VALID) {
        if (data.temperature > 0) {  // Valid reading
            commManager.sendAlert(
                data.temperature > TEMP_MAX_VALID ? AlertType::HIGH_TEMPERATURE : AlertType::LOW_TEMPERATURE,
                AlertSeverity::SEVERITY_HIGH,
                "Temperature out of range: " + String(data.temperature) + "°C"
            );
        }
    }
    
    // Check feed level
    if (data.feedLevelPercent < BATTERY_LOW_THRESHOLD) {
        commManager.sendAlert(AlertType::LOW_FEED, AlertSeverity::SEVERITY_MEDIUM,
            "Feed level low: " + String(data.feedLevelPercent) + "%");
    }
    
    // Check battery
    PowerStatus power = powerManager.getStatus();
    if (power.batteryPercent < BATTERY_LOW_THRESHOLD) {
        AlertSeverity severity = power.batteryPercent < BATTERY_CRITICAL ? 
            AlertSeverity::SEVERITY_CRITICAL : AlertSeverity::SEVERITY_HIGH;
        commManager.sendAlert(AlertType::LOW_BATTERY, severity,
            "Battery low: " + String(power.batteryPercent) + "%");
    }
}

/**
 * Handle ESP32 wake-up reason
 */
void handleWakeupReason() {
    esp_sleep_wakeup_cause_t wakeupReason = esp_sleep_get_wakeup_cause();
    
    switch (wakeupReason) {
        case ESP_SLEEP_WAKEUP_TIMER:
            Serial.println("[INFO] Woke up from timer (scheduled feeding)");
            break;
        case ESP_SLEEP_WAKEUP_EXT0:
            Serial.println("[INFO] Woke up from external interrupt");
            break;
        case ESP_SLEEP_WAKEUP_TOUCHPAD:
            Serial.println("[INFO] Woke up from touchpad");
            break;
        default:
            Serial.println("[INFO] Normal boot (not from deep sleep)");
            break;
    }
}

/**
 * Enter deep sleep mode
 */
void enterDeepSleep() {
    Serial.println("[INFO] Entering deep sleep...");
    
    // Calculate sleep duration based on next feeding
    uint64_t sleepDuration = feedingController.getTimeToNextFeeding();
    if (sleepDuration > DEEP_SLEEP_DURATION_US) {
        sleepDuration = DEEP_SLEEP_DURATION_US;
    }
    
    // Wake up before feeding time
    if (sleepDuration > WAKE_BEFORE_FEED_MS * 1000) {
        sleepDuration -= WAKE_BEFORE_FEED_MS * 1000;
    }
    
    // Disconnect cleanly
    commManager.disconnect();
    
    // Configure wake-up timer
    esp_sleep_enable_timer_wakeup(sleepDuration);
    
    // Enter deep sleep
    Serial.printf("[INFO] Sleeping for %llu seconds\n", sleepDuration / 1000000);
    esp_deep_sleep_start();
}


#endif // !ESP32_CAM
