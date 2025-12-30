# MQTT Communication Guide

This document explains how the ESP32 hardware devices communicate with the Go backend server using MQTT (Message Queuing Telemetry Transport).

## What is MQTT?

MQTT is a lightweight messaging protocol designed for IoT (Internet of Things) devices. Think of it like a postal system:

- **Broker**: The post office that receives and delivers messages
- **Publisher**: Someone sending a letter (device sending data)
- **Subscriber**: Someone receiving letters (backend listening for data)
- **Topic**: The address on the letter (e.g., `devices/feeder-001/sensors`)
- **Payload**: The content of the letter (sensor data, commands, etc.)

### Why MQTT for Fish Feeders?

1. **Low bandwidth**: Perfect for cellular/GSM connections (uses minimal data)
2. **Reliable delivery**: Messages can be guaranteed to arrive (QoS levels)
3. **Bi-directional**: Both device and server can send/receive
4. **Offline support**: Messages queue when device is disconnected
5. **Real-time**: Near-instant message delivery

## Architecture Overview

```
┌─────────────────┐         ┌─────────────────┐         ┌─────────────────┐
│   ESP32 Device  │◄───────►│   MQTT Broker   │◄───────►│   Go Backend    │
│   (Fish Feeder) │  MQTT   │   (Mosquitto)   │  MQTT   │    Server       │
└─────────────────┘         └─────────────────┘         └─────────────────┘
        │                           │                           │
        │  Publishes:               │                           │  Subscribes:
        │  - Sensor data            │                           │  - All device data
        │  - Feeding events         │                           │  - Alerts
        │  - Status updates         │                           │
        │  - Alerts                 │                           │  Publishes:
        │                           │                           │  - Commands
        │  Subscribes:              │                           │  - Config updates
        │  - Commands               │                           │  - Schedule changes
        │  - Config updates         │                           │
        │  - Schedule changes       │                           │
        └───────────────────────────┴───────────────────────────┘
```

## Topic Structure

Topics are organized hierarchically like file paths:

### Device Telemetry (Device → Backend)

| Topic Pattern | Description | Example |
|--------------|-------------|---------|
| `devices/{deviceID}/telemetry` | All sensor readings | `devices/feeder-001/telemetry` |
| `devices/{deviceID}/sensors` | Raw sensor data | `devices/feeder-001/sensors` |
| `devices/{deviceID}/feeding` | Feeding events | `devices/feeder-001/feeding` |
| `devices/{deviceID}/status` | Device status | `devices/feeder-001/status` |
| `devices/{deviceID}/alerts` | Device alerts | `devices/feeder-001/alerts` |

### Device Commands (Backend → Device)

| Topic Pattern | Description | Example |
|--------------|-------------|---------|
| `devices/{deviceID}/commands` | Commands to device | `devices/feeder-001/commands` |
| `devices/{deviceID}/config` | Configuration updates | `devices/feeder-001/config` |

### Device Shadow (State Synchronization)

Device Shadow keeps track of device state even when offline:

| Topic Pattern | Description |
|--------------|-------------|
| `$aws/things/{deviceID}/shadow/update` | Device reports its state |
| `$aws/things/{deviceID}/shadow/get` | Request current shadow state |
| `$aws/things/{deviceID}/shadow/update/delta` | Differences between desired and reported state |

## Message Formats

All messages use **Protocol Buffers (Protobuf)** for efficient binary serialization, reducing cellular data usage by 60-80% compared to JSON.

### Telemetry Message

```protobuf
message DeviceTelemetry {
  string device_id = 1;
  int64 timestamp = 2;
  float temperature = 3;        // Water temperature (°C)
  float dissolved_oxygen = 4;   // DO level (mg/L)
  float ph = 5;                 // pH level
  float turbidity = 6;          // Water clarity (NTU)
  float weight_grams = 7;       // Feed hopper weight
  float weight_percent = 8;     // Feed level percentage
  int32 battery_level = 9;      // Battery percentage
  float battery_voltage = 10;   // Battery voltage
  PowerSource power_source = 11; // Solar/Battery/Electric
  float solar_voltage = 12;     // Solar panel voltage
  int32 cellular_signal = 13;   // GSM signal strength (CSQ)
  int32 wifi_rssi = 14;         // WiFi signal (dBm)
  DeviceStatus status = 15;     // Online/Offline/Sleeping
}
```

### Feeding Event Message

```protobuf
message FeedingEvent {
  string device_id = 1;
  int64 timestamp = 2;
  float quantity_grams = 3;     // Amount dispensed
  int32 duration_seconds = 4;   // Feeding duration
  FeedingTrigger trigger = 5;   // Scheduled/Manual/Adaptive
  FeedingResult result = 6;     // Success/Failed/Jammed
  string error_message = 7;     // Error details if failed
  float temperature = 8;        // Water temp during feeding
  float dissolved_oxygen = 9;   // DO during feeding
  float q10_factor = 10;        // Q10 adjustment applied
  float obm_safety_factor = 11; // OBM safety factor applied
}
```

### Command Message

```protobuf
message DeviceCommand {
  string device_id = 1;
  string command_id = 2;        // Unique command ID
  int64 timestamp = 3;
  CommandType type = 4;         // FeedNow/StopFeeding/UpdateConfig/etc.
  bytes payload = 5;            // Command-specific data
  int32 timeout_seconds = 6;    // Command timeout
}
```

## Communication Flow Examples

### 1. Regular Sensor Data Upload

```
ESP32                          MQTT Broker                    Go Backend
  │                                │                              │
  │  PUBLISH telemetry data        │                              │
  │  Topic: devices/feeder-001/telemetry                          │
  │ ──────────────────────────────►│                              │
  │                                │  Forward to subscribers      │
  │                                │─────────────────────────────►│
  │                                │                              │
  │                                │                    Process & store
  │                                │                    in database
```

### 2. Manual Feeding Command

```
Mobile App                     Go Backend                    MQTT Broker                    ESP32
    │                              │                              │                           │
    │  POST /api/v1/feeding/manual │                              │                           │
    │─────────────────────────────►│                              │                           │
    │                              │  PUBLISH command             │                           │
    │                              │  Topic: devices/feeder-001/commands                      │
    │                              │─────────────────────────────►│                           │
    │                              │                              │  Forward to device        │
    │                              │                              │──────────────────────────►│
    │                              │                              │                           │
    │                              │                              │              Execute feeding
    │                              │                              │                           │
    │                              │                              │  PUBLISH feeding event    │
    │                              │                              │◄──────────────────────────│
    │                              │◄─────────────────────────────│                           │
    │  WebSocket: feeding complete │                              │                           │
    │◄─────────────────────────────│                              │                           │
```

### 3. Device Shadow Synchronization

When device comes online after being offline:

```
ESP32                          MQTT Broker                    Go Backend
  │                                │                              │
  │  PUBLISH shadow/get            │                              │
  │  (Request current state)       │                              │
  │ ──────────────────────────────►│                              │
  │                                │─────────────────────────────►│
  │                                │                              │
  │                                │  PUBLISH shadow/get/accepted │
  │                                │  (Return stored state)       │
  │                                │◄─────────────────────────────│
  │◄───────────────────────────────│                              │
  │                                │                              │
  │  Compare with local state      │                              │
  │  Apply any pending changes     │                              │
  │                                │                              │
  │  PUBLISH shadow/update         │                              │
  │  (Report current state)        │                              │
  │ ──────────────────────────────►│                              │
  │                                │─────────────────────────────►│
```

## Quality of Service (QoS) Levels

MQTT supports three QoS levels:

| QoS | Name | Description | Use Case |
|-----|------|-------------|----------|
| 0 | At most once | Fire and forget | Non-critical telemetry |
| 1 | At least once | Guaranteed delivery (may duplicate) | Sensor data, alerts |
| 2 | Exactly once | Guaranteed single delivery | Commands, feeding events |

Our system uses:
- **QoS 1** for telemetry and sensor data
- **QoS 2** for commands and feeding events

## Security

### TLS Encryption

All MQTT connections use TLS 1.2+ encryption:

```
ESP32 ◄──── TLS 1.2 ────► MQTT Broker ◄──── TLS 1.2 ────► Backend
```

### Authentication

1. **Username/Password**: Basic authentication for development
2. **X.509 Certificates**: mTLS (mutual TLS) for production
   - Each device has a unique client certificate
   - Backend validates device identity
   - Certificates stored in ESP32 secure element

### Topic-Level Authorization

Devices can only publish/subscribe to their own topics:
- `feeder-001` can publish to `devices/feeder-001/*`
- `feeder-001` cannot access `devices/feeder-002/*`

## Offline Handling

When the device loses connectivity:

1. **Local Buffering**: ESP32 stores data in PSRAM (up to 4MB)
2. **Priority Queue**: Critical alerts sent first when reconnected
3. **Compression**: Data compressed with gzip before transmission
4. **Automatic Retry**: Exponential backoff for reconnection

```
┌─────────────────────────────────────────────────────────────┐
│                    ESP32 Offline Buffer                      │
├─────────────────────────────────────────────────────────────┤
│  Priority 5 (Critical): Emergency alerts, feeding failures  │
│  Priority 4 (High): Feeding events, low battery warnings    │
│  Priority 3 (Medium): Sensor anomalies                      │
│  Priority 2 (Low): Regular telemetry                        │
│  Priority 1 (Lowest): Diagnostic data                       │
└─────────────────────────────────────────────────────────────┘
```

## Setting Up MQTT Broker

### Development (Docker)

```bash
# Start Mosquitto broker with Docker
docker run -d --name mosquitto \
  -p 1883:1883 \
  -p 9001:9001 \
  eclipse-mosquitto:2
```

### Production (Eclipse Mosquitto)

1. Install Mosquitto:
```bash
# Ubuntu/Debian
sudo apt install mosquitto mosquitto-clients

# Or use Docker
docker-compose up -d mqtt
```

2. Configure TLS in `/etc/mosquitto/mosquitto.conf`:
```
listener 8883
cafile /etc/mosquitto/certs/ca.crt
certfile /etc/mosquitto/certs/server.crt
keyfile /etc/mosquitto/certs/server.key
require_certificate true
```

3. Configure authentication:
```
allow_anonymous false
password_file /etc/mosquitto/passwd
acl_file /etc/mosquitto/acl
```

## Testing MQTT Communication

### Using mosquitto_sub/pub

```bash
# Subscribe to all device telemetry
mosquitto_sub -h localhost -t "devices/+/telemetry" -v

# Publish a test command
mosquitto_pub -h localhost -t "devices/feeder-001/commands" \
  -m '{"type":"feed_now","quantity":50}'

# Subscribe to feeding events
mosquitto_sub -h localhost -t "devices/+/feeding" -v
```

### Using MQTT Explorer

[MQTT Explorer](http://mqtt-explorer.com/) is a GUI tool for testing:

1. Connect to `localhost:1883`
2. Browse topics in real-time
3. Publish test messages
4. View message history

## Environment Variables

Configure MQTT in `.env`:

```bash
# MQTT Broker Connection
SFF_MQTT_BROKER_URL=tcp://localhost:1883    # Development
SFF_MQTT_BROKER_URL=ssl://mqtt.example.com:8883  # Production with TLS

# Authentication
SFF_MQTT_USERNAME=backend
SFF_MQTT_PASSWORD=secure_password

# Connection Settings
SFF_MQTT_CLIENT_ID=smart-fish-feeder-backend
SFF_MQTT_KEEP_ALIVE=60s
SFF_MQTT_QOS=1

# TLS Settings (Production)
SFF_MQTT_TLS_ENABLED=true
SFF_MQTT_TLS_CA_CERT=/path/to/ca.crt
SFF_MQTT_TLS_CLIENT_CERT=/path/to/client.crt
SFF_MQTT_TLS_CLIENT_KEY=/path/to/client.key
```

## Troubleshooting

### Device Not Receiving Commands

1. Check device is subscribed to command topic
2. Verify QoS level matches
3. Check broker ACL permissions
4. Ensure device is online (check status topic)

### High Latency

1. Check network connectivity
2. Reduce message payload size
3. Use QoS 0 for non-critical data
4. Check broker load

### Messages Not Persisting

1. Enable message persistence in broker
2. Use QoS 1 or 2
3. Set `retained` flag for config messages

## Further Reading

- [MQTT Specification](https://mqtt.org/mqtt-specification/)
- [Eclipse Mosquitto Documentation](https://mosquitto.org/documentation/)
- [AWS IoT Device Shadow](https://docs.aws.amazon.com/iot/latest/developerguide/iot-device-shadows.html)
- [Protocol Buffers](https://developers.google.com/protocol-buffers)
