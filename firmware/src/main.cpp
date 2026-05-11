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
#include "managers/SystemDiagnostics.h"
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
SystemDiagnostics systemDiagnostics;
NVSStorage nvsStorage;

// Timing variables
unsigned long lastTelemetryTime = 0;
unsigned long lastSensorReadTime = 0;
unsigned long lastAlertTime = 0;
#ifdef WOKWI_SIM
unsigned long lastSimHeartbeatTime = 0;
#endif

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
#ifdef NO_SOLAR_INPUT
    // Battery-only deployment: deep sleep would trigger on every low-battery check
    // because there is never solar to recharge mid-run. Keep running until critical.
    powerManager.setDeepSleepEnabled(false);
#endif
    
    // Initialize sensor manager
    if (!sensorManager.begin()) {
        Serial.println("[ERROR] Failed to initialize sensor manager");
    }
    pinMode(PIN_FEED_BTN, INPUT);  // External pull-up R7(10kOhm) on PCB - do NOT use INPUT_PULLUP
    
    // Initialize feeding controller
    if (!feedingController.begin(&sensorManager, &nvsStorage)) {
        Serial.println("[ERROR] Failed to initialize feeding controller");
    }
    
    // Initialize communication manager
    if (!commManager.begin(&deviceManager, &nvsStorage)) {
        Serial.println("[ERROR] Failed to initialize communication manager");
    }

    // Wire MQTT command handler
    commManager.setCommandCallback([](CommandType type, const JsonDocument& doc) {
        if (type == CommandType::FEED_NOW) {
            float grams = doc["grams"] | MANUAL_FEED_GRAMS_DEFAULT;
            feedingController.feedRemote(grams);
        } else if (type == CommandType::STOP_FEEDING) {
            feedingController.stopFeeding();
        } else if (type == CommandType::RUN_DIAGNOSTICS) {
            // Check if this is a pong response
            if (doc["nonce"].is<uint32_t>()) {
                systemDiagnostics.handlePong(doc);
            } else {
                // On-demand diagnostics request from mobile app
                systemDiagnostics.runFullCheck();
                commManager.sendDiagnosticsReport(systemDiagnostics);
            }
        }
    });

    // Wire config callback - backend pushes full schedule on every create/update/delete
    commManager.setConfigCallback([](const JsonDocument& doc) {
        JsonArrayConst entries = doc["schedules"].as<JsonArrayConst>();
        if (entries.isNull()) return;

        int count = 0;
        ScheduleEntry newSchedule[SCHEDULE_MAX_ENTRIES];
        memset(newSchedule, 0, sizeof(newSchedule));

        for (JsonObjectConst entry : entries) {
            if (count >= SCHEDULE_MAX_ENTRIES) break;
            newSchedule[count].hour         = entry["hour"]           | 0;
            newSchedule[count].minute       = entry["minute"]         | 0;
            newSchedule[count].quantityGrams = entry["quantity_grams"] | MANUAL_FEED_GRAMS_DEFAULT;
            newSchedule[count].daysOfWeek   = entry["days_bitmask"]   | 0x7F; // all days default
            newSchedule[count].enabled      = entry["is_active"]      | true;
            count++;
        }

        feedingController.setSchedule(newSchedule, count);
        Serial.printf("[Config] Schedule updated: %d entries\n", count);
    });
    
    // Initialize system diagnostics (runs full hardware check)
    systemDiagnostics.begin(&sensorManager, &powerManager, &feedingController, &commManager);
    
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

#ifdef WOKWI_SIM
        if (now - lastSimHeartbeatTime >= 5000) {
            lastSimHeartbeatTime = now;
            SensorData data = sensorManager.getCurrentData();
            PowerStatus power = powerManager.getStatus();
            Serial.printf(
                "[SIM] uptime=%lus temp=%.2fC feed=%.1f%% battery=%.1f%% mqtt=%s buffered=%d\n",
                now / 1000,
                data.temperature,
                data.feedLevelPercent,
                power.batteryPercent,
                commManager.isConnected() ? "connected" : "disconnected",
                commManager.getOfflineBufferCount()
            );
        }
#endif
        
        // Process incoming commands
        commManager.processIncomingMessages();
        
        // Update system diagnostics (handles ping timeouts, periodic checks)
        systemDiagnostics.update();
        
        // Flush offline buffer if connected
        if (commManager.isConnected()) {
            commManager.flushOfflineBuffer();
            
            // Send diagnostics report periodically (piggyback on telemetry cycle)
            static unsigned long lastDiagReportTime = 0;
            if (now - lastDiagReportTime >= 300000UL) {  // Every 5 minutes
                lastDiagReportTime = now;
                commManager.sendDiagnosticsReport(systemDiagnostics);
                systemDiagnostics.sendPipelinePing();
            }
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
        
        // Manual feed button polling (200ms debounce)
        static bool lastBtnState = HIGH;
        static unsigned long lastTriggerMs = 0;
        bool currentBtnState = digitalRead(PIN_FEED_BTN);
        if (lastBtnState == HIGH && currentBtnState == LOW) {
            if (millis() - lastTriggerMs > 200) {
                Serial.println("[SW1] Manual feed button pressed");
                feedingController.feedNow(MANUAL_FEED_GRAMS_DEFAULT);
                lastTriggerMs = millis();
            }
        }
        lastBtnState = currentBtnState;

        // Update device manager (provisioning timeout + status LED)
        deviceManager.update();

        // Update feeding controller
        feedingController.update();

        // Update power manager
        powerManager.update();
        
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

/**
 * Check sensor readings and generate alerts if needed.
 * Rate-limited to once per 5 minutes to avoid alert spam.
 */
void checkSensorAlerts() {
    unsigned long now = millis();
    // Suppress duplicate alerts for 5 minutes
    if (now - lastAlertTime < 300000UL && lastAlertTime != 0) {
        return;
    }

    SensorData data = sensorManager.getCurrentData();
    bool alertSent = false;

    // Temperature: lethal range (>= CLARIAS_LETHAL_MAX or < CLARIAS_TEMP_MIN)
    if (data.temperatureValid) {
        if (data.temperature >= CLARIAS_LETHAL_MAX) {
            commManager.sendAlert(
                AlertType::HIGH_TEMPERATURE,
                AlertSeverity::SEVERITY_CRITICAL,
                "LETHAL temperature: " + String(data.temperature, 1) + "C - stop feeding immediately"
            );
            alertSent = true;
        } else if (data.temperature > CLARIAS_CRITICAL_MAX) {
            commManager.sendAlert(
                AlertType::HIGH_TEMPERATURE,
                AlertSeverity::SEVERITY_HIGH,
                "High temperature stress: " + String(data.temperature, 1) + "C (optimal max " + String(CLARIAS_OPTIMAL_MAX, 0) + "C)"
            );
            alertSent = true;
        } else if (data.temperature < CLARIAS_TEMP_MIN) {
            commManager.sendAlert(
                AlertType::LOW_TEMPERATURE,
                AlertSeverity::SEVERITY_HIGH,
                "Low temperature: " + String(data.temperature, 1) + "C (min viable " + String(CLARIAS_TEMP_MIN, 0) + "C)"
            );
            alertSent = true;
        }
    }

    // Feed level
    if (data.feedLevelValid && data.feedLevelPercent < FEED_LEVEL_LOW_THRESHOLD) {
        commManager.sendAlert(
            AlertType::LOW_FEED,
            AlertSeverity::SEVERITY_MEDIUM,
            "Feed level low: " + String(data.feedLevelPercent, 1) + "%"
        );
        alertSent = true;
    }

    // Battery
    PowerStatus power = powerManager.getStatus();
    if (power.batteryPercent < BATTERY_CRITICAL) {
        commManager.sendAlert(
            AlertType::LOW_BATTERY,
            AlertSeverity::SEVERITY_CRITICAL,
            "Battery critical: " + String(power.batteryPercent, 1) + "%"
        );
        alertSent = true;
    } else if (power.batteryPercent < BATTERY_LOW_THRESHOLD) {
        commManager.sendAlert(
            AlertType::LOW_BATTERY,
            AlertSeverity::SEVERITY_HIGH,
            "Battery low: " + String(power.batteryPercent, 1) + "%"
        );
        alertSent = true;
    }

    if (alertSent) {
        lastAlertTime = now;
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
