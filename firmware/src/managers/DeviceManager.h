/**
 * @file DeviceManager.h
 * @brief Device identification, BLE provisioning, and certificate management
 */

#ifndef DEVICE_MANAGER_H
#define DEVICE_MANAGER_H

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "../storage/NVSStorage.h"

// BLE Service and Characteristic UUIDs
#define SERVICE_UUID           "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHAR_DEVICE_ID_UUID    "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define CHAR_WIFI_SSID_UUID    "beb5483e-36e1-4688-b7f5-ea07361b26a9"
#define CHAR_WIFI_PASS_UUID    "beb5483e-36e1-4688-b7f5-ea07361b26aa"
#define CHAR_MQTT_HOST_UUID    "beb5483e-36e1-4688-b7f5-ea07361b26ab"
#define CHAR_STATUS_UUID       "beb5483e-36e1-4688-b7f5-ea07361b26ac"

// Provisioning states
enum class ProvisioningState {
    IDLE,
    ADVERTISING,
    CONNECTED,
    RECEIVING_CREDENTIALS,
    VALIDATING,
    COMPLETE,
    FAILED
};

// Device status
struct DeviceInfo {
    String deviceID;
    String firmwareVersion;
    bool isProvisioned;
    bool hasCertificate;
    uint32_t bootCount;
};

class DeviceManager : public BLEServerCallbacks, public BLECharacteristicCallbacks {
public:
    DeviceManager();
    
    /**
     * Initialize device manager
     * @param storage NVS storage instance
     * @return true if successful
     */
    bool begin(NVSStorage* storage);
    
    /**
     * Get device ID
     * @return Device ID string
     */
    String getDeviceID() const;
    
    /**
     * Check if device is provisioned
     * @return true if provisioned with credentials
     */
    bool isProvisioned() const;
    
    /**
     * Enter BLE provisioning mode
     */
    void enterProvisioningMode();
    
    /**
     * Exit provisioning mode
     */
    void exitProvisioningMode();
    
    /**
     * Get provisioning state
     * @return Current provisioning state
     */
    ProvisioningState getProvisioningState() const;
    
    /**
     * Update provisioning (call in loop)
     */
    void update();
    
    /**
     * Get device info
     * @return DeviceInfo struct
     */
    DeviceInfo getDeviceInfo() const;
    
    /**
     * Get WiFi SSID
     * @return WiFi SSID
     */
    String getWiFiSSID() const;
    
    /**
     * Get WiFi password
     * @return WiFi password
     */
    String getWiFiPassword() const;
    
    /**
     * Get MQTT host
     * @return MQTT broker host
     */
    String getMQTTHost() const;
    
    /**
     * Get MQTT username
     * @return MQTT username
     */
    String getMQTTUsername() const;
    
    /**
     * Get MQTT password
     * @return MQTT password
     */
    String getMQTTPassword() const;
    
    /**
     * Get cellular APN
     * @return Cellular APN
     */
    String getCellularAPN() const;
    
    // BLE callbacks
    void onConnect(BLEServer* pServer) override;
    void onDisconnect(BLEServer* pServer) override;
    void onWrite(BLECharacteristic* pCharacteristic) override;

private:
    NVSStorage* _storage;
    String _deviceID;
    String _wifiSSID;
    String _wifiPassword;
    String _mqttHost;
    String _mqttUsername;
    String _mqttPassword;
    String _cellularAPN;
    
    bool _isProvisioned;
    ProvisioningState _provisioningState;
    
    BLEServer* _bleServer;
    BLEService* _bleService;
    BLECharacteristic* _charDeviceID;
    BLECharacteristic* _charWifiSSID;
    BLECharacteristic* _charWifiPass;
    BLECharacteristic* _charMqttHost;
    BLECharacteristic* _charStatus;
    
    bool _deviceConnected;
    unsigned long _provisioningStartTime;
    static const unsigned long PROVISIONING_TIMEOUT = 300000; // 5 minutes
    
    /**
     * Generate unique device ID from MAC address
     * @return Device ID string
     */
    String generateDeviceID();
    
    /**
     * Load credentials from NVS
     * @return true if credentials exist
     */
    bool loadCredentials();
    
    /**
     * Save credentials to NVS
     * @return true if successful
     */
    bool saveCredentials();
    
    /**
     * Initialize BLE server
     */
    void initBLE();
    
    /**
     * Update status LED based on provisioning state
     */
    void updateStatusLED();
    
    /**
     * Validate received credentials
     * @return true if valid
     */
    bool validateCredentials();
};

#endif // DEVICE_MANAGER_H
