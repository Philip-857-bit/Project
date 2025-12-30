/**
 * @file CommunicationManager.h
 * @brief GSM-primary/WiFi-secondary communication with MQTT
 */

#ifndef COMMUNICATION_MANAGER_H
#define COMMUNICATION_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "DeviceManager.h"
#include "SensorManager.h"
#include "PowerManager.h"
#include "FeedingController.h"
#include "../storage/NVSStorage.h"

// Connection state
enum class ConnectionState {
    DISCONNECTED,
    CONNECTING_WIFI,
    CONNECTING_GSM,
    CONNECTING_MQTT,
    CONNECTED,
    ERROR
};

// Alert types (matching backend)
enum class AlertType {
    LOW_FEED = 1,
    LOW_BATTERY = 2,
    LOW_OXYGEN = 3,
    HIGH_TEMPERATURE = 4,
    LOW_TEMPERATURE = 5,
    PH_OUT_OF_RANGE = 6,
    FEEDER_JAMMED = 7,
    SENSOR_ERROR = 8,
    CONNECTIVITY_LOST = 9,
    POWER_FAILURE = 10,
    MAINTENANCE_REQ = 11
};

// Alert severity (matching backend)
enum class AlertSeverity {
    SEVERITY_INFO = 1,
    SEVERITY_LOW = 2,
    SEVERITY_MEDIUM = 3,
    SEVERITY_HIGH = 4,
    SEVERITY_CRITICAL = 5
};

// Command types (matching backend)
enum class CommandType {
    FEED_NOW = 1,
    STOP_FEEDING = 2,
    UPDATE_SCHEDULE = 3,
    UPDATE_CONFIG = 4,
    CALIBRATE_SENSOR = 5,
    REBOOT = 6,
    ENTER_SLEEP = 7,
    WAKE_UP = 8,
    RUN_DIAGNOSTICS = 9,
    CAPTURE_IMAGE = 10,
    ANTI_JAM = 11
};

// Offline message buffer entry
struct OfflineMessage {
    String topic;
    uint8_t* payload;
    size_t length;
    unsigned long timestamp;
    uint8_t priority;  // 1-5, 5 = critical
};

// Command callback type
typedef void (*CommandCallback)(CommandType type, const JsonDocument& payload);

class CommunicationManager {
public:
    CommunicationManager();
    ~CommunicationManager();
    
    /**
     * Initialize communication manager
     * @param deviceManager Device manager instance
     * @param storage NVS storage instance
     * @return true if successful
     */
    bool begin(DeviceManager* deviceManager, NVSStorage* storage);
    
    /**
     * Main loop - maintain connectivity
     */
    void loop();
    
    /**
     * Check if connected to MQTT
     * @return true if connected
     */
    bool isConnected() const;
    
    /**
     * Get connection state
     * @return ConnectionState enum
     */
    ConnectionState getState() const;
    
    /**
     * Disconnect from network
     */
    void disconnect();
    
    /**
     * Send telemetry data
     * @param sensorData Sensor readings
     * @param powerStatus Power status
     * @return true if sent or buffered
     */
    bool sendTelemetry(const SensorData& sensorData, const PowerStatus& powerStatus);
    
    /**
     * Send feeding event
     * @param event Feeding event data
     * @return true if sent or buffered
     */
    bool sendFeedingEvent(const FeedingEvent& event);
    
    /**
     * Send alert
     * @param type Alert type
     * @param severity Alert severity
     * @param message Alert message
     * @return true if sent or buffered
     */
    bool sendAlert(AlertType type, AlertSeverity severity, const String& message);
    
    /**
     * Send diagnostics report
     * @return true if sent or buffered
     */
    bool sendDiagnostics();
    
    /**
     * Process incoming messages
     */
    void processIncomingMessages();
    
    /**
     * Flush offline buffer
     * @return Number of messages sent
     */
    int flushOfflineBuffer();
    
    /**
     * Set command callback
     * @param callback Function to call on command receipt
     */
    void setCommandCallback(CommandCallback callback);
    
    /**
     * Get WiFi signal strength
     * @return RSSI in dBm
     */
    int getWiFiRSSI() const;
    
    /**
     * Get cellular signal strength
     * @return CSQ value (0-31)
     */
    int getCellularSignal() const;
    
    /**
     * Get offline buffer count
     * @return Number of buffered messages
     */
    int getOfflineBufferCount() const;

private:
    DeviceManager* _deviceManager;
    NVSStorage* _storage;
    
    WiFiClient _wifiClient;
    PubSubClient _mqttClient;
    
    ConnectionState _state;
    bool _useGSM;
    bool _wifiAvailable;
    bool _gsmAvailable;
    
    unsigned long _lastConnectAttempt;
    unsigned long _lastReconnectAttempt;
    int _connectRetries;
    
    // Offline buffer
    OfflineMessage* _offlineBuffer;
    int _offlineBufferHead;
    int _offlineBufferTail;
    int _offlineBufferCount;
    
    // Topics
    String _topicTelemetry;
    String _topicFeeding;
    String _topicAlerts;
    String _topicCommands;
    String _topicConfig;
    String _topicDiagnostics;
    
    CommandCallback _commandCallback;
    
    // GSM module
    HardwareSerial* _gsmSerial;
    int _cellularSignal;
    
    /**
     * Connect to WiFi
     * @return true if connected
     */
    bool connectWiFi();
    
    /**
     * Connect to GSM
     * @return true if connected
     */
    bool connectGSM();
    
    /**
     * Connect to MQTT broker
     * @return true if connected
     */
    bool connectMQTT();
    
    /**
     * Subscribe to device topics
     */
    void subscribeTopics();
    
    /**
     * MQTT message callback
     */
    static void mqttCallback(char* topic, byte* payload, unsigned int length);
    
    /**
     * Handle incoming command
     * @param topic MQTT topic
     * @param payload Message payload
     * @param length Payload length
     */
    void handleMessage(const char* topic, uint8_t* payload, unsigned int length);
    
    /**
     * Publish message (with offline buffering)
     * @param topic MQTT topic
     * @param payload Message payload
     * @param length Payload length
     * @param priority Message priority (1-5)
     * @return true if sent or buffered
     */
    bool publish(const String& topic, uint8_t* payload, size_t length, uint8_t priority = 3);
    
    /**
     * Add message to offline buffer
     * @param topic MQTT topic
     * @param payload Message payload
     * @param length Payload length
     * @param priority Message priority
     * @return true if buffered
     */
    bool bufferMessage(const String& topic, uint8_t* payload, size_t length, uint8_t priority);
    
    /**
     * Initialize GSM module
     * @return true if successful
     */
    bool initGSM();
    
    /**
     * Send AT command to GSM module
     * @param command AT command
     * @param timeout Timeout in ms
     * @return Response string
     */
    String sendATCommand(const String& command, unsigned long timeout = 1000);
    
    /**
     * Build topic string
     * @param suffix Topic suffix
     * @return Full topic string
     */
    String buildTopic(const char* suffix);
    
    // Singleton instance for static callback
    static CommunicationManager* _instance;
};

#endif // COMMUNICATION_MANAGER_H
