# Smart Fish Feeder - ESP32 Firmware

Intelligent fish feeding controller firmware for Nigerian aquaculture.

## Hardware Components

### Main Controller
- **LILYGO T-A7670 R2** - ESP32-WROVER-B with A7670G 4G LTE Cat1
- Build: `pio run -e t-a7670-dm542`

### Camera Module
- **ESP32-CAM** (AI-Thinker) with OV2640 sensor
- Build: `pio run -e esp32cam`

### Motor System
- **NEMA 23 Stepper Motor** - High torque for auger
- **DM542 Driver** (Primary) - 24-48V, up to 4.2A
- **TB6600 Driver** (Alternative) - 9-42V, up to 4A
- **20mm Wood Drill Auger Bit** - Feed dispenser

### Sensors
- **HX711 + 20kg Load Cell** - Primary feed weight measurement
- **JSN-SR04T** - Backup waterproof ultrasonic for feed level
- **DS18B20** - Waterproof temperature probe

### Power System (Solar)
- **Solar Panel** (12V/24V, 20-50W recommended)
- **Solar Charge Controller** (PWM or MPPT)
- **18650 Li-Ion Battery** (built-in on T-A7670 R2 for ESP32)
- **24-48V Battery Bank** (for motor, separate from ESP32)

## Wiring Diagram

### T-A7670 R2 to DM542/TB6600

```
T-A7670 R2            DM542/TB6600
──────────            ────────────
GPIO32 ──────────────► PUL+ (Step)
GND    ──────────────► PUL- 
GPIO33 ──────────────► DIR+ (Direction)
GND    ──────────────► DIR-
GPIO0  ──────────────► ENA+ (Enable)
GND    ──────────────► ENA-

Note: DM542 requires 5V logic - use optocoupler or level shifter
      TB6600 works with 3.3V directly
```

### DM542 DIP Switch Settings (Recommended)

```
Microstep: 8 (SW5=ON, SW6=OFF, SW7=ON, SW8=OFF)
Current: Set based on your NEMA 23 motor (typically 2.8A)

SW1-SW3: Current setting
SW4: Half current when idle (ON recommended)
SW5-SW8: Microstep setting
```

### TB6600 DIP Switch Settings (Recommended)

```
Microstep: 8 (S4=ON, S5=OFF, S6=ON)
Current: Set based on your NEMA 23 motor

S1-S3: Current setting
S4-S6: Microstep setting
```

### Sensor Connections

```
T-A7670 R2            Sensor
──────────            ──────
# HX711 Load Cell (20kg)
GPIO39 (VN) ◄─────────  HX711 DOUT
GPIO5  ──────────────►  HX711 SCK
3.3V   ──────────────►  HX711 VCC
GND    ──────────────►  HX711 GND

# JSN-SR04T Ultrasonic
GPIO17 ──────────────►  TRIG
GPIO34 ◄──────────────  ECHO
5V     ──────────────►  VCC
GND    ──────────────►  GND

# DS18B20 Temperature
GPIO23 ──────────────►  DATA (with 4.7k pull-up to 3.3V)
3.3V   ──────────────►  VCC
GND    ──────────────►  GND
```

### Power System Wiring

```
Solar Panel (12V/24V)
        │
        ▼
┌───────────────────┐
│  Charge Controller │
│  (PWM or MPPT)     │
└───────┬───────────┘
        │
        ▼
┌───────────────────┐     ┌─────────────┐
│  24-48V Battery   │────►│  DM542/TB6600│────► NEMA 23 Motor
│  Bank (for motor) │     └─────────────┘
└───────────────────┘
        │
        │ (via DC-DC converter to 5V)
        ▼
┌───────────────────┐
│  T-A7670 R2       │
│  (USB-C or 5V in) │
│  + 18650 backup   │
└───────────────────┘

Solar Voltage Monitoring:
Solar Panel ──┬── 10kΩ ──┬── 10kΩ ──┬── GND
              │          │          │
              │          └──► GPIO36 (VP)
```

## Building & Uploading

### Prerequisites
- [PlatformIO](https://platformio.org/) CLI or VS Code extension
- USB-C cable for T-A7670 R2

### Build Commands

```bash
# Build with DM542 driver (default)
pio run -e t-a7670-dm542

# Build with TB6600 driver
pio run -e t-a7670-tb6600

# Build camera module
pio run -e esp32cam

# Upload to main controller
pio run -e t-a7670-dm542 --target upload

# Monitor serial output
pio device monitor --baud 115200
```

## Configuration

### Motor Calibration

The auger dispenser needs calibration for accurate feeding:

1. **Measure grams per revolution:**
   - Run motor for exactly 1 revolution
   - Weigh the dispensed feed
   - Update `GRAMS_PER_REVOLUTION` in config.h or use calibration command

2. **Set microsteps:**
   - Match DIP switch setting on DM542/TB6600
   - Default: 8 microsteps
   - Update `MOTOR_MICROSTEPS` if different

### DM542/TB6600 Settings

| Parameter | DM542 | TB6600 | Notes |
|-----------|-------|--------|-------|
| Input Voltage | 24-48V DC | 9-42V DC | Use 24V for NEMA 23 |
| Max Current | 4.2A | 4.0A | Set to motor rating |
| Microsteps | 1-256 | 1-32 | 8 recommended |
| Pulse Width | 2.5µs min | 5µs min | Firmware handles this |

### Nigerian Carrier APNs

Update `MODEM_APN` in `include/config.h`:

```cpp
// MTN Nigeria
#define MODEM_APN "web.gprs.mtnnigeria.net"

// Glo Nigeria
#define MODEM_APN "gloflat"

// Airtel Nigeria
#define MODEM_APN "internet.ng.airtel.com"

// 9mobile
#define MODEM_APN "9mobile"
```

## Features

### Feeding Control
- Scheduled feeding (up to 10 entries)
- Manual feeding via app
- Q10 temperature-adjusted portions
- OBM (Oxygen Budget Management) safety
- Feed level monitoring (dual sensing)

### Communication
- **Primary**: A7670G 4G LTE Cat1
- **Secondary**: WiFi (when available)
- MQTT protocol for real-time updates
- Offline buffering for poor connectivity
- GPS location tracking

### Power Management
- Solar-first power strategy
- 18650 battery backup for ESP32
- Deep sleep for power conservation
- Low battery/solar alerts
- Power source detection

### Sensors
- Water temperature monitoring
- Feed hopper level (load cell + ultrasonic)
- Camera for feeding verification

## MQTT Topics

| Topic | Direction | Description |
|-------|-----------|-------------|
| `devices/{id}/telemetry` | Publish | Sensor data |
| `devices/{id}/feeding` | Publish | Feeding events |
| `devices/{id}/alerts` | Publish | Alerts |
| `devices/{id}/commands` | Subscribe | Commands |
| `devices/{id}/config` | Subscribe | Configuration |

## Troubleshooting

### Motor Not Running
1. Check 24-48V power to DM542/TB6600
2. Verify ENA+ is LOW when running (GPIO0)
3. Check motor coil connections (A+/A-, B+/B-)
4. Verify DIP switch settings match config
5. For DM542: Check level shifter/optocoupler

### Motor Running But Not Dispensing
1. Check auger is properly attached to motor shaft
2. Verify motor direction (swap A+/A- if reversed)
3. Check for feed blockage in hopper
4. Calibrate grams per revolution

### A7670 Not Connecting
1. Check SIM card insertion (nano SIM)
2. Verify antenna connection
3. Check APN settings for your carrier
4. Ensure SIM has data plan active
5. Check MODEM_EN (GPIO12) is HIGH

### Solar Not Detected
1. Check voltage divider on GPIO36
2. Verify solar panel output voltage
3. Check charge controller connections
4. Ensure panel is in sunlight

### Temperature Sensor Not Reading
1. Verify 4.7kΩ pull-up resistor on DATA line
2. Check OneWire connection to GPIO23
3. Ensure probe is waterproof version

## Bill of Materials

| Component | Specification | Qty |
|-----------|--------------|-----|
| LILYGO T-A7670 R2 | ESP32 + A7670G | 1 |
| ESP32-CAM | AI-Thinker OV2640 | 1 |
| NEMA 23 Stepper | 2.8A, 1.8° | 1 |
| DM542 or TB6600 | Stepper driver | 1 |
| 20mm Wood Auger | Feed dispenser | 1 |
| HX711 + 20kg Load Cell | Weight sensor | 1 |
| JSN-SR04T | Ultrasonic sensor | 1 |
| DS18B20 | Waterproof temp probe | 1 |
| Solar Panel | 12V/24V, 20-50W | 1 |
| Charge Controller | PWM/MPPT | 1 |
| 24V Battery | For motor | 1 |
| 18650 Battery | For ESP32 (built-in) | 1 |

## Wokwi Setup (PlatformIO)

This repository is now configured for Wokwi simulation with PlatformIO.

### Files added for Wokwi

- `wokwi.toml` - points Wokwi to the PlatformIO firmware output
- `diagram.json` - Wokwi circuit (ESP32 + HC-SR04 + DS18B20 + stepper driver)
- `platformio.ini` environment: `t-a7670-wokwi`
- `scripts/load_wokwi_env.py` - loads `.env` / `.env.local` values into build flags
- `.env.example` - template for local simulation credentials

### Configure MQTT credentials for Wokwi

1. Copy `.env.example` to `.env` inside `firmware/`.
2. Set your MQTT values in `.env`.
3. Build `t-a7670-wokwi`.

Supported `.env` keys:

- `WOKWI_DEFAULT_WIFI_SSID`
- `WOKWI_DEFAULT_WIFI_PASS`
- `WOKWI_DEFAULT_MQTT_HOST`
- `WOKWI_DEFAULT_MQTT_USER`
- `WOKWI_DEFAULT_MQTT_PASS`
- `MQTT_USE_TLS` (`1` or `0`)
- `MQTT_SKIP_CERT_VERIFY` (`1` or `0`)
- `MQTT_PORT` (optional)
- `MQTT_PORT_TLS` (optional)

### Simulation profile

- Battery is simulated as fixed **24V**
- Solar input is disabled in simulation (`NO_SOLAR_INPUT`)
- Driver is configured as **DM542 (Step/Dir/EN)**
- Schematic pin map is enabled (`SCHEMATIC_PINMAP`):
        - MOTOR_STEP  -> GPIO25
        - MOTOR_STEP2 -> GPIO19
        - TRIG        -> GPIO12
        - ECHO_S      -> GPIO13
        - DATA        -> GPIO14

Note: Wokwi does not provide a native DM542 model, so the diagram uses a compatible
stepper driver part as a **DM542 signal-level stand-in** (Step/Dir/EN behavior).

### Run with PlatformIO extension

1. Select environment: `t-a7670-wokwi`
2. Build project
3. Run command: **Wokwi: Start Simulator**

### Run with CLI

```bash
pio run -e t-a7670-wokwi
```

## License

Proprietary - Smart Fish Feeder Project
