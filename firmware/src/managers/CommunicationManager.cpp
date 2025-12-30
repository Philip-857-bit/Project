/**
 * @file CommunicationManager.cpp
 * @brief GSM-primary/WiFi-secondary communication implementation
 * Uses TinyGSM library for A7670 on LILYGO T-A7670 R2
 */

#include "CommunicationManager.h"
#include "../../include/config.h"

// TinyGSM configuration - must be before include
#define TINY_GSM_MODEM_A7670
#define TINY_GSM_RX_BUFFER 1024

#include <TinyGsmClient.h>

// Static instance for callback
CommunicationManager* CommunicationManager::_instance = nullptr;

// GSM Serial - use hardware serial for A7670
HardwareSerial GSMSerial(1);

// TinyGSM modem instance
TinyGsm* modem = nullptr;
TinyGsmClient* gsmClient = nullptr;

CommunicationManager::CommunicationManager()
    : _deviceManager(nullptr)
    , _storage(nullptr)
    , _mqttClient(_wifiClient)
    , _state(ConnectionState::DISCONNECTED)
    , _useGSM(false)
    , _wifiAvailable(false)
    , _gsmAvailable(false)
    , _lastConnectAttempt(0)
    , _lastReconnectAttempt(0)
    , _connectRetries(0)
    , _offlineBuffer(nullptr)
    , _offlineBufferHead(0)
    , _offlineBufferTail(0)
    , _offlineBufferCount(0)
    , _commandCallback(nullptr)
    , _gsmSerial(nullptr)
    , _cellularSignal(0) {
    
    _instance = this;
}

CommunicationManager::~CommunicationManager() {
    if (_offlineBuffer != nullptr) {
        // Free buffered payloads
        for (int i = 0; i < OFFLINE_BUFFER_SIZE; i++) {
            if (_offlineBuffer[i].payload != nullptr) {
                free(_offlineBuffer[i].payload);
            }
        }
        delete[] _offlineBuffer;
    }
    _instance = nullptr;
}

bool CommunicationManager::begin(DeviceManager* deviceManager, NVSStorage* storage) {
    _deviceManager = deviceManager;
    _storage = storage;
    
    // Allocate offline buffer
    _offlineBuffer = new OfflineMessage[OFFLINE_BUFFER_SIZE];
    memset(_offlineBuffer, 0, sizeof(OfflineMessage) * OFFLINE_BUFFER_SIZE);
    
    // Build topic strings
    String deviceID = _deviceManager->getDeviceID();
    _topicTelemetry = buildTopic("telemetry");
    _topicFeeding = buildTopic("feeding");
    _topicAlerts = buildTopic("alerts");
    _topicCommands = buildTopic("commands");
    _topicConfig = buildTopic("config");
    _topicDiagnostics = buildTopic("diagnostics");
    
    // Configure MQTT client
    _mqttClient.setBufferSize(MQTT_BUFFER_SIZE);
    _mqttClient.setKeepAlive(MQTT_KEEPALIVE);
    _mqttClient.setCallback(mqttCallback);
    
    // Initialize GSM module
    _gsmAvailable = initGSM();
    
    // Check WiFi credentials
    _wifiAvailable = !_deviceManager->getWiFiSSID().isEmpty();
    
    Serial.printf("[CommManager] WiFi available: %s, GSM available: %s\n",
                  _wifiAvailable ? "Yes" : "No",
                  _gsmAvailable ? "Yes" : "No");
    
    return _wifiAvailable || _gsmAvailable;
}

bool CommunicationManager::initGSM() {
#ifdef LILYGO_T_A7670
    // Initialize serial for A7670
    GSMSerial.begin(MODEM_BAUD_RATE, SERIAL_8N1, MODEM_RX, MODEM_TX);
    _gsmSerial = &GSMSerial;
    
    // Configure power pins
    pinMode(MODEM_PWRKEY, OUTPUT);
    pinMode(MODEM_EN, OUTPUT);
    pinMode(MODEM_DTR, OUTPUT);
    
    // Enable modem
    digitalWrite(MODEM_EN, HIGH);
    digitalWrite(MODEM_DTR, LOW);
    
    // Power on sequence for A7670
    Serial.println("[CommManager] Powering on A7670...");
    digitalWrite(MODEM_PWRKEY, LOW);
    delay(100);
    digitalWrite(MODEM_PWRKEY, HIGH);
    delay(1000);
    digitalWrite(MODEM_PWRKEY, LOW);
    
    delay(5000);  // Wait for module to boot
    
    // Initialize TinyGSM modem
    modem = new TinyGsm(GSMSerial);
    
    Serial.println("[CommManager] Initializing modem...");
    if (!modem->restart()) {
        Serial.println("[CommManager] Modem restart failed, trying init...");
        if (!modem->init()) {
            Serial.println("[CommManager] Modem init failed");
            return false;
        }
    }
    
    // Get modem info
    String modemInfo = modem->getModemInfo();
    Serial.printf("[CommManager] Modem: %s\n", modemInfo.c_str());
    
    // Wait for network
    Serial.println("[CommManager] Waiting for network...");
    if (!modem->waitForNetwork(60000)) {
        Serial.println("[CommManager] Network not available");
        return false;
    }
    
    Serial.println("[CommManager] Network connected");
    
    // Create GSM client for MQTT
    gsmClient = new TinyGsmClient(*modem);
    
    return true;
#else
    return false;
#endif
}

String CommunicationManager::sendATCommand(const String& command, unsigned long timeout) {
    _gsmSerial->println(command);
    
    String response = "";
    unsigned long start = millis();
    
    while (millis() - start < timeout) {
        while (_gsmSerial->available()) {
            char c = _gsmSerial->read();
            response += c;
        }
        if (response.indexOf("OK") != -1 || response.indexOf("ERROR") != -1) {
            break;
        }
        delay(10);
    }
    
    return response;
}

void CommunicationManager::loop() {
    // Handle reconnection
    if (_state != ConnectionState::CONNECTED) {
        unsigned long now = millis();
        
        if (now - _lastReconnectAttempt > MQTT_RECONNECT_DELAY_MS) {
            _lastReconnectAttempt = now;
            
            // Try WiFi first if available
            if (_wifiAvailable && !_useGSM) {
                if (connectWiFi()) {
                    if (connectMQTT()) {
                        _state = ConnectionState::CONNECTED;
                        subscribeTopics();
                        return;
                    }
                }
                _connectRetries++;
                
                // Switch to GSM after retries
                if (_connectRetries >= WIFI_MAX_RETRIES && _gsmAvailable) {
                    Serial.println("[CommManager] Switching to GSM");
                    _useGSM = true;
                    _connectRetries = 0;
                }
            }
            
            // Try GSM
            if (_gsmAvailable && _useGSM) {
                if (connectGSM()) {
                    if (connectMQTT()) {
                        _state = ConnectionState::CONNECTED;
                        subscribeTopics();
                        return;
                    }
                }
            }
        }
    } else {
        // Maintain connection
        if (!_mqttClient.connected()) {
            _state = ConnectionState::DISCONNECTED;
            Serial.println("[CommManager] MQTT disconnected");
        } else {
            _mqttClient.loop();
        }
    }
}

bool CommunicationManager::connectWiFi() {
    _state = ConnectionState::CONNECTING_WIFI;
    
    String ssid = _deviceManager->getWiFiSSID();
    String password = _deviceManager->getWiFiPassword();
    
    Serial.printf("[CommManager] Connecting to WiFi: %s\n", ssid.c_str());
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());
    
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_CONNECT_TIMEOUT_MS) {
        delay(500);
        Serial.print(".");
    }
    Serial.println();
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("[CommManager] WiFi connected, IP: %s\n", WiFi.localIP().toString().c_str());
        return true;
    }
    
    Serial.println("[CommManager] WiFi connection failed");
    return false;
}

bool CommunicationManager::connectGSM() {
    _state = ConnectionState::CONNECTING_GSM;
    
    Serial.println("[CommManager] Connecting via GSM...");
    
#ifdef LILYGO_T_A7670
    if (!modem || !modem->isNetworkConnected()) {
        Serial.println("[CommManager] Network not connected");
        return false;
    }
    
    // Get signal quality
    _cellularSignal = modem->getSignalQuality();
    Serial.printf("[CommManager] Signal strength: %d\n", _cellularSignal);
    
    // Connect GPRS
    String apn = _deviceManager->getCellularAPN();
    Serial.printf("[CommManager] Connecting to APN: %s\n", apn.c_str());
    
    if (!modem->gprsConnect(apn.c_str(), "", "")) {
        Serial.println("[CommManager] GPRS connection failed");
        return false;
    }
    
    Serial.println("[CommManager] GPRS connected");
    
    // Update MQTT client to use GSM
    _mqttClient.setClient(*gsmClient);
    
    return true;
#else
    return false;
#endif
}

bool CommunicationManager::connectMQTT() {
    _state = ConnectionState::CONNECTING_MQTT;
    
    String host = _deviceManager->getMQTTHost();
    String username = _deviceManager->getMQTTUsername();
    String password = _deviceManager->getMQTTPassword();
    String clientID = _deviceManager->getDeviceID();
    
    Serial.printf("[CommManager] Connecting to MQTT: %s\n", host.c_str());
    
    _mqttClient.setServer(host.c_str(), MQTT_PORT);
    
    bool connected;
    if (username.isEmpty()) {
        connected = _mqttClient.connect(clientID.c_str());
    } else {
        connected = _mqttClient.connect(clientID.c_str(), username.c_str(), password.c_str());
    }
    
    if (connected) {
        Serial.println("[CommManager] MQTT connected");
        return true;
    }
    
    Serial.printf("[CommManager] MQTT connection failed, state: %d\n", _mqttClient.state());
    return false;
}

void CommunicationManager::subscribeTopics() {
    _mqttClient.subscribe(_topicCommands.c_str(), MQTT_QOS);
    _mqttClient.subscribe(_topicConfig.c_str(), MQTT_QOS);
    
    Serial.println("[CommManager] Subscribed to topics");
}

void CommunicationManager::mqttCallback(char* topic, byte* payload, unsigned int length) {
    if (_instance != nullptr) {
        _instance->handleMessage(topic, payload, length);
    }
}

void CommunicationManager::handleMessage(const char* topic, uint8_t* payload, unsigned int length) {
    Serial.printf("[CommManager] Message on %s (%d bytes)\n", topic, length);
    
    // Parse JSON payload
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload, length);
    
    if (error) {
        Serial.printf("[CommManager] JSON parse error: %s\n", error.c_str());
        return;
    }
    
    // Handle commands
    if (String(topic) == _topicCommands) {
        int cmdType = doc["type"] | 0;
        
        if (_commandCallback != nullptr) {
            _commandCallback((CommandType)cmdType, doc);
        }
    }
}

bool CommunicationManager::sendTelemetry(const SensorData& sensorData, const PowerStatus& powerStatus) {
    JsonDocument doc;
    
    doc["device_id"] = _deviceManager->getDeviceID();
    doc["timestamp"] = millis();
    doc["temperature"] = sensorData.temperature;
    doc["dissolved_oxygen"] = sensorData.dissolvedOxygen;
    doc["ph"] = sensorData.pH;
    doc["turbidity"] = sensorData.turbidity;
    doc["weight_grams"] = sensorData.feedWeightGrams;
    doc["weight_percentage"] = sensorData.feedLevelPercent;
    doc["battery_level"] = (int)powerStatus.batteryPercent;
    doc["battery_voltage"] = powerStatus.batteryVoltage;
    doc["power_source"] = (int)powerStatus.source;
    doc["solar_voltage"] = powerStatus.solarVoltage;
    doc["cellular_signal"] = _cellularSignal;
    doc["wifi_rssi"] = WiFi.RSSI();
    doc["status"] = 1;  // Online
    
    String json;
    serializeJson(doc, json);
    
    return publish(_topicTelemetry, (uint8_t*)json.c_str(), json.length(), 2);
}

bool CommunicationManager::sendFeedingEvent(const FeedingEvent& event) {
    JsonDocument doc;
    
    doc["device_id"] = _deviceManager->getDeviceID();
    doc["timestamp"] = event.timestamp;
    doc["quantity_grams"] = event.quantityGrams;
    doc["actual_dispensed"] = event.actualDispensed;
    doc["duration_seconds"] = event.durationMs / 1000;
    doc["trigger"] = (int)event.trigger;
    doc["result"] = (int)event.result;
    doc["error_message"] = event.errorMessage;
    doc["temperature"] = event.temperature;
    doc["dissolved_oxygen"] = event.dissolvedOxygen;
    doc["q10_factor"] = event.q10Factor;
    doc["obm_safety_factor"] = event.obmSafetyFactor;
    
    String json;
    serializeJson(doc, json);
    
    return publish(_topicFeeding, (uint8_t*)json.c_str(), json.length(), 4);
}

bool CommunicationManager::sendAlert(AlertType type, AlertSeverity severity, const String& message) {
    JsonDocument doc;
    
    doc["device_id"] = _deviceManager->getDeviceID();
    doc["timestamp"] = millis();
    doc["severity"] = (int)severity;
    doc["type"] = (int)type;
    doc["message"] = message;
    
    String json;
    serializeJson(doc, json);
    
    uint8_t priority = (severity == AlertSeverity::SEVERITY_CRITICAL) ? 5 : 4;
    return publish(_topicAlerts, (uint8_t*)json.c_str(), json.length(), priority);
}

bool CommunicationManager::sendDiagnostics() {
    JsonDocument doc;
    
    doc["device_id"] = _deviceManager->getDeviceID();
    doc["timestamp"] = millis();
    doc["firmware_version"] = FIRMWARE_VERSION;
    doc["uptime_seconds"] = millis() / 1000;
    doc["free_heap_bytes"] = ESP.getFreeHeap();
    doc["gsm_connected"] = _gsmAvailable && _useGSM;
    doc["wifi_connected"] = WiFi.status() == WL_CONNECTED;
    doc["mqtt_connected"] = _mqttClient.connected();
    doc["gsm_signal_strength"] = _cellularSignal;
    doc["wifi_signal_strength"] = WiFi.RSSI();
    
    String json;
    serializeJson(doc, json);
    
    return publish(_topicDiagnostics, (uint8_t*)json.c_str(), json.length(), 2);
}

bool CommunicationManager::publish(const String& topic, uint8_t* payload, size_t length, uint8_t priority) {
    if (_mqttClient.connected()) {
        if (_mqttClient.publish(topic.c_str(), payload, length)) {
            return true;
        }
    }
    
    // Buffer for offline sending
    return bufferMessage(topic, payload, length, priority);
}

bool CommunicationManager::bufferMessage(const String& topic, uint8_t* payload, size_t length, uint8_t priority) {
    if (_offlineBufferCount >= OFFLINE_BUFFER_SIZE) {
        // Buffer full - remove lowest priority message
        int lowestPriority = 6;
        int lowestIndex = -1;
        
        for (int i = 0; i < OFFLINE_BUFFER_SIZE; i++) {
            if (_offlineBuffer[i].priority < lowestPriority) {
                lowestPriority = _offlineBuffer[i].priority;
                lowestIndex = i;
            }
        }
        
        if (lowestIndex >= 0 && priority > lowestPriority) {
            free(_offlineBuffer[lowestIndex].payload);
            _offlineBuffer[lowestIndex].payload = nullptr;
            _offlineBufferCount--;
        } else {
            return false;  // Can't buffer
        }
    }
    
    // Find empty slot
    for (int i = 0; i < OFFLINE_BUFFER_SIZE; i++) {
        if (_offlineBuffer[i].payload == nullptr) {
            _offlineBuffer[i].topic = topic;
            _offlineBuffer[i].payload = (uint8_t*)malloc(length);
            memcpy(_offlineBuffer[i].payload, payload, length);
            _offlineBuffer[i].length = length;
            _offlineBuffer[i].timestamp = millis();
            _offlineBuffer[i].priority = priority;
            _offlineBufferCount++;
            return true;
        }
    }
    
    return false;
}

int CommunicationManager::flushOfflineBuffer() {
    if (!_mqttClient.connected() || _offlineBufferCount == 0) {
        return 0;
    }
    
    int sent = 0;
    
    // Send highest priority first
    for (int p = 5; p >= 1; p--) {
        for (int i = 0; i < OFFLINE_BUFFER_SIZE; i++) {
            if (_offlineBuffer[i].payload != nullptr && _offlineBuffer[i].priority == p) {
                if (_mqttClient.publish(_offlineBuffer[i].topic.c_str(), 
                                        _offlineBuffer[i].payload, 
                                        _offlineBuffer[i].length)) {
                    free(_offlineBuffer[i].payload);
                    _offlineBuffer[i].payload = nullptr;
                    _offlineBufferCount--;
                    sent++;
                }
            }
        }
    }
    
    if (sent > 0) {
        Serial.printf("[CommManager] Flushed %d offline messages\n", sent);
    }
    
    return sent;
}

void CommunicationManager::processIncomingMessages() {
    _mqttClient.loop();
}

void CommunicationManager::disconnect() {
    _mqttClient.disconnect();
    WiFi.disconnect();
    _state = ConnectionState::DISCONNECTED;
}

String CommunicationManager::buildTopic(const char* suffix) {
    return "devices/" + _deviceManager->getDeviceID() + "/" + suffix;
}

// Getters
bool CommunicationManager::isConnected() const { return _state == ConnectionState::CONNECTED; }
ConnectionState CommunicationManager::getState() const { return _state; }
int CommunicationManager::getWiFiRSSI() const { return WiFi.RSSI(); }
int CommunicationManager::getCellularSignal() const { return _cellularSignal; }
int CommunicationManager::getOfflineBufferCount() const { return _offlineBufferCount; }
void CommunicationManager::setCommandCallback(CommandCallback callback) { _commandCallback = callback; }
