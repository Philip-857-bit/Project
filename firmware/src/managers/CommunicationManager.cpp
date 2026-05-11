/**
 * @file CommunicationManager.cpp
 * @brief GSM-primary/WiFi-secondary communication implementation
 * Uses TinyGSM library for A7670 on LILYGO T-A7670 R2
 */

#include "CommunicationManager.h"
#include "../../include/config.h"
#include <time.h>

// TinyGSM configuration - must be before include
#define TINY_GSM_RX_BUFFER 1024

// TinyGSM 0.11.x does not provide an A7670-specific profile.
// Use the SIM7600 profile, which is compatible with A7670 AT command set.
#if defined(TINY_GSM_MODEM_A7670) && !defined(TINY_GSM_MODEM_SIM7600)
#define TINY_GSM_MODEM_SIM7600
#endif

#include <TinyGsmClient.h>

static bool isDigitsOnly(const String& value) {
    if (value.isEmpty()) {
        return false;
    }
    for (size_t i = 0; i < value.length(); i++) {
        char c = value.charAt(i);
        if (c < '0' || c > '9') {
            return false;
        }
    }
    return true;
}

static void parseBrokerEndpoint(const String& rawEndpoint, String& hostOut, uint16_t& portOut, bool& useTLSOut) {
    String endpoint = rawEndpoint;
    endpoint.trim();

    bool explicitPort = false;
    bool useTLS = MQTT_USE_TLS != 0;
    uint16_t port = useTLS ? MQTT_PORT_TLS : MQTT_PORT;

    if (endpoint.startsWith("ssl://") || endpoint.startsWith("mqtts://")) {
        useTLS = true;
        endpoint = endpoint.substring(endpoint.indexOf("://") + 3);
    } else if (endpoint.startsWith("tcp://") || endpoint.startsWith("mqtt://")) {
        useTLS = false;
        endpoint = endpoint.substring(endpoint.indexOf("://") + 3);
    }

    int slashIndex = endpoint.indexOf('/');
    if (slashIndex >= 0) {
        endpoint = endpoint.substring(0, slashIndex);
    }

    int colonIndex = endpoint.lastIndexOf(':');
    if (colonIndex > 0 && endpoint.indexOf(']') == -1) {
        String portString = endpoint.substring(colonIndex + 1);
        portString.trim();

        if (isDigitsOnly(portString)) {
            long parsedPort = portString.toInt();
            if (parsedPort > 0 && parsedPort <= 65535) {
                port = (uint16_t)parsedPort;
                endpoint = endpoint.substring(0, colonIndex);
                explicitPort = true;
            }
        }
    }

    if (endpoint.endsWith(".hivemq.cloud") && !explicitPort) {
        useTLS = true;
        port = MQTT_PORT_TLS;
    } else if (useTLS && !explicitPort && port == MQTT_PORT) {
        port = MQTT_PORT_TLS;
    }

    endpoint.trim();
    hostOut = endpoint;
    portOut = port;
    useTLSOut = useTLS;
}

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
    , _configCallback(nullptr)
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
    _topicTelemetry   = buildTopic("telemetry");
    _topicFeeding     = buildTopic("feeding");
    _topicAlerts      = buildTopic("alerts");
    _topicCommands    = buildTopic("commands");
    _topicConfig      = buildTopic("config");
    _topicDiagnostics = buildTopic("diagnostics");
    _topicDiagPing    = buildTopic("diagnostics/ping");
    _topicDiagPong    = buildTopic("diagnostics/pong");
    _topicDiagReport  = buildTopic("diagnostics/report");
    
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
#ifdef MODEM_DTR
    pinMode(MODEM_DTR, OUTPUT);
#endif

    // Enable modem
    digitalWrite(MODEM_EN, HIGH);
#ifdef MODEM_DTR
    digitalWrite(MODEM_DTR, LOW);  // Keep modem awake
#endif
    
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
    
    String endpoint = _deviceManager->getMQTTHost();
    String username = _deviceManager->getMQTTUsername();
    String password = _deviceManager->getMQTTPassword();
    String clientID = _deviceManager->getDeviceID();

    String host;
    uint16_t port = MQTT_PORT;
    bool useTLS = false;
    parseBrokerEndpoint(endpoint, host, port, useTLS);

    if (host.isEmpty()) {
        Serial.println("[CommManager] MQTT host is empty");
        return false;
    }

    if (_useGSM) {
        if (useTLS) {
            Serial.println("[CommManager] TLS over GSM is not configured in this build, falling back to plaintext MQTT over GSM");
        }
        _mqttClient.setClient(*gsmClient);
    } else {
        if (useTLS) {
#if MQTT_SKIP_CERT_VERIFY
            _wifiSecureClient.setInsecure();
#endif
            _mqttClient.setClient(_wifiSecureClient);
        } else {
            _mqttClient.setClient(_wifiClient);
        }
    }
    
    Serial.printf("[CommManager] Connecting to MQTT: %s:%u (TLS=%s)\n",
                  host.c_str(),
                  (unsigned int)port,
                  useTLS ? "Yes" : "No");
    
    _mqttClient.setServer(host.c_str(), port);
    
    bool connected;
    if (username.isEmpty()) {
        connected = _mqttClient.connect(clientID.c_str());
    } else {
        connected = _mqttClient.connect(clientID.c_str(), username.c_str(), password.c_str());
    }
    
    if (connected) {
        Serial.println("[CommManager] MQTT connected");
        publishSelfRegistration();
        return true;
    }

    Serial.printf("[CommManager] MQTT connection failed, state: %d\n", _mqttClient.state());
    return false;
}

void CommunicationManager::publishSelfRegistration() {
    String bindCode = _storage->getString(NVS_KEY_BINDING_CODE);

    JsonDocument doc;
    doc["device_serial"]    = _deviceManager->getDeviceID();
    doc["firmware_version"] = FIRMWARE_VERSION;
    if (!bindCode.isEmpty()) {
        doc["binding_code"] = bindCode;
    }

    String json;
    serializeJson(doc, json);

    _mqttClient.publish("devices/register", json.c_str());
    Serial.println("[CommManager] Self-registration published");
}

void CommunicationManager::subscribeTopics() {
    _mqttClient.subscribe(_topicCommands.c_str(), MQTT_QOS);
    _mqttClient.subscribe(_topicConfig.c_str(), MQTT_QOS);
    _mqttClient.subscribe(_topicDiagPong.c_str(), MQTT_QOS);
    
    Serial.println("[CommManager] Subscribed to topics (commands, config, diagnostics/pong)");
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
        return;
    }

    // Handle config push (schedule updates from backend)
    if (String(topic) == _topicConfig) {
        if (_configCallback != nullptr) {
            _configCallback(doc);
        }
        return;
    }

    // Handle diagnostics pong (pipeline health)
    if (String(topic) == _topicDiagPong) {
        Serial.println("[CommManager] Received diagnostics pong");
        // Will be handled by SystemDiagnostics via main.cpp callback
        if (_commandCallback != nullptr) {
            // Re-use command callback with a special type
            _commandCallback(CommandType::RUN_DIAGNOSTICS, doc);
        }
    }
}

static const char* powerSourceString(uint8_t source) {
    switch (source) {
        case 1:  return "solar";
        case 2:  return "battery";
        case 3:  return "electric";
        default: return "battery";  // UNKNOWN falls back to battery
    }
}

bool CommunicationManager::sendTelemetry(const SensorData& sensorData, const PowerStatus& powerStatus) {
    JsonDocument doc;

    doc["device_id"]         = _deviceManager->getDeviceID();
    doc["timestamp"]         = (int64_t)time(nullptr);
    doc["water_temperature"] = sensorData.temperature;
    doc["weight_grams"]      = sensorData.feedWeightGrams;
    doc["weight_percentage"] = sensorData.feedLevelPercent;
    doc["battery_level"]     = (int)powerStatus.batteryPercent;
    doc["battery_voltage"]   = powerStatus.batteryVoltage;
    doc["power_source"]      = powerSourceString(static_cast<uint8_t>(powerStatus.source));
    doc["solar_voltage"]     = powerStatus.solarVoltage;
    doc["cellular_signal"]   = _cellularSignal;
    doc["wifi_rssi"]         = WiFi.RSSI();
    doc["status"]            = 1;  // Online
    
    String json;
    serializeJson(doc, json);

    // Publish on both topics:
    // - "sensors" is consumed by the backend to persist SensorData to the DB
    // - "telemetry" is the keep-alive broadcast (logged only)
    String topicSensors = buildTopic("sensors");
    publish(topicSensors, (uint8_t*)json.c_str(), json.length(), 2);
    return publish(_topicTelemetry, (uint8_t*)json.c_str(), json.length(), 2);
}

static const char* triggerTypeString(FeedingTrigger t) {
    switch (t) {
        case FeedingTrigger::SCHEDULED: return "SCHEDULED";
        case FeedingTrigger::REMOTE:    return "MANUAL";   // remote-triggered = user-initiated manual
        default:                        return "MANUAL";
    }
}

bool CommunicationManager::sendFeedingEvent(const FeedingEvent& event) {
    JsonDocument doc;

    doc["device_id"]        = _deviceManager->getDeviceID();
    doc["timestamp"]        = event.timestamp;
    doc["quantity_grams"]   = event.quantityGrams;
    doc["actual_dispensed"] = event.actualDispensed;
    doc["duration_seconds"] = event.durationMs / 1000;
    doc["trigger_type"]     = triggerTypeString(event.trigger);
    doc["result"]           = (int)event.result;
    doc["error_message"]    = event.errorMessage;
    doc["temperature"]      = event.temperature;
    doc["q10_factor"]       = event.q10Factor;
    doc["obm_safety_factor"] = event.obmSafetyFactor;
    
    String json;
    serializeJson(doc, json);
    
    return publish(_topicFeeding, (uint8_t*)json.c_str(), json.length(), 4);
}

bool CommunicationManager::sendAlert(AlertType type, AlertSeverity severity, const String& message) {
    JsonDocument doc;
    
    doc["device_id"] = _deviceManager->getDeviceID();
    doc["timestamp"] = (int64_t)time(nullptr);
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
    doc["timestamp"] = (int64_t)time(nullptr);
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

bool CommunicationManager::sendDiagnosticsReport(const SystemDiagnostics& diagnostics) {
    JsonDocument doc;
    doc["device_id"] = _deviceManager->getDeviceID();
    doc["type"] = "diagnostics_report";
    diagnostics.toJson(doc);
    
    String json;
    serializeJson(doc, json);
    
    return publish(_topicDiagReport, (uint8_t*)json.c_str(), json.length(), 3);
}

bool CommunicationManager::sendPipelinePing(uint32_t nonce) {
    if (!_mqttClient.connected()) return false;

    JsonDocument doc;
    doc["device_id"] = _deviceManager->getDeviceID();
    doc["nonce"]     = nonce;
    doc["timestamp"] = (int64_t)millis();
    
    String json;
    serializeJson(doc, json);
    
    return _mqttClient.publish(_topicDiagPing.c_str(), json.c_str());
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
void CommunicationManager::setConfigCallback(ConfigCallback callback)   { _configCallback  = callback; }
