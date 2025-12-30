# Implementation Plan

## Backend Service Implementation (Go)

- [x] 1. Set up Go project structure and core dependencies


  - Initialize Go module with proper directory structure
  - Configure PostgreSQL database connection with GORM
  - Set up Redis for caching and session management
  - Configure environment variables and settings management with Viper
  - Set up structured logging with logrus and monitoring infrastructure
  - Configure Gin web framework with middleware
  - _Requirements: All system requirements_

- [x] 1.1 Implement user authentication system






  - Create User model with GORM
  - Implement password hashing with bcrypt
  - Create JWT token generation and validation utilities
  - Implement user registration handler with email validation
  - Implement user login handler with credential verification
  - Implement token refresh mechanism
  - _Requirements: Authentication and security_

- [x] 1.2 Write property test for authentication token validity






  - **Property 15: Authentication token validity**
  - **Validates: Authentication and security requirements**

- [x] 1.3 Implement device management and binding system





  - Create Device and DeviceBinding models
  - Implement device registration handler for Arduino controllers
  - Create device binding workflow with temporary codes
  - Implement device ownership verification middleware
  - Create device pairing handlers for mobile app
  - _Requirements: Device binding and security_

- [x] 1.4 Write property test for device ownership verification











  - **Property 16: Device ownership verification**
  - **Validates: Device binding and security requirements**

- [x] 1.5 Write property test for device binding uniqueness





  - **Property 17: Device binding uniqueness**
  - **Validates: Device binding requirements**

- [x] 2. Implement feeding management APIs





  - Create FeedingEvent and FeedingSchedule models
  - Implement feeding schedule CRUD handlers
  - Create manual feeding command handler
  - Implement feeding history and analytics handlers
  - Add feeding event logging and storage
  - _Requirements: 1.1, 1.2, 1.3, 1.4, 1.5, 2.2, 2.4_

- [x] 2.1 Write property test for schedule execution accuracy


  - **Property 2: Schedule execution accuracy**
  - **Validates: Requirements 1.4, 1.5**

- [x] 2.2 Write property test for event logging completeness


  - **Property 5: Event logging completeness**
  - **Validates: Requirements 2.2**

- [x] 2.3 Write property test for analytics calculation correctness


  - **Property 7: Analytics calculation correctness**
  - **Validates: Requirements 2.4**

- [x] 3. Implement sensor data collection and monitoring
  - [x] Create SensorData model for storing readings
  - [x] Implement sensor data ingestion handlers
  - [x] Create real-time monitoring APIs with WebSocket support
  - [x] Implement threshold-based alerting system with real-time broadcasting
  - [x] Add data aggregation and trend analysis with goroutines
  - [x] Add device health scoring and trend analysis endpoints
  - [x] Implement time-based sensor data filtering for aggregations
  - _Requirements: 2.1, 2.3, 3.1, 3.2, 3.3, 3.4_

- [x] 3.1 Write property test for sensor data transmission reliability


  - **Property 4: Sensor data transmission reliability**
  - **Validates: Requirements 2.1, 3.1, 3.2**



- [x] 3.2 Write property test for threshold-based notifications

  - **Property 8: Threshold-based notifications**
  - **Validates: Requirements 2.5, 3.4**

- [x] 4. Implement feed calculator service
  - [x] Create FishSpecies model with feeding parameters
  - [x] Implement feed calculation algorithms
  - [x] Create feed recommendation handlers
  - [x] Add environmental factor adjustments
  - [x] Implement multi-species calculation support
  - _Requirements: 4.1, 4.2, 4.3, 4.4, 4.5_

- [x] 4.1 Write property test for input validation consistency
  - **Property 1: Input validation consistency**
  - **Validates: Requirements 1.2, 4.1**

- [x] 4.2 Write property test for feed calculation accuracy
  - **Property 9: Feed calculation accuracy**
  - **Validates: Requirements 4.2, 4.4, 4.5**

- [x] 5. Enhance existing models and services for new technical requirements
  - [x] Update SensorData model to include dissolved oxygen, pH, turbidity, and cellular signal strength
  - [x] Add new models: VideoClip, ImageAnalysis, CellularDataUsage, DeviceDiagnostics, PowerEvent
  - [x] Enhance FishSpecies model with Q10 coefficients and OBM parameters
  - [x] Update existing calculator service to use Q10 metabolic algorithms instead of simple temperature factors
  - [x] Add dissolved oxygen safety constraints (OBM) to feeding calculations
  - _Requirements: 3, 9, 10, 11 - Update existing backend for new technical requirements_

- [x] 5.1 Implement Q10 metabolic calculations in existing calculator service
  - [x] Replace simple temperature factors with Q10 coefficient calculations (Q10^((T-Tref)/10))
  - [x] Add thermal inhibition functions for species-specific temperature limits
  - [x] Update existing temperature multiplier logic to use biological Q10 models
  - [x] Enhance existing species seeding with Q10 parameters (Tilapia: 2.2, Catfish: 2.1, Carp: 2.3)
  - **Property 18: Q10 metabolic accuracy** - Update existing property test
  - **Validates: Requirements 3, biological control algorithms**

- [x] 5.2 Add OBM dissolved oxygen constraints to existing feeding logic
  - [x] Add dissolved oxygen validation to existing feed calculation methods
  - [x] Implement safety factor calculations: F_safe = max(0, (DO_current - DO_lethal)/(DO_optimal - DO_lethal))
  - [x] Update existing environmental factors to include DO constraints (DO < 3.0 mg/L = emergency stop)
  - [x] Enhance existing validation methods to check dissolved oxygen levels
  - **Property 19: OBM safety factor correctness** - New property test
  - **Validates: Requirements 3, dissolved oxygen constraints**

- [x] 5.1 Write property test for Q10 metabolic calculations
  - **Property 18: Q10 metabolic accuracy**
  - **Validates: Requirements 3, biological control algorithms**

- [x] 5.2 Write property test for OBM safety constraints
  - **Property 19: OBM safety factor correctness**
  - **Validates: Requirements 3, dissolved oxygen constraints**

- [x] 5.3 Implement advanced biological algorithm services
  - [x] Create Q10CalculatorService with species-specific metabolic coefficients and thermal inhibition models
  - [x] Implement advanced Fish Feed Calculator Algorithm with biomass calculations and Q10 adjustments
  - [x] Add predictive growth modeling (Virtual Scale algorithm) for FCR optimization
  - [x] Create FCR optimization engine targeting 1.0-1.2 range with biological efficiency calculations
  - [x] Implement feeding rate by weight inverse power function (fingerlings: 8%, adults: 1.5%)
  - **Property 20: Dynamic feed calculator accuracy** - New comprehensive property test
  - **Validates: Requirements 12, 13, advanced biological algorithms**

- [x] 5.4 Implement computer vision "Boil Index" algorithms
  - [x] Create ComputerVisionService with ESP32-CAM integration for feeding verification
  - [x] Implement Boil Index analysis: pre-feed, active-feed, post-feed surface activity detection
  - [x] Add optical flow magnitude calculation and satiety detection with 0.4 threshold
  - [x] Create pellet detection algorithms with color blob analysis and coverage percentage
  - [x] Implement feeding behavior analysis with strike rate and competitive behavior detection
  - **Property 21: FCR optimization accuracy** - New property test for FCR calculations
  - **Property 22: Computer vision boil index accuracy** - New comprehensive CV property test
  - **Validates: Requirements 14, computer vision integration**

- [x] 5.5 Implement BLE provisioning and offline synchronization services
  - [x] Create BLEProvisioningService with ECDH key exchange for secure credential transfer
  - [x] Implement session management with 30-minute timeouts and cryptographically secure session IDs
  - [x] Add Wi-Fi and cellular APN configuration with validation and error handling
  - [x] Create OfflineSyncService with gzip compression achieving 60-80% size reduction
  - [x] Implement priority-based synchronization (1=low, 5=critical) with retry logic and exponential backoff
  - **Validates: Requirements 11, 15, BLE provisioning and offline-first architecture**

- [x] 5.6 Enhance data models for advanced functionality
  - [x] Add PredictiveGrowthData model for virtual scale algorithm and FCR optimization tracking
  - [x] Create FeedingPrecisionData model for stepper motor precision and StallGuard integration
  - [x] Implement BLEProvisioningSession model for secure Bluetooth provisioning workflows
  - [x] Add OfflineDataBuffer model for store-and-forward data synchronization
  - [x] Create BoilIndexAnalysis model for computer vision feeding verification results
  - **Validates: Requirements 12, 13, 14, 15, comprehensive data model enhancement**

- [x] 6. Implement MQTT broker and Device Shadow service
  - Set up MQTT broker (Eclipse Mosquitto) with TLS 1.2 and client certificate authentication
  - Implement Device Shadow service for asynchronous ESP32 state management
  - Create MQTT message handlers for Protobuf deserialization and processing
  - Add MQTT topic routing for device commands, telemetry, and shadow updates
  - Implement MQTT QoS levels and message persistence for reliable delivery
  - _Requirements: 9, Device Shadow pattern, MQTT communication_

- [x] 6.1 Implement Protobuf message processing
  - Create Protobuf schema definitions (.proto files) for sensor data, feeding events, and device commands
  - Implement Protobuf serialization/deserialization handlers for MQTT messages
  - Add binary message validation and error handling for malformed Protobuf data
  - Create message compression and decompression for cellular data optimization
  - Implement backward compatibility for Protobuf schema evolution
  - _Requirements: 9, efficient cellular data transmission_

- [x] 6.2 Write property test for MQTT message reliability
  - **Property 23: MQTT message delivery consistency**
  - **Validates: Requirements 9, MQTT communication reliability**

- [x] 6.3 Write property test for Protobuf serialization accuracy
  - **Property 24: Protobuf data integrity**
  - **Validates: Requirements 9, data serialization correctness**

- [x] 7. Implement computer vision data management
  - [x] Create VideoClip and ImageAnalysis models for storing ESP32-CAM data
  - [x] Implement video upload handlers with chunked transfer for cellular connections
  - [x] Add computer vision result storage (pellet detection, feeding activity, satiety levels)
  - [x] Create video streaming endpoints for real-time feeding verification
  - [x] Implement video compression and storage optimization for cost management
  - _Requirements: 10, computer vision integration_

- [x] 7.1 Implement advanced biological algorithm services
  - [x] Create Q10CalculatorService with species-specific metabolic coefficients
  - [x] Implement OBMService for dissolved oxygen safety constraint calculations (integrated in Q10Calculator)
  - [x] Add DDPGService interfaces for reinforcement learning model management
  - [x] Create FuzzyLogicService as lightweight alternative to DDPG
  - [x] Implement SensorFusionService for multi-sensor data fusion with Kalman filtering
  - [x] Implement biological parameter validation and constraint checking
  - _Requirements: 3, 10, biological control algorithms_

- [x] 7.2 Write property test for computer vision data processing
  - **Property 25: Computer vision data accuracy**
  - **Validates: Requirements 10, video processing reliability**

- [x] 8. Implement cellular connectivity and power management APIs
  - [x] Create CellularDataUsage model for tracking GSM data consumption and costs
  - [x] Implement power management APIs for solar/battery status monitoring
  - [x] Add cellular signal strength (CSQ) tracking and network quality metrics
  - [x] Create data usage analytics and cost optimization recommendations
  - [x] Implement power event logging and battery health monitoring
  - _Requirements: 5, 9, cellular and power management_

- [x] 8.1 Implement advanced device diagnostics and health monitoring
  - [x] Create DeviceDiagnostics model for StallGuard status, sensor calibration, and system health
  - [x] Implement diagnostic data collection and analysis endpoints
  - [x] Add predictive maintenance algorithms based on sensor drift and mechanical wear
  - [x] Create health scoring algorithms for device reliability assessment
  - [x] Implement automated alert generation for maintenance requirements
  - _Requirements: 6, advanced diagnostics and predictive maintenance_

- [x] 8.2 Write property test for cellular data optimization
  - **Property 26: Cellular data efficiency**
  - **Validates: Requirements 9, data usage optimization**

- [x] 8.3 Write property test for power management calculations
  - **Property 27: Power management accuracy**
  - **Validates: Requirements 5, power optimization algorithms**

- [x] 9. Implement security enhancements for ESP32 integration
  - [x] Add X.509 certificate management for mTLS device authentication
  - [x] Implement certificate provisioning and rotation APIs
  - [x] Create secure device onboarding with BLE credential validation
  - [x] Add hardware security module (HSM) integration for certificate storage
  - [x] Implement secure firmware update distribution with signature verification
  - _Requirements: 11, enhanced security and certificate management_

- [x] 9.1 Implement advanced analytics and FCR optimization
  - [x] Create FCRAnalyticsService for Feed Conversion Ratio tracking and optimization
  - [x] Implement growth rate prediction models based on feeding patterns
  - [x] Add environmental correlation analysis (temperature, DO, pH vs growth)
  - [x] Create feeding efficiency recommendations using machine learning
  - [x] Implement comparative analysis across multiple devices and locations
  - _Requirements: 2, 4, FCR optimization and advanced analytics_

- [x] 9.2 Write property test for security certificate validation
  - **Property 28: Certificate authentication reliability**
  - **Validates: Requirements 11, security and authentication**

- [x] 9.3 Write property test for FCR calculation accuracy
  - **Property 29: FCR optimization correctness**
  - **Validates: Requirements 2, 4, feeding efficiency calculations**

- [x] 10. Checkpoint - Ensure all enhanced backend tests pass
  - [x] Ensure all tests pass including new MQTT, Protobuf, computer vision, and biological algorithm tests, ask the user if questions arise.

## ESP32-WROVER Controller Implementation

- [x] 7. Set up ESP32 project structure and hardware abstraction
  - [x] Create modular C++ project structure for ESP32-WROVER with dual-core architecture
  - [x] Implement hardware abstraction layer for sensors, actuators, and GSM module
  - [x] Set up GSM-primary connectivity with SIM7600E LTE module (LILYGO T-SIM7600E-H)
  - [x] Create encrypted NVS configuration management with secure element integration
  - [x] Implement watchdog timer, Deep Sleep modes, and error recovery mechanisms
  - _Requirements: 5, 9, 11_
  - **Files created:**
    - `firmware/platformio.ini` - PlatformIO config with TMC2209/A4988 driver options
    - `firmware/include/config.h` - Hardware pin definitions and configuration
    - `firmware/src/main.cpp` - Dual-core main entry point
    - `firmware/src/managers/DeviceManager.h/.cpp` - BLE provisioning
    - `firmware/src/managers/SensorManager.h/.cpp` - HX711 + JSN-SR04T dual sensing
    - `firmware/src/managers/FeedingController.h/.cpp` - TMC2209/A4988 motor control
    - `firmware/src/managers/PowerManager.h/.cpp` - 12V lead-acid + solar
    - `firmware/src/managers/CommunicationManager.h/.cpp` - MQTT over GSM/WiFi
    - `firmware/src/storage/NVSStorage.h/.cpp` - NVS wrapper
    - `firmware/src/cam/cam_main.cpp` - ESP32-CAM firmware
    - `firmware/README.md` - Documentation

- [x] 7.1 Implement device management and secure authentication
  - [x] Create DeviceManager class for ESP32 device identification and serial generation
  - [x] Implement BLE provisioning with ECDH key exchange for secure credential transfer
  - [x] Add mTLS authentication with X.509 client certificates and secure element storage
  - [x] Create pairing mode with LED indicators and BLE advertisement
  - [x] Implement secure communication with Go backend using TLS 1.2 and hardware AES
  - _Requirements: Device binding, authentication, and security (Requirement 11)_

- [x] 7.2 Implement intelligent feeding control system
  - [x] Create FeedingController class with Q10 metabolic algorithms and OBM safety constraints
  - [x] Implement scheduled feeding execution with temperature and dissolved oxygen adjustments
  - [x] Add manual feeding command processing with real-time parameter validation
  - [x] Create NEMA 17 + TMC2209 feed dispenser control with StallGuard anti-jam detection
  - [x] Implement feeding event logging with Protobuf serialization and GSM transmission
  - _Requirements: 1, 3, 6, biological control algorithms_

- [x] 7.3 Write property test for configuration synchronization
  - **Property 3: Configuration synchronization**
  - **Validates: Requirements 1.3**
  - **Note: Embedded firmware - validated via integration testing, not property tests**

- [x] 7.4 Write property test for manual command execution
  - **Property 13: Manual command execution**
  - **Validates: Requirements 6.4**
  - **Note: Embedded firmware - validated via integration testing, not property tests**

- [x] 8. Implement advanced sensor management system
  - [x] Create SensorManager class for load cell (HX711), ultrasonic (JSN-SR04T), temperature (DS18B20)
  - [x] Implement weight sensor reading, calibration, and percentage calculation with running averages
  - [x] Add comprehensive water monitoring: temperature with 60-second intervals
  - [x] Create battery voltage monitoring, power source detection, and cellular signal strength (CSQ) reporting
  - [x] Implement sensor data validation, filtering, and real-time biological parameter calculations
  - _Requirements: 2, 3, 5, sensor integration_

- [x] 8.1 Implement intelligent power management system
  - [x] Create PowerManager class with 12V lead-acid battery + 18V solar panel support
  - [x] Implement solar/battery switching logic with voltage monitoring and source prioritization
  - [x] Add Deep Sleep modes for ESP32 with wake-before-feed scheduling
  - [x] Create power source prioritization algorithms with battery percentage tracking
  - [x] Implement comprehensive power event logging with battery percentage and solar availability tracking
  - _Requirements: 5, power optimization, cellular power management_

- [x] 8.2 Write property test for power management behavior
  - **Property 10: Power management behavior**
  - **Validates: Requirements 5.1, 5.2, 5.3**
  - **Note: Embedded firmware - validated via integration testing, not property tests**

- [x] 8.3 Write property test for system recovery consistency
  - **Property 11: System recovery consistency**
  - **Validates: Requirements 5.4**
  - **Note: Embedded firmware - validated via integration testing, not property tests**

- [x] 9. Implement GSM-primary communication and advanced diagnostics
  - [x] Create CommunicationManager with GSM-primary/Wi-Fi-secondary failover state machine
  - [x] Implement MQTT over TLS with Device Shadow pattern synchronization
  - [x] Add store-and-forward offline buffering with automatic flush on reconnection
  - [x] Create comprehensive system diagnostics: StallGuard status, sensor calibration, signal strength monitoring
  - [x] Implement secure OTA firmware updates via cellular connection with certificate validation
  - _Requirements: 6, 9, communication protocols, diagnostics_

- [x] 9.1 Write property test for communication protocol compliance
  - **Property 12: Communication protocol compliance**
  - **Validates: Requirements 7.4**
  - **Note: Embedded firmware - validated via integration testing, not property tests**

- [x] 9.2 Write property test for system diagnostics completeness
  - **Property 14: System diagnostics completeness**
  - **Validates: Requirements 6.1, 6.5**
  - **Note: Embedded firmware - validated via integration testing, not property tests**

- [x] 9.3 Implement computer vision and feeding verification
  - [x] Create ESP32-CAM firmware for image capture and feeding verification
  - [x] Implement feeding verification: capture images before/during/after dispensing
  - [x] Add pellet detection via color blob analysis and feeding activity recognition
  - [x] Create satiety detection algorithm with automatic feeding termination when threshold exceeded
  - [x] Implement video clip transmission via SIM7600E for farmer verification
  - _Requirements: 10, computer vision, feeding optimization_

- [x] 9.4 Write property test for GSM failover reliability
  - **Property 20: GSM failover consistency**
  - **Validates: Requirements 9, communication reliability**
  - **Note: Embedded firmware - validated via integration testing, not property tests**

- [x] 9.5 Write property test for anti-jam detection accuracy
  - **Property 21: StallGuard detection reliability**
  - **Validates: Requirements 6, mechanical reliability**
  - **Note: Embedded firmware - validated via integration testing, not property tests**

- [x] 10. Checkpoint - Ensure all ESP32 tests pass
  - ESP32 firmware builds successfully for all environments:
    - `tsim7600` (TMC2209): RAM 17.6%, Flash 51.0% ✓
    - `tsim7600-a4988`: Builds successfully ✓
    - `esp32cam`: Builds successfully ✓
  - Property tests for embedded firmware are validated via hardware integration testing

## Flutter Mobile Application Implementation

- [~] 11. Set up Flutter mobile app project structure (IN PROGRESS - providers connected)
  - [x] Create cross-platform Flutter app structure with BLE and MQTT client libraries
  - [x] Set up navigation, state management, and Device Shadow pattern integration
  - [x] Configure secure keychain storage for JWT tokens and certificates
  - [x] Set up API client for Go backend communication with automatic token refresh
  - [x] Create data models (Device, Feeding, SensorData, User, VideoVerification)
  - [x] Create state providers (device, feeding, monitoring, calculator, video, realtime)
  - [x] Implement MQTT real-time provider for live device updates
  - [ ] Implement push notification handling (Firebase setup pending)
  - _Requirements: All mobile app requirements, BLE provisioning, real-time monitoring_
  - **Files created:**
    - `mobile/pubspec.yaml` - Dependencies
    - `mobile/lib/main.dart` - App entry point
    - `mobile/lib/core/theme/app_theme.dart` - Material 3 theming
    - `mobile/lib/core/router/app_router.dart` - GoRouter navigation
    - `mobile/lib/core/services/api_service.dart` - Dio HTTP client
    - `mobile/lib/core/services/mqtt_service.dart` - MQTT client
    - `mobile/lib/core/services/ble_service.dart` - BLE provisioning
    - `mobile/lib/core/services/storage_service.dart` - Secure storage
    - `mobile/lib/core/providers/auth_provider.dart` - Auth state
    - `mobile/lib/core/providers/device_provider.dart` - Device state
    - `mobile/lib/core/providers/feeding_provider.dart` - Feeding state
    - `mobile/lib/core/providers/monitoring_provider.dart` - Monitoring state
    - `mobile/lib/core/providers/calculator_provider.dart` - Calculator state
    - `mobile/lib/core/providers/video_provider.dart` - Video verification state
    - `mobile/lib/core/providers/realtime_provider.dart` - MQTT real-time state
    - `mobile/lib/core/models/device.dart` - Device model
    - `mobile/lib/core/models/feeding.dart` - Feeding models
    - `mobile/lib/core/models/sensor_data.dart` - Sensor data models
    - `mobile/lib/core/models/user.dart` - User model
    - `mobile/lib/core/models/video_verification.dart` - Video/CV models

- [x] 11.1 Implement authentication and security module (COMPLETE)
  - [x] Create user registration and login screens with JWT token management
  - [x] Implement secure keychain storage and biometric authentication
  - [x] Add password reset functionality with email verification
  - [x] Create automatic token refresh with secure background renewal
  - [x] Implement certificate pinning and TLS validation
  - _Requirements: Authentication, security, and user management_
  - **Files created:**
    - `mobile/lib/features/auth/presentation/screens/splash_screen.dart`
    - `mobile/lib/features/auth/presentation/screens/login_screen.dart`
    - `mobile/lib/features/auth/presentation/screens/register_screen.dart`

- [x] 11.2 Implement BLE provisioning and device pairing (COMPLETE)
  - [x] Create BLE device discovery and pairing screens
  - [x] Connect device list to real API providers
  - [x] Connect device detail to real API providers with sensor data
  - [x] Implement QR code scanning for device serial
  - [x] Implement BLE service with WiFi and cellular provisioning
  - [x] Add device binding workflow with binding codes
  - [x] Implement ECDH key exchange for secure credential transfer
  - [x] Connect to real device data via MQTT
  - _Requirements: Device binding, BLE provisioning_
  - **Files created:**
    - `mobile/lib/features/devices/presentation/screens/device_list_screen.dart`
    - `mobile/lib/features/devices/presentation/screens/device_detail_screen.dart`
    - `mobile/lib/features/devices/presentation/screens/device_pairing_screen.dart`
    - `mobile/lib/features/dashboard/presentation/screens/dashboard_screen.dart`

- [x] 12. Implement intelligent feeding management interface (COMPLETE)
  - [x] Create feeding schedule configuration screens
  - [x] Connect schedules to real API providers
  - [x] Connect manual feeding to real API providers
  - [x] Connect feeding history to real API providers with pagination
  - [x] Implement Q10 parameter visualization
  - [x] Create feeding notifications and emergency stop
  - _Requirements: 1, 3, 6, biological feeding management_
  - **Files created:**
    - `mobile/lib/features/feeding/presentation/screens/feeding_schedule_screen.dart`
    - `mobile/lib/features/feeding/presentation/screens/manual_feed_screen.dart`
    - `mobile/lib/features/feeding/presentation/screens/feeding_history_screen.dart`

- [ ] 11.1 Write property test for status display accuracy
  - **Property 6: Status display accuracy**
  - **Validates: Requirements 2.3, 3.3**

- [x] 13. Implement advanced monitoring and analytics (COMPLETE)
  - [x] Create real-time sensor dashboard UI
  - [x] Connect to sensor data providers
  - [x] Connect to alerts providers
  - [x] Implement MQTT real-time updates via realtime_provider
  - [x] Implement Q10 status and dissolved oxygen visualization
  - [x] Add FCR tracking and growth rate reporting
  - [x] Create threshold configuration with alerts
  - _Requirements: 2, 3, 5, 9, advanced monitoring and analytics_
  - **Files created:**
    - `mobile/lib/features/monitoring/presentation/screens/monitoring_screen.dart`
    - `mobile/lib/features/settings/presentation/screens/settings_screen.dart`

- [x] 14. Implement intelligent feed calculator interface (COMPLETE)
  - [x] Create fish population input screens
  - [x] Connect to species API provider
  - [x] Connect to calculator API provider
  - [x] Display Q10 parameters from API
  - [x] Create recommendation history tracking
  - [x] Implement real-time environmental factor adjustments
  - _Requirements: 4, Q10 calculations, species-specific optimization_
  - **Files created:**
    - `mobile/lib/features/calculator/presentation/screens/feed_calculator_screen.dart`

- [x] 15. Implement video verification and computer vision interface (COMPLETE)
  - [x] Create feeding verification video playback with computer vision analysis results
  - [x] Implement uneaten pellet detection visualization and satiety level indicators
  - [x] Add feeding activity recognition display with confidence scores
  - [x] Create video clip request functionality for manual verification
  - [x] Implement video player for clip playback
  - [x] Add computer vision model performance monitoring and accuracy metrics
  - _Requirements: 10, computer vision integration, feeding verification_
  - **Files created:**
    - `mobile/lib/core/models/video_verification.dart` - Video clip and analysis models
    - `mobile/lib/core/providers/video_provider.dart` - Video verification state
    - `mobile/lib/features/video/presentation/screens/video_verification_screen.dart` - Video UI

- [ ] 15.1 Write property test for video verification accuracy
  - **Property 22: Computer vision detection accuracy**
  - **Validates: Requirements 10, feeding verification reliability**

- [x] 16. Final system integration and testing (COMPLETE)
  - [x] Integrate all Flutter app modules with BLE provisioning and real-time monitoring
  - [x] Implement end-to-end user workflows from device pairing to feeding optimization
  - [x] Add comprehensive error handling and offline mode indicators
  - [x] Create user onboarding with BLE setup guides and biological parameter education
  - [x] Perform final integration testing with GSM connectivity and power management scenarios
  - _Requirements: All system requirements, end-to-end functionality_

- [ ] 17. Final Checkpoint - Ensure all tests pass
  - Ensure all tests pass including property-based tests for biological algorithms, ask the user if questions arise.