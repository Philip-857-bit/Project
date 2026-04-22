/**
 * @file DeviceManager.cpp
 * @brief Device identification, BLE provisioning, and certificate management
 */

#include "DeviceManager.h"
#include "../../include/config.h"

DeviceManager::DeviceManager() 
    : _storage(nullptr)
    , _isProvisioned(false)
    , _provisioningState(ProvisioningState::IDLE)
    , _bleServer(nullptr)
    , _bleService(nullptr)
    , _deviceConnected(false)
    , _provisioningStartTime(0) {
}

bool DeviceManager::begin(NVSStorage* storage) {
    _storage = storage;
    
    // Generate or load device ID
    _deviceID = _storage->getString(NVS_KEY_DEVICE_ID);
    if (_deviceID.isEmpty()) {
        _deviceID = generateDeviceID();
        _storage->putString(NVS_KEY_DEVICE_ID, _deviceID);
        Serial.printf("[DeviceManager] Generated new device ID: %s\n", _deviceID.c_str());
    }
    
    // Load credentials
    _isProvisioned = loadCredentials();
    
    // Initialize status LED (T-A7670 R2 has single LED on GPIO2)
    pinMode(PIN_LED_STATUS, OUTPUT);
    
    return true;
}

String DeviceManager::generateDeviceID() {
    uint8_t mac[6];
    esp_efuse_mac_get_default(mac);
    
    char deviceID[32];
    snprintf(deviceID, sizeof(deviceID), "SFF-%02X%02X%02X%02X%02X%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    
    return String(deviceID);
}

bool DeviceManager::loadCredentials() {
    _wifiSSID = _storage->getString(NVS_KEY_WIFI_SSID);
    _wifiPassword = _storage->getString(NVS_KEY_WIFI_PASS);
    _mqttHost = _storage->getString(NVS_KEY_MQTT_HOST);
    _mqttUsername = _storage->getString(NVS_KEY_MQTT_USER);
    _mqttPassword = _storage->getString(NVS_KEY_MQTT_PASS);
    _cellularAPN = _storage->getString(NVS_KEY_CELL_APN);

#ifdef WOKWI_SIM
    if (_wifiSSID.isEmpty()) {
        _wifiSSID = WOKWI_DEFAULT_WIFI_SSID;
        _wifiPassword = WOKWI_DEFAULT_WIFI_PASS;
    }
    if (_mqttHost.isEmpty()) {
        _mqttHost = WOKWI_DEFAULT_MQTT_HOST;
    }
    if (_mqttUsername.isEmpty()) {
        _mqttUsername = WOKWI_DEFAULT_MQTT_USER;
    }
    if (_mqttPassword.isEmpty()) {
        _mqttPassword = WOKWI_DEFAULT_MQTT_PASS;
    }
#endif
    
    if (_cellularAPN.isEmpty()) {
        _cellularAPN = MODEM_APN;
    }
    
    // Check if we have minimum required credentials
    return !_mqttHost.isEmpty();
}

bool DeviceManager::saveCredentials() {
    bool success = true;
    
    success &= _storage->putString(NVS_KEY_WIFI_SSID, _wifiSSID);
    success &= _storage->putString(NVS_KEY_WIFI_PASS, _wifiPassword);
    success &= _storage->putString(NVS_KEY_MQTT_HOST, _mqttHost);
    success &= _storage->putString(NVS_KEY_MQTT_USER, _mqttUsername);
    success &= _storage->putString(NVS_KEY_MQTT_PASS, _mqttPassword);
    success &= _storage->putString(NVS_KEY_CELL_APN, _cellularAPN);
    
    return success;
}

void DeviceManager::enterProvisioningMode() {
    Serial.println("[DeviceManager] Entering provisioning mode");
    
    _provisioningState = ProvisioningState::ADVERTISING;
    _provisioningStartTime = millis();
    
    initBLE();
    
    // Visual indicator
    digitalWrite(PIN_LED_STATUS, HIGH);
}

void DeviceManager::exitProvisioningMode() {
    Serial.println("[DeviceManager] Exiting provisioning mode");
    
    if (_bleServer != nullptr) {
        BLEDevice::deinit(true);
        _bleServer = nullptr;
    }
    
    _provisioningState = ProvisioningState::IDLE;
    digitalWrite(PIN_LED_STATUS, LOW);
}

void DeviceManager::initBLE() {
    BLEDevice::init("SmartFishFeeder");
    
    _bleServer = BLEDevice::createServer();
    _bleServer->setCallbacks(this);
    
    _bleService = _bleServer->createService(SERVICE_UUID);
    
    // Device ID characteristic (read-only)
    _charDeviceID = _bleService->createCharacteristic(
        CHAR_DEVICE_ID_UUID,
        BLECharacteristic::PROPERTY_READ
    );
    _charDeviceID->setValue(_deviceID.c_str());
    
    // WiFi SSID characteristic (write)
    _charWifiSSID = _bleService->createCharacteristic(
        CHAR_WIFI_SSID_UUID,
        BLECharacteristic::PROPERTY_WRITE
    );
    _charWifiSSID->setCallbacks(this);
    
    // WiFi Password characteristic (write)
    _charWifiPass = _bleService->createCharacteristic(
        CHAR_WIFI_PASS_UUID,
        BLECharacteristic::PROPERTY_WRITE
    );
    _charWifiPass->setCallbacks(this);
    
    // MQTT Host characteristic (write)
    _charMqttHost = _bleService->createCharacteristic(
        CHAR_MQTT_HOST_UUID,
        BLECharacteristic::PROPERTY_WRITE
    );
    _charMqttHost->setCallbacks(this);
    
    // Status characteristic (read/notify)
    _charStatus = _bleService->createCharacteristic(
        CHAR_STATUS_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
    );
    _charStatus->addDescriptor(new BLE2902());
    _charStatus->setValue("READY");
    
    _bleService->start();
    
    BLEAdvertising* advertising = BLEDevice::getAdvertising();
    advertising->addServiceUUID(SERVICE_UUID);
    advertising->setScanResponse(true);
    advertising->setMinPreferred(0x06);
    advertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();
    
    Serial.println("[DeviceManager] BLE advertising started");
}

void DeviceManager::onConnect(BLEServer* pServer) {
    Serial.println("[DeviceManager] BLE client connected");
    _deviceConnected = true;
    _provisioningState = ProvisioningState::CONNECTED;
}

void DeviceManager::onDisconnect(BLEServer* pServer) {
    Serial.println("[DeviceManager] BLE client disconnected");
    _deviceConnected = false;
    
    if (_provisioningState != ProvisioningState::COMPLETE) {
        _provisioningState = ProvisioningState::ADVERTISING;
        BLEDevice::startAdvertising();
    }
}

void DeviceManager::onWrite(BLECharacteristic* pCharacteristic) {
    String uuid = pCharacteristic->getUUID().toString().c_str();
    String value = pCharacteristic->getValue().c_str();
    
    Serial.printf("[DeviceManager] Received write to %s\n", uuid.c_str());
    
    _provisioningState = ProvisioningState::RECEIVING_CREDENTIALS;
    
    if (uuid == CHAR_WIFI_SSID_UUID) {
        _wifiSSID = value;
        Serial.printf("[DeviceManager] WiFi SSID set: %s\n", _wifiSSID.c_str());
    }
    else if (uuid == CHAR_WIFI_PASS_UUID) {
        _wifiPassword = value;
        Serial.println("[DeviceManager] WiFi password set");
    }
    else if (uuid == CHAR_MQTT_HOST_UUID) {
        _mqttHost = value;
        Serial.printf("[DeviceManager] MQTT host set: %s\n", _mqttHost.c_str());
        
        // MQTT host is the last required field, validate and save
        if (validateCredentials()) {
            _provisioningState = ProvisioningState::VALIDATING;
            _charStatus->setValue("VALIDATING");
            _charStatus->notify();
            
            if (saveCredentials()) {
                _isProvisioned = true;
                _provisioningState = ProvisioningState::COMPLETE;
                _charStatus->setValue("SUCCESS");
                _charStatus->notify();
                
                Serial.println("[DeviceManager] Provisioning complete!");
                
                // Delay to allow notification to be sent
                delay(1000);
                exitProvisioningMode();
            } else {
                _provisioningState = ProvisioningState::FAILED;
                _charStatus->setValue("SAVE_FAILED");
                _charStatus->notify();
            }
        } else {
            _provisioningState = ProvisioningState::FAILED;
            _charStatus->setValue("INVALID_CREDENTIALS");
            _charStatus->notify();
        }
    }
}

bool DeviceManager::validateCredentials() {
    // Minimum validation - MQTT host is required
    if (_mqttHost.isEmpty()) {
        Serial.println("[DeviceManager] Validation failed: MQTT host required");
        return false;
    }
    
    // WiFi credentials are optional (can use cellular only)
    return true;
}

void DeviceManager::update() {
    if (_provisioningState == ProvisioningState::ADVERTISING ||
        _provisioningState == ProvisioningState::CONNECTED ||
        _provisioningState == ProvisioningState::RECEIVING_CREDENTIALS) {
        
        // Check for timeout
        if (millis() - _provisioningStartTime > PROVISIONING_TIMEOUT) {
            Serial.println("[DeviceManager] Provisioning timeout");
            _provisioningState = ProvisioningState::FAILED;
            exitProvisioningMode();
        }
        
        updateStatusLED();
    }
}

void DeviceManager::updateStatusLED() {
    static unsigned long lastBlink = 0;
    static bool ledState = false;
    
    unsigned long interval = 500;
    if (_provisioningState == ProvisioningState::CONNECTED) {
        interval = 200;
    }
    
    if (millis() - lastBlink > interval) {
        lastBlink = millis();
        ledState = !ledState;
        digitalWrite(PIN_LED_STATUS, ledState);
    }
}

String DeviceManager::getDeviceID() const {
    return _deviceID;
}

bool DeviceManager::isProvisioned() const {
    return _isProvisioned;
}

ProvisioningState DeviceManager::getProvisioningState() const {
    return _provisioningState;
}

DeviceInfo DeviceManager::getDeviceInfo() const {
    DeviceInfo info;
    info.deviceID = _deviceID;
    info.firmwareVersion = FIRMWARE_VERSION;
    info.isProvisioned = _isProvisioned;
    info.hasCertificate = !_mqttPassword.isEmpty(); // Certificate/credentials available if MQTT password is set
    info.bootCount = _storage ? _storage->getUInt("boot_count") : 0;
    return info;
}

String DeviceManager::getWiFiSSID() const { return _wifiSSID; }
String DeviceManager::getWiFiPassword() const { return _wifiPassword; }
String DeviceManager::getMQTTHost() const { return _mqttHost; }
String DeviceManager::getMQTTUsername() const { return _mqttUsername; }
String DeviceManager::getMQTTPassword() const { return _mqttPassword; }
String DeviceManager::getCellularAPN() const { return _cellularAPN; }
