/**
 * @file config.h
 * @brief Configuration for Smart Fish Feeder ESP32 firmware
 * 
 * Hardware Configuration:
 * - Main Board: LILYGO T-A7670 R2 (ESP32-WROVER-B + A7670G 4G LTE Cat1)
 * - Camera: ESP32-CAM (AI-Thinker, OV2640)
 * - Motor: NEMA 23 Stepper + DM542 or TB6600 Driver
 * - Auger: 20mm Wood Drill Auger Bit
 * - Feed Level: JSN-SR04T Ultrasonic + HX711 Load Cell (20kg, dual sensing)
 * - Temperature: DS18B20 Waterproof Probe
 * - Power: Solar Panel + 18650 Battery (board has built-in charging)
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// =============================================================================
// Firmware Version
// =============================================================================
#define FIRMWARE_VERSION "1.1.0"
#define FIRMWARE_BUILD_DATE __DATE__
#define FIRMWARE_BUILD_TIME __TIME__

// =============================================================================
// Board Detection
// =============================================================================
#if defined(LILYGO_T_A7670)
    #define BOARD_NAME "LILYGO T-A7670 R2"
    #define HAS_GSM_MODULE 1
    #define HAS_SD_CARD 1
    #define HAS_GPS 1           // A7670G variant only
    #define HAS_BATTERY 1       // 18650 holder built-in
#elif defined(ESP32_CAM)
    #define BOARD_NAME "ESP32-CAM"
    #define HAS_CAMERA 1
#else
    #define BOARD_NAME "Generic ESP32"
#endif

// =============================================================================
// LILYGO T-A7670 R2 Pin Definitions (ESP32-WROVER-B + A7670G)
// =============================================================================
#ifdef LILYGO_T_A7670

// A7670G 4G LTE Module (built-in on board)
#define MODEM_TX            26      // ESP32 TX -> A7670 RX
#define MODEM_RX            27      // ESP32 RX <- A7670 TX
#define MODEM_PWRKEY        4       // Power key (LOW pulse to toggle)
#define MODEM_EN            12      // Enable pin
#define MODEM_DTR           25      // Data Terminal Ready
#define MODEM_RI            33      // Ring Indicator

// GPS (A7670G variant only - shares UART with modem via AT commands)
#define GPS_TX              21      // GPS TX (IO21)
#define GPS_RX              22      // GPS RX (IO22)
#define GPS_PPS             19      // GPS PPS (IO19)
#define GPS_WAKE            23      // GPS Wake (IO23)

// SD Card (SPI interface)
#define SD_MISO             2       // SPI MISO
#define SD_MOSI             15      // SPI MOSI
#define SD_SCLK             14      // SPI Clock
#define SD_CS               13      // SPI Chip Select

// I2C Bus (for sensors)
#define PIN_I2C_SDA         21      // Wire_SDA
#define PIN_I2C_SCL         22      // Wire_SCL

// Battery ADC (built-in 18650 monitoring)
#define PIN_BATTERY_ADC     35      // ADC1_CH7 - Battery voltage

// VBUS Detection
#define PIN_VBUS            36      // USB power detection (VP)

// =============================================================================
// DM542 / TB6600 Stepper Motor Driver (Step/Dir/Enable mode)
// For NEMA 23 stepper motor with 20mm wood drill auger
// Available GPIOs on T-A7670: 0, 32, 33, 34, 35, 39 (input only: 34, 35, 39)
// =============================================================================
#ifdef USE_DM542
#define PIN_STEP            GPIO_NUM_32     // PUL+ (Step pulse)
#define PIN_DIR             GPIO_NUM_33     // DIR+ (Direction)
#define PIN_ENABLE          GPIO_NUM_0      // ENA+ (Enable, active LOW)
// Note: PUL-, DIR-, ENA- connect to GND
// DM542 requires 5V logic - use level shifter or optocoupler
#endif

#ifdef USE_TB6600
#define PIN_STEP            GPIO_NUM_32     // PUL+ (Step pulse)
#define PIN_DIR             GPIO_NUM_33     // DIR+ (Direction)
#define PIN_ENABLE          GPIO_NUM_0      // ENA+ (Enable, active LOW)
// Note: PUL-, DIR-, ENA- connect to GND
// TB6600 can work with 3.3V logic directly
#endif

// Legacy TMC2209 support (for smaller motors)
#ifdef USE_TMC2209
#define PIN_STEP            GPIO_NUM_32     // Step pulse
#define PIN_DIR             GPIO_NUM_33     // Direction
#define PIN_ENABLE          GPIO_NUM_0      // Enable (active LOW)
#define PIN_TMC_TX          GPIO_NUM_17     // TMC2209 UART TX (shared)
#define PIN_TMC_RX          GPIO_NUM_16     // TMC2209 UART RX (shared)
#define PIN_DIAG            GPIO_NUM_34     // StallGuard diagnostic (input only)
#endif

// =============================================================================
// A4988 Stepper Motor Driver (Step/Dir mode - fallback)
// =============================================================================
#ifdef USE_A4988
#ifndef PIN_STEP
#define PIN_STEP            GPIO_NUM_32     // Step pulse
#endif
#ifndef PIN_DIR
#define PIN_DIR             GPIO_NUM_33     // Direction
#endif
#ifndef PIN_ENABLE
#define PIN_ENABLE          GPIO_NUM_0      // Enable (active LOW)
#endif
#define PIN_MS1             GPIO_NUM_17     // Microstepping select 1
#define PIN_MS2             GPIO_NUM_16     // Microstepping select 2
#define PIN_MS3             GPIO_NUM_18     // Microstepping select 3
#endif

// =============================================================================
// HX711 Load Cell Amplifier (for precise weight measurement)
// =============================================================================
#define PIN_HX711_DOUT      GPIO_NUM_39     // Data out (VN - input only)
#define PIN_HX711_SCK       GPIO_NUM_5      // Clock

// =============================================================================
// JSN-SR04T Ultrasonic Sensor (Waterproof) - Backup feed level
// =============================================================================
#define PIN_ULTRASONIC_TRIG GPIO_NUM_17     // Trigger pin
#define PIN_ULTRASONIC_ECHO GPIO_NUM_34     // Echo pin (input only)

// Note: If using TMC2209, GPIO17 is TMC_TX - use different pin
#ifdef USE_TMC2209
#undef PIN_ULTRASONIC_TRIG
#define PIN_ULTRASONIC_TRIG GPIO_NUM_5      // Alternative trigger pin
#undef PIN_HX711_SCK
#define PIN_HX711_SCK       GPIO_NUM_18     // Move HX711 clock
#endif

// =============================================================================
// DS18B20 Temperature Sensor (OneWire)
// Note: I2C pins 21/22 are used by GPS, use different pin for OneWire
// =============================================================================
#define PIN_ONEWIRE         GPIO_NUM_23     // OneWire data (with 4.7k pullup)

// =============================================================================
// Power Monitoring (18650 battery via built-in divider)
// =============================================================================
// PIN_BATTERY_ADC already defined above as GPIO35
#define PIN_SOLAR_ADC       GPIO_NUM_36     // Solar panel voltage (VP, with external divider)

// =============================================================================
// Status LEDs
// =============================================================================
#define PIN_LED_STATUS      GPIO_NUM_2      // Built-in LED (directly on GPIO2)

// =============================================================================
// Communication with ESP32-CAM (Serial1)
// =============================================================================
#define PIN_CAM_TX          GPIO_NUM_17     // TX to CAM RX
#define PIN_CAM_RX          GPIO_NUM_16     // RX from CAM TX

#endif // LILYGO_T_A7670

// =============================================================================
// ESP32-CAM Pin Definitions (AI-Thinker with OV2640)
// =============================================================================
#ifdef ESP32_CAM

// Camera pins are fixed for AI-Thinker module
#define PWDN_GPIO_NUM       32
#define RESET_GPIO_NUM      -1
#define XCLK_GPIO_NUM       0
#define SIOD_GPIO_NUM       26
#define SIOC_GPIO_NUM       27
#define Y9_GPIO_NUM         35
#define Y8_GPIO_NUM         34
#define Y7_GPIO_NUM         39
#define Y6_GPIO_NUM         36
#define Y5_GPIO_NUM         21
#define Y4_GPIO_NUM         19
#define Y3_GPIO_NUM         18
#define Y2_GPIO_NUM         5
#define VSYNC_GPIO_NUM      25
#define HREF_GPIO_NUM       23
#define PCLK_GPIO_NUM       22

// Flash LED
#define PIN_FLASH_LED       GPIO_NUM_4

// Communication with main board
#define PIN_MAIN_TX         GPIO_NUM_1
#define PIN_MAIN_RX         GPIO_NUM_3

#endif // ESP32_CAM

// =============================================================================
// Motor Configuration (NEMA 23 + DM542/TB6600 + 20mm Wood Drill Auger)
// =============================================================================
#define MOTOR_STEPS_PER_REV     200         // 1.8° per step (NEMA 23)
#define MOTOR_MICROSTEPS        8           // DM542/TB6600 microstepping (set via DIP switches)
#define MOTOR_MAX_SPEED         800         // Steps per second (lower for NEMA 23 torque)
#define MOTOR_ACCELERATION      400         // Steps per second²
#define MOTOR_CURRENT_MA        2800        // Motor current in mA (set on DM542/TB6600)
#define MOTOR_PULSE_WIDTH_US    5           // Minimum pulse width for DM542/TB6600

// 20mm Wood Drill Auger Calibration
// Auger pitch ~20mm, so one revolution moves ~20mm of feed
// Approximate volume per revolution depends on feed density
#define AUGER_DIAMETER_MM       20.0f       // Auger bit diameter
#define AUGER_PITCH_MM          20.0f       // Auger pitch (distance per revolution)
#define GRAMS_PER_REVOLUTION    25.0f       // Grams dispensed per motor revolution (calibrate!)
#define MIN_FEED_GRAMS          10.0f       // Minimum feed amount
#define MAX_FEED_GRAMS          2000.0f     // Maximum feed amount per session

// =============================================================================
// HX711 Load Cell Configuration (20kg Load Cell)
// =============================================================================
#define LOADCELL_SCALE_FACTOR   420.0f      // Calibration factor (adjust for 20kg cell!)
#define LOADCELL_OFFSET         0           // Tare offset
#define LOADCELL_SAMPLES        10          // Averaging samples
#define LOADCELL_MAX_KG         20.0f       // Maximum load cell capacity
#define HOPPER_CAPACITY_GRAMS   15000.0f    // Feed hopper capacity in grams (15kg)

// =============================================================================
// JSN-SR04T Ultrasonic Sensor Configuration
// =============================================================================
#define ULTRASONIC_MAX_DISTANCE 400         // Max distance in cm
#define ULTRASONIC_MIN_DISTANCE 25          // Min distance in cm (sensor limit)
#define HOPPER_HEIGHT_CM        50.0f       // Height from sensor to empty hopper
#define HOPPER_FULL_DISTANCE_CM 10.0f       // Distance when hopper is full

// =============================================================================
// DS18B20 Temperature Sensor Configuration
// =============================================================================
#define TEMP_RESOLUTION         12          // 12-bit resolution (0.0625°C)
#define TEMP_MIN_VALID          0.0f        // Minimum valid temperature
#define TEMP_MAX_VALID          50.0f       // Maximum valid temperature
#define TEMP_READ_DELAY_MS      750         // Conversion time for 12-bit

// =============================================================================
// Power Management (18650 Li-Ion Battery + Solar)
// T-A7670 R2 has built-in 18650 holder with charging circuit
// =============================================================================
#define BATTERY_FULL_VOLTAGE    4.2f        // 18650 Li-Ion full charge
#define BATTERY_EMPTY_VOLTAGE   3.0f        // 18650 Li-Ion empty (cutoff)
#define BATTERY_NOMINAL_VOLTAGE 3.7f        // Nominal voltage
#define BATTERY_LOW_THRESHOLD   20.0f       // Low battery percentage
#define BATTERY_CRITICAL        10.0f       // Critical battery percentage
#define SOLAR_MIN_VOLTAGE       5.0f        // Minimum solar charging voltage (5V panel)

// Voltage divider ratios (T-A7670 has built-in divider on GPIO35)
#define BATTERY_DIVIDER_RATIO   2.0f        // Built-in divider ratio
#define SOLAR_DIVIDER_RATIO     3.0f        // External divider for solar panel

// ADC Configuration
#define ADC_RESOLUTION          12
#define ADC_VREF                3.3f
#define ADC_MAX_VALUE           4095

// Deep Sleep Configuration
#define DEEP_SLEEP_DURATION_US  (30 * 60 * 1000000ULL)  // 30 minutes
#define WAKE_BEFORE_FEED_MS     (5 * 60 * 1000)         // 5 minutes before scheduled feed

// =============================================================================
// Communication Configuration
// =============================================================================

// WiFi
#define WIFI_CONNECT_TIMEOUT_MS 30000
#define WIFI_RECONNECT_DELAY_MS 5000
#define WIFI_MAX_RETRIES        3

// MQTT (undef first to avoid redefinition warning from PubSubClient)
#ifdef MQTT_KEEPALIVE
#undef MQTT_KEEPALIVE
#endif
#define MQTT_PORT               1883
#define MQTT_PORT_TLS           8883
#define MQTT_KEEPALIVE          60
#define MQTT_QOS                1
#define MQTT_BUFFER_SIZE        2048
#define MQTT_RECONNECT_DELAY_MS 5000

// Cellular (A7670G 4G LTE Cat1)
#define MODEM_BAUD_RATE         115200
#define MODEM_TIMEOUT_MS        30000
#define MODEM_APN               "internet"
#define MODEM_MODEL             "A7670"

// GPS Configuration (A7670G only)
#define GPS_BAUD_RATE           9600
#define GPS_UPDATE_INTERVAL_MS  10000

// Inter-board communication (ESP32-CAM)
#define INTERBOARD_BAUD         115200
#define INTERBOARD_TIMEOUT_MS   5000

// =============================================================================
// Timing Configuration
// =============================================================================
#define TELEMETRY_INTERVAL_MS   60000
#define SENSOR_READ_INTERVAL_MS 5000
#define WATCHDOG_TIMEOUT_MS     30000
#define FEEDING_TIMEOUT_MS      120000

// =============================================================================
// Biological Algorithm Parameters
// =============================================================================

// Q10 Temperature Coefficients
#define Q10_TILAPIA             2.2f
#define Q10_CATFISH             2.1f
#define Q10_CARP                2.3f
#define Q10_DEFAULT             2.0f
#define Q10_REFERENCE_TEMP      25.0f

// OBM Thresholds
#define DO_OPTIMAL_MG_L         6.0f
#define DO_LETHAL_MG_L          2.0f
#define DO_EMERGENCY_STOP_MG_L  3.0f

// Feeding Rate by Weight
#define FEED_RATE_FINGERLING    8.0f
#define FEED_RATE_JUVENILE      4.0f
#define FEED_RATE_ADULT         1.5f

// =============================================================================
// NVS Storage Keys
// =============================================================================
#define NVS_NAMESPACE           "fishfeeder"
#define NVS_KEY_DEVICE_ID       "device_id"
#define NVS_KEY_WIFI_SSID       "wifi_ssid"
#define NVS_KEY_WIFI_PASS       "wifi_pass"
#define NVS_KEY_MQTT_HOST       "mqtt_host"
#define NVS_KEY_MQTT_USER       "mqtt_user"
#define NVS_KEY_MQTT_PASS       "mqtt_pass"
#define NVS_KEY_CELL_APN        "cell_apn"
#define NVS_KEY_LOADCELL_CAL    "lc_cal"
#define NVS_KEY_HOPPER_CAL      "hopper_cal"
#define NVS_KEY_SCHEDULE        "schedule"

// =============================================================================
// Buffer Sizes
// =============================================================================
#define OFFLINE_BUFFER_SIZE     100
#define SCHEDULE_MAX_ENTRIES    10
#define ERROR_LOG_SIZE          50

// =============================================================================
// Camera Configuration (ESP32-CAM only)
// =============================================================================
#ifdef ESP32_CAM
#define CAMERA_FRAME_SIZE       FRAMESIZE_VGA
#define CAMERA_JPEG_QUALITY     12
#define CAMERA_FB_COUNT         2
#endif

#endif // CONFIG_H
