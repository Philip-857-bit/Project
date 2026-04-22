# Smart Fish Feeder Mobile App

A Flutter mobile application for controlling and monitoring the Smart Fish Feeder IoT device.

## Features

- **Device Management**: Pair, configure, and manage multiple fish feeders
- **Feeding Control**: Manual feeding, scheduled feeding, and feeding history
- **Real-time Monitoring**: Live sensor data (temperature, feed level, battery, DO)
- **Feed Calculator**: Q10-based feed calculations with species-specific parameters
- **Video Verification**: Computer vision analysis of feeding behavior
- **Alerts & Notifications**: Threshold-based alerts for critical conditions
- **Biometric Authentication**: Face ID / Fingerprint login support
- **Certificate Pinning**: TLS certificate pinning for enhanced security
- **ECDH Key Exchange**: Secure BLE communication with AES-256-GCM encryption

## Environment Configuration

### Setting Up Environment Variables

1. Copy the example environment file:
   ```bash
   cp .env.example .env
   ```

2. Edit `.env` with your configuration values

3. Build with environment variables:
   ```bash
   # Using dart-define-from-file (recommended)
   flutter build apk --dart-define-from-file=.env
   
   # Or individual defines
   flutter build apk \
     --dart-define=API_BASE_URL=https://your-api.com/api/v1 \
     --dart-define=MQTT_HOST=mqtt.your-domain.com \
     --dart-define=MQTT_PORT=8883 \
     --dart-define=CERT_FINGERPRINT_1=your-fingerprint \
     --dart-define=API_DOMAIN=your-domain.com
   ```

### Environment Variables Reference

| Variable | Description | Where to Get It |
|----------|-------------|-----------------|
| `API_BASE_URL` | Backend API URL | Your deployed backend URL (e.g., Railway deployment URL + `/api/v1`) |
| `MQTT_HOST` | MQTT broker hostname | Your MQTT broker (e.g., HiveMQ Cloud, EMQX, or self-hosted) |
| `MQTT_PORT` | MQTT broker port | Usually `8883` for TLS, `1883` for non-TLS |
| `CERT_FINGERPRINT_1` | Primary TLS certificate SHA256 fingerprint | See "Getting Certificate Fingerprints" below |
| `CERT_FINGERPRINT_2` | Backup TLS certificate fingerprint | Same as above (for certificate rotation) |
| `API_DOMAIN` | Domain for certificate validation | Your API domain (e.g., `smartfishfeeder.com`) |
| `DEBUG_MODE` | Enable debug features | Set to `true` for development, `false` for production |

### Getting Certificate Fingerprints

To get the SHA256 fingerprint of your server's TLS certificate:

```bash
# Linux/macOS
openssl s_client -connect your-domain.com:443 < /dev/null 2>/dev/null | \
  openssl x509 -outform DER | \
  openssl dgst -sha256 -binary | \
  openssl base64

# Windows (PowerShell)
$cert = [System.Net.ServicePointManager]::ServerCertificateValidationCallback = {$true}
$request = [System.Net.HttpWebRequest]::Create("https://your-domain.com")
$request.GetResponse() | Out-Null
$cert = $request.ServicePoint.Certificate
$bytes = $cert.Export([System.Security.Cryptography.X509Certificates.X509ContentType]::Cert)
$sha256 = [System.Security.Cryptography.SHA256]::Create()
[Convert]::ToBase64String($sha256.ComputeHash($bytes))
```

### MQTT Broker Options

1. **HiveMQ Cloud** (Recommended for production)
   - Sign up at https://www.hivemq.com/cloud/
   - Create a cluster and get connection details
   - Use port `8883` with TLS

2. **EMQX Cloud**
   - Sign up at https://www.emqx.com/en/cloud
   - Create a deployment and get broker URL

3. **Self-hosted (Mosquitto)**
   - Install Mosquitto on your server
   - Configure TLS certificates
   - Open port `8883` for secure connections

## Architecture

```
lib/
├── core/
│   ├── models/           # Data models
│   │   ├── device.dart
│   │   ├── feeding.dart
│   │   ├── sensor_data.dart
│   │   ├── user.dart
│   │   └── video_verification.dart
│   ├── providers/        # Riverpod state management
│   │   ├── auth_provider.dart
│   │   ├── device_provider.dart
│   │   ├── feeding_provider.dart
│   │   ├── monitoring_provider.dart
│   │   ├── calculator_provider.dart
│   │   ├── video_provider.dart
│   │   └── realtime_provider.dart
│   ├── services/         # External services
│   │   ├── api_service.dart      # REST API client
│   │   ├── mqtt_service.dart     # MQTT real-time
│   │   ├── ble_service.dart      # Bluetooth provisioning
│   │   └── storage_service.dart  # Secure storage
│   ├── router/           # GoRouter navigation
│   └── theme/            # Material 3 theming
├── features/
│   ├── auth/             # Login, register, splash
│   ├── dashboard/        # Main dashboard
│   ├── devices/          # Device list, detail, pairing
│   ├── feeding/          # Schedules, manual feed, history
│   ├── monitoring/       # Sensor dashboard
│   ├── calculator/       # Feed calculator
│   ├── video/            # Video verification
│   └── settings/         # App settings
└── main.dart
```

## State Management

Uses **Riverpod** for state management with the following providers:

- `authStateProvider` - Authentication state
- `deviceListProvider` - Device list and operations
- `feedingSchedulesProvider` - Feeding schedules CRUD
- `feedingHistoryProvider` - Feeding event history
- `sensorDataProvider` - Real-time sensor data
- `alertsProvider` - Device alerts
- `calculatorProvider` - Feed calculations
- `videoVerificationProvider` - Video clips and analysis
- `realtimeProvider` - MQTT connection state

## Getting Started

### Prerequisites

- Flutter SDK 3.0+
- Dart 3.0+
- Android Studio / VS Code with Flutter extensions
- Android NDK 27.0.12077973 (for native dependencies)

### Installation

```bash
# Get dependencies
flutter pub get

# Run the app (development)
flutter run

# Run with environment variables
flutter run --dart-define-from-file=.env
```

### Build Commands

```bash
# Android APK (debug)
flutter build apk --debug --dart-define-from-file=.env

# Android APK (release)
flutter build apk --release --dart-define-from-file=.env

# Android App Bundle (for Play Store)
flutter build appbundle --release --dart-define-from-file=.env

# iOS (requires macOS)
flutter build ios --release --dart-define-from-file=.env
```

## Key Dependencies

- `flutter_riverpod` - State management
- `go_router` - Navigation
- `dio` - HTTP client
- `flutter_blue_plus` - Bluetooth Low Energy
- `mobile_scanner` - QR code scanning
- `flutter_secure_storage` - Secure token storage
- `intl` - Date/time formatting

## Device Pairing Flow

1. **Scan QR Code** - Scan device QR or enter serial manually
2. **BLE Discovery** - Find device via Bluetooth
3. **Connect** - Establish BLE connection
4. **Configure Network** - Set up WiFi or cellular (APN)
5. **Bind Device** - Link device to user account

## Real-time Updates

The app receives near-real-time updates through backend APIs.

- Mobile polls backend monitoring endpoints for latest telemetry and alerts
- Backend remains the single MQTT bridge to devices
- Device commands are sent to backend HTTP endpoints

## API Endpoints

| Endpoint | Description |
|----------|-------------|
| `POST /auth/login` | User login |
| `POST /auth/register` | User registration |
| `GET /devices` | List user devices |
| `POST /devices/bind` | Bind new device |
| `GET /devices/{id}/sensors` | Get sensor data |
| `GET /devices/{id}/schedules` | Get feeding schedules |
| `POST /devices/{id}/feed` | Trigger manual feed |
| `POST /calculator/feed` | Calculate feed amount |
| `GET /calculator/species` | Get fish species |

## Build

```bash
# Android APK
flutter build apk --release --dart-define-from-file=.env

# iOS
flutter build ios --release --dart-define-from-file=.env

# Android App Bundle
flutter build appbundle --release --dart-define-from-file=.env
```

## Troubleshooting

### NDK Issues
If you encounter NDK-related build errors, ensure you have NDK version `27.0.12077973` installed:
```bash
# Install via Android Studio SDK Manager or:
sdkmanager "ndk;27.0.12077973"
```

### Certificate Pinning in Development
During development, certificate pinning is bypassed automatically. For production builds, ensure you have valid certificate fingerprints configured.

### MQTT Connection Issues
- Verify your MQTT broker is accessible
- Check firewall rules for port 8883 (TLS) or 1883 (non-TLS)
- Ensure JWT token is valid for MQTT authentication

## License

Proprietary - All rights reserved
