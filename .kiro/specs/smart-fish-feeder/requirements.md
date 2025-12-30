# Requirements Document

## Introduction

The Smart Fish Feeder System is an automated aquaculture feeding solution designed for precision farming (Aquaculture 4.0) that combines solar-powered hardware with intelligent software to optimize fish feeding schedules and monitor pond conditions. The system consists of an ESP32-based hardware controller with GSM-primary connectivity, a mobile application for farmer interaction, and a Go backend service for data management and analytics.

The system addresses the critical limitations of legacy timer-based feeders by implementing adaptive biological control algorithms, GSM-primary communication topology for remote deployments, and intelligent mechanical systems with anti-jamming technology. This approach targets Feed Conversion Ratio (FCR) optimization from industry standard 1.5-1.8 down to 1.0-1.2 through real-time environmental monitoring and machine learning algorithms.

## Glossary

- **Smart_Fish_Feeder**: The complete automated feeding system including ESP32-WROVER controller, sensors, actuators, and GSM connectivity
- **Mobile_App**: The Flutter-based farmer-facing mobile application for system configuration and monitoring
- **Backend_Service**: The Go-based server handling data storage, processing, and API endpoints with MQTT/Protobuf protocols
- **ESP32_Controller**: The embedded C++ software running on ESP32-WROVER managing hardware operations with dual-core processing
- **Feed_Dispenser**: The Archimedean screw mechanism with NEMA 17 stepper motor and TMC2209 driver for precise feed release
- **Weight_Sensor**: Load cell-based hardware component measuring remaining feed quantity with calibration capabilities
- **Water_Sensor**: Multi-parameter sensor monitoring water temperature, dissolved oxygen, pH, and turbidity
- **Feeding_Schedule**: Configured times and quantities for automatic feed dispensing with Q10 metabolic adjustments
- **Feed_Calculator**: Algorithm determining optimal feed amounts using Q10 coefficients, OBM models, and species-specific parameters
- **GSM_Module**: SIM7600G LTE Cat-4 or SIM7000 LTE-M module providing primary cellular connectivity
- **Q10_Algorithm**: Biological control algorithm adjusting feeding rates based on temperature-dependent metabolic rates
- **OBM_Model**: Optimal Behavior Model incorporating dissolved oxygen constraints on fish feeding behavior
- **StallGuard**: TMC2209 sensorless stall detection technology preventing feed dispenser jams
- **DDPG**: Deep Deterministic Policy Gradient reinforcement learning algorithm for adaptive feeding optimization
- **Protobuf**: Protocol Buffers binary serialization format for efficient cellular data transmission
- **Device_Shadow**: Digital twin pattern for asynchronous device state management and synchronization

## Requirements

### Requirement 1

**User Story:** As a fish farmer, I want to configure automated feeding schedules through a mobile app, so that I can ensure consistent feeding without manual intervention.

#### Acceptance Criteria

1. WHEN a farmer opens the feeding schedule interface, THE Mobile_App SHALL display current schedule settings with time and quantity configurations
2. WHEN a farmer sets a feeding time and quantity, THE Mobile_App SHALL validate the inputs and send configuration to the Backend_Service
3. WHEN feeding schedule changes are saved, THE Backend_Service SHALL transmit updated parameters to the Arduino_Controller
4. WHEN the scheduled time arrives, THE Arduino_Controller SHALL activate the Feed_Dispenser for the specified duration
5. WHERE multiple feeding times are configured, THE Arduino_Controller SHALL execute each feeding event according to the stored schedule

### Requirement 2

**User Story:** As a fish farmer, I want to monitor feed levels and consumption patterns, so that I can plan refills and optimize feeding amounts.

#### Acceptance Criteria

1. WHEN the Weight_Sensor detects feed level changes, THE Arduino_Controller SHALL transmit weight data to the Backend_Service
2. WHEN feed is dispensed, THE Arduino_Controller SHALL record the feeding event with timestamp and quantity
3. WHEN a farmer requests feed status, THE Mobile_App SHALL display current feed weight and percentage remaining
4. WHEN generating consumption reports, THE Backend_Service SHALL calculate daily, weekly, monthly, and yearly feeding statistics
5. WHEN feed levels drop below a threshold, THE Mobile_App SHALL notify the farmer to refill the feeder

### Requirement 3

**User Story:** As a fish farmer, I want to monitor water conditions with intelligent feeding adjustments, so that I can maintain optimal fish health and feeding effectiveness while preventing hypoxic stress.

#### Acceptance Criteria

1. WHEN the Water_Sensor measures temperature, dissolved oxygen, pH, and turbidity, THE ESP32_Controller SHALL record readings at 60-second intervals with running averages
2. WHEN water parameter data is collected, THE ESP32_Controller SHALL transmit measurements to the Backend_Service using Protobuf serialization over GSM
3. WHEN a farmer requests water status, THE Mobile_App SHALL display current parameters, historical trends, and Q10-adjusted feeding recommendations
4. WHEN dissolved oxygen drops below 3.0 mg/L, THE ESP32_Controller SHALL immediately stop feeding and send CRITICAL_ALARM via MQTT priority alert
5. WHERE water conditions affect feeding, THE ESP32_Controller SHALL automatically adjust feeding schedules using Q10 metabolic factors and OBM safety constraints

### Requirement 4

**User Story:** As a fish farmer, I want a feed calculator to determine optimal feeding amounts, so that I can provide appropriate nutrition without waste.

#### Acceptance Criteria

1. WHEN a farmer inputs fish population data, THE Feed_Calculator SHALL validate species, count, and average weight parameters
2. WHEN calculating feed requirements, THE Feed_Calculator SHALL apply species-specific feeding ratios and growth factors
3. WHEN feed recommendations are generated, THE Mobile_App SHALL display daily feeding amounts and frequency suggestions
4. WHEN environmental factors change, THE Feed_Calculator SHALL adjust recommendations based on water temperature and season
5. WHERE multiple fish species are present, THE Feed_Calculator SHALL provide combined feeding recommendations

### Requirement 5

**User Story:** As a fish farmer, I want the system to operate reliably on solar power with intelligent power management, so that feeding continues during various weather conditions with optimized energy consumption.

#### Acceptance Criteria

1. WHEN 50W solar panels generate power, THE ESP32_Controller SHALL prioritize solar energy through CN3791 MPPT controller for maximum power extraction
2. WHEN solar power is insufficient, THE ESP32_Controller SHALL switch to 12V 12Ah LiFePO4 battery backup automatically with voltage monitoring
3. WHEN power levels are low, THE ESP32_Controller SHALL enter Deep Sleep mode (~10uA) and SIM7600 Sleep Mode (~2mA) while maintaining critical feeding functions
4. WHEN power is restored, THE ESP32_Controller SHALL resume normal operations, exit sleep modes, and sync buffered data via store-and-forward mechanism
5. WHERE power management is active, THE ESP32_Controller SHALL log power source changes, battery percentage, and implement PSM/eDRX cellular power saving modes

### Requirement 6

**User Story:** As a fish farmer, I want comprehensive system monitoring with anti-jamming technology and visual feedback, so that I can manage the feeder remotely and troubleshoot mechanical issues.

#### Acceptance Criteria

1. WHEN the ESP32_Controller starts up, THE system SHALL perform self-diagnostics on I2C sensors, stepper motor, GSM module, and report component status via MQTT
2. WHEN communication is established, THE ESP32_Controller SHALL maintain regular data synchronization using Device Shadow pattern with GSM-primary/Wi-Fi-secondary failover
3. WHEN feed dispenser jams occur, THE TMC2209 StallGuard SHALL detect back-EMF changes, execute anti-jam routine (reverse, agitate, retry), and alert via CRITICAL_ALARM
4. WHEN manual feeding is requested, THE Mobile_App SHALL send immediate commands via MQTT, ESP32_Controller SHALL execute feeding, and ESP32-CAM SHALL capture verification video
5. WHERE system maintenance is needed, THE Mobile_App SHALL provide diagnostic information including signal strength (CSQ), battery voltage, sensor calibration status, and troubleshooting guidance

### Requirement 7

**User Story:** As a system developer, I want well-documented and maintainable code, so that the system can be easily updated and debugged.

#### Acceptance Criteria

1. WHEN C++ code is written for the Arduino_Controller, THE code SHALL include comprehensive comments explaining functionality and hardware interactions
2. WHEN API endpoints are implemented, THE Backend_Service SHALL provide complete documentation with request/response examples
3. WHEN mobile app features are developed, THE Mobile_App SHALL include inline documentation for complex logic and user interactions
4. WHEN system integration occurs, THE components SHALL use standardized communication protocols and data formats
5. WHERE code modifications are made, THE documentation SHALL be updated to reflect current functionality

### Requirement 8

**User Story:** As a quality assurance engineer, I want comprehensive testing coverage, so that system reliability and correctness can be verified.

#### Acceptance Criteria

1. WHEN backend functionality is implemented, THE Backend_Service SHALL include unit tests for all API endpoints and business logic
2. WHEN Arduino code is developed, THE Arduino_Controller SHALL include hardware simulation tests for sensor readings and actuator control
3. WHEN mobile app features are created, THE Mobile_App SHALL include integration tests for user workflows and backend communication
4. WHEN system components interact, THE test suite SHALL verify end-to-end functionality across all system boundaries
5. WHERE data processing occurs, THE tests SHALL validate calculation accuracy and data integrity throughout the system

### Requirement 9

**User Story:** As a fish farmer operating in remote locations, I want GSM-primary connectivity with intelligent failover, so that my feeder remains connected even without Wi-Fi infrastructure.

#### Acceptance Criteria

1. WHEN the ESP32_Controller boots, THE system SHALL prioritize GSM connection via SIM7600G LTE Cat-4 module over Wi-Fi for primary connectivity
2. WHEN GSM signal strength (CSQ) is below 10 (-95dBm) or registration fails, THE system SHALL implement tiered retry with airplane mode toggle and Wi-Fi failover
3. WHEN cellular data costs are a concern, THE system SHALL use Protobuf binary serialization reducing payload size by 60-80% compared to JSON
4. WHEN network connectivity is lost, THE system SHALL buffer telemetry data in PSRAM and implement store-and-forward mechanism upon reconnection
5. WHERE dual connectivity is available, THE system SHALL use Wi-Fi for high-bandwidth operations (firmware updates, video) and GSM for critical telemetry

### Requirement 10

**User Story:** As a fish farmer, I want computer vision-based feeding verification, so that I can ensure fish are actually consuming the feed and prevent waste.

#### Acceptance Criteria

1. WHEN feeding begins, THE ESP32-CAM SHALL capture images of the water surface before, during, and after feed dispensing
2. WHEN analyzing feed consumption, THE quantized YOLOv8n model SHALL detect floating pellets and fish feeding activity with edge AI processing
3. WHEN uneaten pellets are detected above threshold, THE system SHALL immediately stop feeding and report satiety reached to prevent waste
4. WHEN feeding activity is confirmed, THE system SHALL continue dispensing remaining ration in controlled increments
5. WHERE visual verification is enabled, THE Mobile_App SHALL display feeding event videos for farmer confirmation and trust building

### Requirement 11

**User Story:** As a fish farmer, I want secure device provisioning and management, so that my feeder is protected from unauthorized access and tampering.

#### Acceptance Criteria

1. WHEN provisioning a new device, THE system SHALL use BLE (Bluetooth Low Energy) with ECDH key exchange for secure credential transfer
2. WHEN establishing backend communication, THE ESP32_Controller SHALL use mTLS with X.509 client certificates stored in encrypted NVS or secure element (ATECC608)
3. WHEN device binding occurs, THE system SHALL ensure unique device-to-user association with time-limited binding codes and attempt rate limiting
4. WHEN transmitting data, THE system SHALL use TLS 1.2 encryption with hardware-accelerated AES for all MQTT communications
5. WHERE security is compromised, THE system SHALL support secure firmware updates and certificate rotation without physical access

### Requirement 12

**User Story:** As a fish farmer, I want advanced biological control algorithms with Q10 metabolic models, so that feeding is precisely adjusted for fish metabolism and environmental conditions.

#### Acceptance Criteria

1. WHEN calculating feeding amounts, THE Q10_Calculator SHALL apply species-specific metabolic coefficients (Q10 = 2.0-2.5) using the formula: FR_adj = SFR × Q10^((T-Tref)/10) × Thermal_Inhibition(T)
2. WHEN water temperature exceeds species critical limits (>34°C for Tilapia), THE system SHALL implement thermal inhibition to prevent feeding during thermal stress
3. WHEN dissolved oxygen drops below critical thresholds, THE OBM_Model SHALL apply safety factors: F_safe = max(0, (DO_current - DO_lethal)/(DO_optimal - DO_lethal))
4. WHEN DO levels fall below 3.0 mg/L, THE system SHALL immediately stop feeding and send CRITICAL_ALARM via MQTT priority alert
5. WHERE environmental conditions are optimal, THE system SHALL target FCR optimization from industry standard 1.5-1.8 down to 1.0-1.2

### Requirement 13

**User Story:** As a fish farmer, I want predictive growth modeling and FCR optimization, so that I can achieve maximum feeding efficiency and minimize waste.

#### Acceptance Criteria

1. WHEN implementing virtual scale algorithms, THE system SHALL calculate weight gain using: ΔW = Feed_Consumed / FCR_expected and W_avg_new = W_avg_old + (ΔW / N_fish)
2. WHEN calculating feeding rates by weight, THE system SHALL use inverse power function: Rate = 8.0 × (weight^-0.3) with fingerlings eating 5-8% and adults eating 1.5-2% of body weight
3. WHEN tracking FCR performance, THE system SHALL calculate biological efficiency: Q10Factor × ThermalInhibition × OBMSafetyFactor and provide optimization recommendations
4. WHEN generating FCR suggestions, THE system SHALL provide actionable recommendations for temperature control, aeration, and environmental optimization
5. WHERE feeding efficiency is suboptimal, THE system SHALL identify improvement potential and suggest specific interventions

### Requirement 14

**User Story:** As a fish farmer, I want computer vision feeding verification with "Boil Index" analysis, so that I can ensure fish are actively consuming feed and prevent waste.

#### Acceptance Criteria

1. WHEN feeding begins, THE ESP32-CAM SHALL capture pre-feed, active-feed, and post-feed images for comprehensive analysis
2. WHEN analyzing feeding activity, THE system SHALL calculate Boil Index using optical flow magnitude and surface activity detection algorithms
3. WHEN satiety is detected (activity drops below 0.4 threshold), THE system SHALL execute early cutoff to prevent overfeeding and waste
4. WHEN uneaten pellets are detected, THE system SHALL count pellets and calculate coverage percentage using color blob detection
5. WHERE feeding verification is enabled, THE system SHALL provide confidence scores and feeding efficiency metrics to optimize future feeding events

### Requirement 15

**User Story:** As a fish farmer operating in remote locations, I want offline-first data synchronization with intelligent compression, so that my system continues operating during network outages.

#### Acceptance Criteria

1. WHEN network connectivity is lost, THE system SHALL buffer telemetry data using gzip compression achieving 60-80% size reduction
2. WHEN data synchronization resumes, THE system SHALL use priority-based sync (1=low, 5=critical) with retry logic and exponential backoff
3. WHEN processing buffered data, THE system SHALL handle different data types (sensor_data, feeding_event, alert, video_clip) with appropriate processing workflows
4. WHEN storage capacity is reached, THE system SHALL implement intelligent data pruning while preserving critical alerts and feeding events
5. WHERE cellular data costs are a concern, THE system SHALL provide data usage analytics and compression statistics for cost optimization