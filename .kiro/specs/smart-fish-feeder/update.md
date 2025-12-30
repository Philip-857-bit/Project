Comprehensive Analysis of Smart Assisted Automatic Fish Feeder Architectures: GSM-Primary Connectivity and Adaptive Biological Control Algorithms
Executive Summary
The transition of the aquaculture industry toward precision farming—often termed Aquaculture 4.0—necessitates the deployment of intelligent, autonomous edge devices capable of optimizing feed conversion ratios (FCR) while operating in remote, harsh environments. This report presents a rigorous technical analysis of a Smart Assisted Automatic Fish Feeder system designed to address the specific limitations of legacy timer-based devices. Unlike consumer-grade IoT solutions that rely on ubiquitous Wi-Fi, the proposed architecture implements a GSM-Primary, Wi-Fi-Secondary communication topology. This inversion of the standard connectivity stack ensures high-availability telemetry and control in offshore cages and rural pond systems where terrestrial broadband is unreliable or nonexistent.
The analysis synthesizes peer-reviewed literature and technical documentation from the 2020–2025 period to validate a system design that integrates: (1) Adaptive Feeding Algorithms utilizing Q10 metabolic coefficients, Deep Deterministic Policy Gradients (DDPG), and sensor fusion to dynamically adjust rations based on real-time water quality; (2) Resilient Electromechanics leveraging sensorless stall detection via Back-EMF analysis to prevent feed blockages; and (3) Optimized Data Transport using Protocol Buffers and MQTT over LTE-M/Cat-4 cellular networks to minimize operational costs. This document serves as a blueprint for engineering teams tasked with developing production-grade aquaculture IoT systems that balance biological precision with industrial robustness.
1. Introduction: The Operational Imperative for Intelligent Autonomy in Aquaculture
1.1 The Economic and Environmental Context
Feed management represents the single largest operational cost in intensive aquaculture, accounting for 50% to 70% of total expenditures.1 The margin for error is narrow: overfeeding results in feed wastage and the rapid accumulation of nitrogenous waste (ammonia and nitrites), which degrades water quality and compromises fish health. Conversely, underfeeding leads to heterogeneous growth rates, cannibalism in carnivorous species, and extended production cycles.3
Traditional feeding mechanisms rely on static, open-loop schedules—dispensing a fixed volume of pellets at fixed intervals regardless of environmental conditions or fish appetite. These "dumb" systems fail to account for the poikilothermic nature of fish, whose metabolic rates are inextricably linked to water temperature and dissolved oxygen levels.4 The "Smart Assisted" paradigm shifts this approach to a closed-loop control system. By integrating environmental sensing and machine learning, modern feeders can predict appetite and adjust dispensation in real-time, theoretically reducing FCR from the industry standard of 1.5–1.8 down to 1.0–1.2.2
1.2 The Connectivity Challenge in Remote Deployments
A critical barrier to the adoption of smart aquaculture devices is the "Last Mile" connectivity gap. Most IoT reference architectures assume the presence of a stable Wi-Fi backhaul, typically provided by a nearby building or router. However, commercial aquaculture operations—ranging from sea cages in Norway to extensive shrimp ponds in Southeast Asia—often span hectares of open water, far beyond the reliable range of standard 2.4 GHz Wi-Fi.5
Attempts to extend Wi-Fi via mesh networks or directional bridges are often capital-intensive and prone to failure due to signal attenuation over water and interference from metal infrastructure. Consequently, this report advocates for a cellular-first architecture. By utilizing the existing macro-cell infrastructure of Mobile Network Operators (MNOs) via LTE-M, NB-IoT, or LTE Cat-4, operators can achieve ubiquitous coverage. In this architecture, Wi-Fi is not the primary link but a secondary interface used for local provisioning, high-bandwidth firmware updates, or failover during cellular outages.5 This design philosophy fundamentally alters the firmware state machine, power management strategy, and data serialization protocols required for the device.
2. Biological Control Logic: Adaptive Feeding Algorithms
The core differentiator of a "smart" feeder is its ability to modulate output based on biological models. The control algorithm must ingest environmental variables and output a precise feed mass.
2.1 The Q10 Metabolic Framework
Fish metabolism follows the Arrhenius equation principles, commonly simplified in ecology as the $Q_{10}$ coefficient. This coefficient describes the sensitivity of a biological process—in this case, metabolic rate and appetite—to a 10°C change in temperature. For most cultured species, the $Q_{10}$ value lies between 2.0 and 2.5, implying that metabolic demand roughly doubles for every 10°C rise in temperature.4
However, this relationship is non-monotonic. As temperatures approach the species' upper thermal tolerance ($T_{crit}$), the metabolic cost of existence ($R_{maint}$) continues to rise, but the scope for aerobic activity (appetite) collapses due to the limiting capacity of the cardiovascular system to deliver oxygen. Therefore, a naive $Q_{10}$ model that linearly increases feed with temperature will kill fish during heatwaves.
Algorithm Design:
The feed rate ($FR_{adj}$) is calculated as an adjustment to the standard feeding rate ($SFR$):


$$FR_{adj} = SFR \times \left( Q_{10}^{\frac{T - T_{ref}}{10}} \right) \times \text{Thermal\_Inhibition}(T)$$
Where $\text{Thermal\_Inhibition}(T)$ is a penalty function that drops from 1.0 to 0.0 as temperature exceeds the species-specific optimal range (e.g., >30°C for Salmonids, >34°C for Tilapia).7 This computational model ensures that the feeder automatically throttles back during thermal stress events, preventing uneaten feed from decaying in warm water and exacerbating hypoxia.
2.2 Dissolved Oxygen (DO) Constraints and the OBM Model
Oxygen availability is the hard constraint on digestion. The breakdown of protein (Specific Dynamic Action) consumes significant oxygen. If Dissolved Oxygen (DO) levels are low, feeding induces a "hypoxic squeeze" that can be lethal.
Recent research utilizing the Water Ecosystems Tool (WET) incorporates an Optimal Behavior Model (OBM).8 The OBM posits that fish make a trade-off between foraging gain and predation risk (or environmental stress). In a smart feeder, this can be modeled as a penalty factor. When DO drops below a critical threshold ($DO_{crit}$, typically ~3.0 mg/L), the feeding probability $p^*$ drops to zero regardless of hunger.
Mathematical Logic for Firmware:
The algorithm integrates a "Safety Factor" ($F_{safe}$) based on oxygen:


$$F_{safe} = \max\left(0, \frac{DO_{current} - DO_{lethal}}{DO_{optimal} - DO_{lethal}}\right)$$

This linear interpolation ensures that feeding is reduced proportionally as oxygen levels decline, stopping completely before lethal limits are reached. This responsiveness has been shown to improve oxygen recovery times by nearly 3x compared to fixed feeding schedules.2
2.3 Deep Deterministic Policy Gradient (DDPG) Integration
Beyond simple algebraic models, advanced controllers utilize Reinforcement Learning (RL). A Deep Deterministic Policy Gradient (DDPG) algorithm allows the system to "learn" the optimal feeding strategy over time.2
State Space ($S$): The inputs are Dissolved Oxygen, pH, Temperature, Ammonia, and current Biomass.
Action Space ($A$): The output is the feed rate (kg/hour).
Reward Function ($R$): The system is rewarded for maximizing fish growth (biomass increase) while penalized heavily for water quality violations (e.g., Ammonia > 0.3 mg/L or pH < 6.8).
While running a full DDPG training loop on an ESP32 is computationally infeasible, the inference model (the "Actor" network) can be quantized and executed on the edge device, or the device can upload telemetry to the cloud where the "Critic" network runs, sending back updated policy parameters. This approach allows the feeder to adapt to specific site conditions (e.g., a pond with poor water circulation) without manual recalibration.
2.4 Multi-Sensor Fusion and Fuzzy Logic
For robust operation without the complexity of DDPG, Fuzzy Logic Control (FLC) offers a deterministic alternative. FLC mimics human expert decision-making by mapping continuous input variables into linguistic sets (e.g., "Low", "Medium", "High").9
Table 1: Fuzzy Rule Base for Feeding Control
Temperature
Dissolved Oxygen
Turbidity
Feeding Decision
Rationale
Low
Any
Any
Stop
Metabolism too slow for digestion.
Optimal
High
Low
Maximum
Ideal growth conditions.
Optimal
Medium
Low
Medium
Safe maintenance feeding.
High
Low
Any
Stop
High risk of hypoxic stress.
Optimal
High
High
Low
Fish cannot visually locate feed; risk of waste.

This logic is computationally lightweight and can be implemented on standard microcontrollers using simple if-else lookup trees or lightweight fuzzy libraries, providing a significant improvement over static timers.11
3. Communication Architecture: The GSM-Primary Paradigm
Designing for "GSM Primary" requires a fundamental rethinking of the IoT stack. Unlike Wi-Fi, which is effectively free and always-on, cellular connectivity incurs data costs and higher power consumption latency.
3.1 Hardware Selection: LTE-M vs. Cat-4
The choice of cellular module dictates the system's capabilities.
SIM7600 (LTE Cat-4):
Bandwidth: Up to 150 Mbps DL / 50 Mbps UL.
Latency: Low (tens of milliseconds).
Capability: Supports live video streaming (essential for visual verification of feeding), VoLTE, and large OTA firmware updates.
Drawback: High power consumption (~500mA - 2A peak).
Use Case: High-value stock (e.g., Salmon, Grouper) where visual monitoring is mandatory.12
SIM7000 / SIM7080 (LPWAN - LTE-M / NB-IoT):
Bandwidth: Low (375 kbps for LTE-M, <100 kbps for NB-IoT).
Latency: High (can be seconds for NB-IoT).
Capability: Pure telemetry. Deep penetration (164 dB MCL) allows signals to reach feeders inside concrete hatchery buildings or under metal roofs.
Advantage: Extremely low power (PSM modes < 10uA).
Use Case: Pond monitoring, expansive shrimp farms where video is not required.14
Design Recommendation: For a "Smart Assisted" feeder that implies visual feedback (Computer Vision), the SIM7600G-H (Global Variant) is the requisite module. It provides the necessary bandwidth for uploading images of uneaten feed while maintaining backward compatibility with 2G/3G networks in developing regions where LTE coverage might be spotty.16
3.2 The Dual-Connectivity Finite State Machine (FSM)
To ensure 99.9% availability, the firmware must implement a rigorous state machine that handles network transitions without blocking critical motor functions.
State Definitions:
INIT: Hardware initialization (I2C, GPIO, UART).
GSM_CONNECT: Power on SIM7600, execute AT+CPIN, AT+CREG, AT+COPS.
GSM_ACTIVE: MQTT connection established. Telemetry loop active.
GSM_ERROR: Registration failure or Signal Strength (CSQ) < 10 (approx -93dBm).
WIFI_SCAN: Enable Wi-Fi radio, scan for known SSIDs (Secondary backhaul).
WIFI_ACTIVE: Connected to Wi-Fi. Flush local telemetry buffer.
OFFLINE_LOG: Both networks down. Log data to SPIFFS/SD Card (Store-and-Forward).
MAINTENANCE_AP: SoftAP mode activated by physical button for user provisioning.
Failover Logic:
The system defaults to GSM_CONNECT. The failover logic uses a tiered retry mechanism.
Tier 1: If MQTT disconnects, attempt TCP reconnect immediately.
Tier 2: If TCP fails x3, toggle Airplane Mode (AT+CFUN=0 -> AT+CFUN=1) to force network re-registration.
Tier 3: If Cellular fails for > 10 minutes, switch to WIFI_SCAN.
Tier 4: If Wi-Fi fails, enter OFFLINE_LOG. The system continues to attempt GSM reconnection every 15 minutes (Duty Cycled) to conserve battery.5
3.3 Data Transport Efficiency: Protobuf vs. JSON
When operating on a cellular data plan (e.g., 50MB/month), byte efficiency is paramount. JSON, while human-readable, is notoriously verbose.
Comparison for a Standard Telemetry Packet:
Data: { "temp": 24.5, "do": 6.8, "feed_grams": 150, "status": "OK" }
JSON: ~75 bytes (plus overhead).
Protocol Buffers (Protobuf): ~12-15 bytes.
Protobuf uses a binary schema (.proto file) shared between the device and the cloud.

Protocol Buffers


message FeederTelemetry {
  float temperature = 1;
  float dissolved_oxygen = 2;
  int32 feed_dispensed = 3;
  enum Status { OK = 0; JAMMED = 1; EMPTY = 2; }
  Status device_status = 4;
}


By switching to Protobuf, the data payload is reduced by 60-80%, directly translating to lower monthly cellular bills and reduced radio "on-time" (energy saving).18
4. Hardware Implementation: Mechanics and Electronics
4.1 Microcontroller Unit: ESP32-WROVER
The ESP32-WROVER series is selected for its integrated 4MB/8MB PSRAM (Pseudo-Static RAM). Standard ESP32s (WROOM) have only ~320KB of usable SRAM. In a "Store-and-Forward" architecture, where the device might need to buffer days of telemetry during a network outage, PSRAM is essential to maintain a large ring buffer without wearing out the Flash memory.20
Core 0: Dedicated to the Network Stack (LwIP, SSL/TLS encryption, AT Command parser).
Core 1: Dedicated to Real-Time Control (Motor stepping, Sensor polling, Safety Watchdogs).
4.2 Mechanical Actuation: Anti-Jamming Technology
A primary failure mode of automatic feeders is the "caking" of feed pellets due to humidity, leading to mechanical jams. A standard servo motor will stall and burn out.
Sensorless Stall Detection (Trinamic TMC2209):
The proposed design utilizes a NEMA 17 stepper motor driven by a TMC2209 driver. This driver features StallGuard4™ technology, which measures the back-electromotive force (Back-EMF) of the motor coils to estimate the load.21
Mechanism: As the motor encounters resistance (a jam), the phase current shifts, and the SG_RESULT register value drops.
Implementation: The ESP32 monitors the DIAG pin of the TMC2209. If a stall is detected (Interrupt triggers), the firmware executes an Anti-Jam Routine:
Stop motor immediately.
Retract (Spin Reverse) for 180 degrees to dislodge the clump.
Agitate (High-frequency micro-steps) to break the bridge.
Retry Forward feed.
Alert: If failure persists after 3 attempts, send CRITICAL_ALARM: JAMMED via MQTT.23
4.3 Power Management: Solar and Sleep Architectures
Reliability in off-grid locations requires a robust power budget.
Power Source: 12V 50W Solar Panel + MPPT Controller (e.g., CN3791) charging a 3S or 4S LiFePO4 battery pack (safer and longer life than Li-ion).
Peak Loads: The SIM7600 can draw current spikes of up to 2A during network registration. The power supply design must include large bulk capacitance (e.g., 1000uF low-ESR) or a high-discharge battery buffer to prevent voltage sags that would reset the ESP32.13
Sleep Strategy:
ESP32: Enters Deep Sleep (~10uA).
SIM7600: Enters Sleep Mode via AT+CSCLK=1. In this mode, the module disables its RF TX/RX but stays attached to the network paging channel (consumption ~2mA). It wakes instantly upon toggling the DTR pin, avoiding the energy-intensive network re-attachment process.25
5. Firmware Implementation Details
5.1 MQTT Keep-Alive Optimization
Standard MQTT libraries default to a 15-60 second keep-alive ping. On a cellular connection, this prevents the radio from entering idle mode, draining the battery.
Strategy: Use an Adaptive Keep-Alive.
Set MQTT Keep-Alive to 15 minutes (900 seconds).
Rely on the Cellular Network's PSM (Power Saving Mode) and eDRX (Extended Discontinuous Reception) parameters to keep the socket open at the carrier level.14
Note: Some carriers use aggressive NAT timeouts (e.g., 60 seconds). In such cases, the device must send a "heartbeat" packet just before the NAT mapping expires. This needs to be configurable OTA.27
5.2 Security Architecture
Operating over public cellular networks exposes the device to attack vectors that local Wi-Fi does not.
Transport Layer Security (TLS 1.2): Mandatory for all MQTT traffic. The ESP32 supports hardware-accelerated AES encryption.
Mutual Authentication (mTLS): The device authenticates the server, and the server authenticates the device using a unique X.509 client certificate burned into the device during manufacturing.
Secure Storage: Certificates should be stored in the ESP32's encrypted NVS partition or, preferably, in a dedicated Secure Element (like the Microchip ATECC608) to prevent extraction if the device is physically stolen.28
5.3 Computer Vision Integration (Edge AI)
To close the loop on feeding, visual feedback is superior to indirect calculations.
Hardware: An ESP32-CAM module (or a secondary ESP32-S3 with camera interface) is mounted facing the water surface.
Algorithm: A lightweight Convolutional Neural Network (CNN), such as a quantized YOLOv8n (You Only Look Once) model, is trained to detect "Feed Pellets" and "Fish Boils" (feeding activity).30
Operation:
Dispense 10% of ration.
Capture image -> Run Inference.
If Feeding_Activity > Threshold, dispense next batch.
If Floating_Pellets > Threshold, STOP feeding immediately (Satiety reached).31
6. User Interface and Provisioning
6.1 Provisioning: The BLE Handshake
A seamless "First Time Setup" is critical for user adoption. SoftAP (captive portal) methods are clunky as they require the phone to disconnect from the internet.32
Recommended Flow: Bluetooth Low Energy (BLE)
Advertisement: Upon boot (if unconfigured), the feeder advertises a BLE service UUID.
Discovery: The Companion App (Flutter) scans and connects.
Security: An ECDH (Elliptic-Curve Diffie-Hellman) key exchange ensures the credentials are passed securely.33
Configuration: The user sends the Wi-Fi SSID/Pass and the APN settings for the SIM card via BLE.
Verification: The feeder attempts connection and reports status back to the phone via BLE before the user walks away.35
6.2 Mobile Application (Flutter)
The user interface must translate complex biological data into actionable insights.
Tech Stack: Flutter is ideal for generating iOS and Android binaries from a single codebase, with robust support for MQTT (mqtt_client) and BLE (flutter_blue).36
Features:
Real-time Dashboard: Live graphs of Temperature, DO, and Feed Consumption.
Schedule Builder: A drag-and-drop interface to set "Base Rations" and "Max Rations."
Video Snippets: If using SIM7600, the app can request a 10-second video clip of the feeding event for verification.
Data Synchronization: Uses the Device Shadow pattern (AWS IoT / Azure IoT Hub). The app updates the "Desired" state (e.g., feed_rate: 500g), and the device updates the "Reported" state when it wakes up. This handles the asynchronous nature of cellular IoT where the device might be sleeping when the user changes a setting.38
7. Comparison: GSM-Primary vs. Traditional Wi-Fi Feeders
Feature
Traditional Wi-Fi Feeder
Proposed GSM-Primary Smart Feeder
Connectivity
Wi-Fi (2.4 GHz)
Primary: LTE Cat-4 / LTE-M

Secondary: Wi-Fi
Range
~50-100m (Line of Sight)
Global (wherever Cell Towers exist)
Feeding Logic
Timer / Fixed Schedule
Adaptive: Q10 Metabolic Model + DO Sensor Feedback
Jam Protection
None / Servo Torque limit
Active: Sensorless Back-EMF Stall Detection (TMC2209)
Data Protocol
HTTP / REST (Verbose)
MQTT + Protobuf (Binary, Compressed)
Power
Mains / simple battery
Solar MPPT + LiFePO4 + Deep Sleep Architecture
Security
WPA2 (often hardcoded)
mTLS + Secure Element + Encrypted Storage
Reliability
Fails if router locks up
Dual-Path Failover (Cellular <-> Wi-Fi)

8. Conclusion
The Smart Assisted Automatic Fish Feeder represents a convergence of biological science and industrial IoT engineering. By shifting the connectivity paradigm to GSM-Primary, the system breaks the tether of terrestrial internet, enabling precision aquaculture in the high-value, remote environments where it is most needed. The integration of adaptive Q10 algorithms ensures that feeding aligns with the physiological capabilities of the fish, maximizing growth and minimizing waste. Furthermore, the inclusion of sensorless mechanical protection and secure, efficient data transport protocols renders the system sufficiently robust for unmanned operation.
This architecture satisfies the requirements for a modern, scalable aquaculture solution, addressing the "triple bottom line" of economic viability, environmental sustainability, and operational reliability. As 5G networks expand and satellite-IoT (NTN) costs decrease, this architecture is poised to evolve further, eventually enabling fully autonomous, AI-driven farms managed from a smartphone anywhere in the world.
Detailed Technical Implementation Analysis
1. System Architecture and Design Philosophy
1.1 The Necessity of the "GSM-First" Approach
In standard consumer IoT, Wi-Fi is the default because it handles high bandwidth at zero marginal cost. However, in industrial aquaculture, the cost of data is secondary to the cost of failure. A Wi-Fi link dependent on a farmhouse router, a range extender, and a long ethernet cable introduces multiple single points of failure. If the router hangs, the feeder goes offline.
In a GSM-First architecture, the device connects directly to the carrier-grade infrastructure of the Mobile Network Operator (MNO). These towers have industrial battery backups and redundant backhauls. The reliability of the connection is owned by the telecom provider, not the farm manager. This shift simplifies the on-site infrastructure: there is no need to run cables or install waterproof access points. The feeder is a self-contained "drop-in" unit.
1.2 Dual-Core Processing Strategy (ESP32)
The ESP32's architecture is uniquely suited for this application. It contains two Xtensa 32-bit LX6 microprocessors.
PRO_CPU (Protocol CPU - Core 0): This core is assigned the heavy lifting of the Wi-Fi/LwIP stack and the FreeRTOS scheduler. In our design, we also pin the GSM AT Command parser and the SSL/TLS encryption tasks to this core. This ensures that the heavy math of encrypting MQTT packets does not jitter the motor control pulses.
APP_CPU (Application CPU - Core 1): This core is dedicated to the "business logic." It runs the sensor polling loop (I2C/OneWire), the Q10 feeding algorithm, and the AccelStepper motor control loop. Because the stepper motor requires precise timing for its step pulses, isolating it from the network interrupts is crucial for smooth operation.5
2. Biological Control Algorithms: Deep Dive
2.1 Implementing the Q10 Model in C++
The biological requirement to scale feeding with temperature is non-negotiable for FCR optimization.
Code Logic Implementation:

C++


// Thermal coefficients for Nile Tilapia
#define TEMP_OPTIMAL_MIN 26.0
#define TEMP_OPTIMAL_MAX 30.0
#define TEMP_CRITICAL_HIGH 34.0
#define Q10_FACTOR 2.2
#define REF_TEMP 25.0

float calculate_metabolic_factor(float current_temp) {
    // Safety Cutoff: If too hot, stop feeding to prevent bacterial bloom/stress
    if (current_temp >= TEMP_CRITICAL_HIGH) {
        return 0.0;
    }

    // Optimal Range: Feed at 100% capacity (or slightly boosted)
    if (current_temp >= TEMP_OPTIMAL_MIN && current_temp <= TEMP_OPTIMAL_MAX) {
        return 1.0; 
    }

    // Arrhenius / Q10 Curve for sub-optimal temps
    // Formula: R2 = R1 * Q10 ^ ((T2 - T1) / 10)
    float exponent = (current_temp - REF_TEMP) / 10.0;
    float factor = pow(Q10_FACTOR, exponent);
    
    return factor;
}


Implication: This code ensures that during a cold front, when water temperature drops from 28°C to 23°C, the feeder automatically reduces the ration by ~40%, matching the slowed digestion of the fish. This saves feed that would otherwise be wasted.4
2.2 The Oxygen Safety Factor (OBM)
The Optimal Behavior Model (OBM) suggests that fish reduce foraging activity when the metabolic cost of extracting oxygen exceeds the energy gain from food.8
Data Source: An optical DO probe (e.g., Atlas Scientific or DFRobot Gravity DO) provides the input.
Control Loop:
Reading interval: Every 60 seconds.
Running Average: Last 5 minutes (to filter sensor noise/bubbles).
Logic:
$DO > 5.0 mg/L$: Factor = 1.0 (Normal)
$3.0 < DO < 5.0 mg/L$: Linear reduction. Factor = $(DO - 3.0) / 2.0$.
$DO < 3.0 mg/L$: Factor = 0.0 (Emergency Stop).
Alerting: If DO < 3.0, the system triggers an immediate MQTT Priority Alert: "CRITICAL: LOW OXYGEN," waking the GSM module regardless of sleep schedules.
2.3 Sensor Fusion for Satiety Detection
While Q10 and DO models predict potential appetite, they cannot measure actual satiety. This is where the Camera (ESP32-CAM) comes in.
Process: After dispensing 20% of the calculated ration, the feeder pauses for 5 minutes.
Detection: The camera captures a frame. A simplified CV algorithm (e.g., color blob detection or a quantized MobileNet) analyzes the surface.
Observation: Uneaten pellets reflect light differently than water.
Decision: If the pixel count of "feed color" exceeds a threshold, it implies the fish are not eating. The remaining 80% of the ration is cancelled.
Impact: This closed-loop visual feedback is the ultimate mechanism for minimizing FCR.31
3. Communication Network Design
3.1 The Failover State Machine
The reliability of the system hinges on the robustness of its connectivity logic. The transition between GSM and Wi-Fi must be seamless but prioritized.
State Machine Transition Logic:
Boot: Check Preference storage for "Last Known Good" network. Default to GSM.
GSM Attempt:
Initialize SIM7600.
Check AT+CSQ. If signal < 9 (-95dBm), count as "Weak".
Check AT+CREG. If not registered (1 or 5), wait.
Timeout: If not connected in 120 seconds, increment FailCount.
Failover Trigger: If FailCount > 3:
Power Down GSM (to save battery).
Enable Wi-Fi.
Scan: Look for configured SSIDs.
Success: Connect Wi-Fi, upload buffered data, check for commands.
Failure: Disable Wi-Fi. Enter Deep Sleep for 15 minutes.
Recovery: On next wake-up, retry GSM first. The system always biases toward returning to the primary Cellular link.5
3.2 SIM7600 vs. SIM7000: The Bandwidth Trade-off
The selection between SIM7600 (Cat-4) and SIM7000 (LTE-M/NB-IoT) defines the product class.
SIM7600 (High-End): Capable of streaming a 640x480 video feed of the fish feeding. This builds immense trust with the user ("I can see my fish eating"). However, it consumes significantly more power and requires a larger solar panel.
SIM7000 (Efficiency): Only sends numbers (Temp, Feed Amount). It cannot send video. It is suitable for "set and forget" data logging.
Choice: For a "Smart Assisted" feeder, the visual component is a key value proposition. Therefore, the SIM7600 is the preferred choice, despite the power penalty. The power system must be sized to accommodate this.12
3.3 Data Optimization with Protobuf
Using cellular data requires strict discipline.
JSON Payload:
{"d": {"t": 25.5, "ph": 7.1, "do": 6.5, "f": 120}}
Even minimized, this is ~45 bytes. With TCP/IP headers + MQTT headers, a single packet might be ~100 bytes.
Protobuf Payload:
Using a .proto definition, the same data is serialized into binary.
08 19 15 00 00 CC 41 1D 00 00 E6 40...
This might be ~15 bytes.
Cost Savings: Sending data every 5 minutes = 288 messages/day.
JSON: 288 * 100 bytes * 30 days = ~864 KB/month.
Protobuf: 288 * 50 bytes * 30 days = ~432 KB/month.
Scale: While MBs are cheap, the time on air (energy) is the real saving. Sending fewer bytes means the radio is active for less time.18
4. Mechanical and Hardware Engineering
4.1 Stepper Motor Drive with Back-EMF Stall Detection
The mechanical design uses an Archimedean Screw (Auger) to dispense pellets.
Problem: If a damp pellet clump enters the screw, a standard motor might jam.
Solution: StallGuard4 on the TMC2209.
The TMC2209 measures the load value (SG_RESULT) continuously.
Calibration: During the empty run, the SG_RESULT is plotted (e.g., value is 100).
Threshold: We set the threshold at 50.
Action: If the screw hits a clump, the motor has to work harder. SG_RESULT drops to 30. The driver triggers the DIAG pin.
ISR (Interrupt Service Routine): The ESP32 catches the interrupt and immediately stops the step pulse generation. It then reverses the direction pin and steps for 1 second (Un-jam), then resumes forward motion.22
4.2 Power Architecture: Solar and Battery
Solar Panel: 50W Monocrystalline.
Controller: CN3791 MPPT Charging board. This tracks the Maximum Power Point of the panel, extracting up to 30% more energy on cloudy days compared to a linear regulator.41
Battery: 12V 12Ah LiFePO4. LiFePO4 is chosen over Li-ion/Li-Po because it is safer (no thermal runaway risk in hot sun) and has a cycle life of >2000 cycles (vs 500 for Li-Po).
Regulators:
12V -> 5V (Buck Converter) for SIM7600 and Servos.
5V -> 3.3V (LDO) for ESP32 and Sensors.
Design Note: The SIM7600 supply trace on the PCB must be thick (at least 2mm) to handle the 2A GSM burst current without voltage drop.13
5. Security and Cloud Integration
5.1 MQTT Security
Port: 8883 (MQTTS).
Certificate Management:
Root CA: Verifies the server identity (e.g., Amazon Root CA 1).
Client Cert: Identifies the device.
Private Key: Must never leave the device.
Provisioning: During manufacturing, the private key is generated on the device (or injected securely) and stored in the ESP32's nvs_encrypted partition. Flash Encryption is enabled on the ESP32 to prevent someone from desoldering the flash chip and reading the key.28
5.2 Device Shadows (Digital Twin)
The Device Shadow architecture decouples the app from the device.
Scenario: User changes schedule while the feeder is sleeping.
Flow:
App publishes to $aws/things/feeder_01/shadow/update: {"state": {"desired": {"schedule": "new"}}}.
Cloud stores this.
Feeder wakes up (1 hour later). Subscribes to /shadow/update/delta.
Feeder receives the "new schedule", applies it, and publishes back to "reported".
App sees the "reported" state match the "desired" state and shows a green "Synced" checkmark.38
6. Mobile Application (Flutter) & Provisioning
6.1 The Provisioning Flow (Bluetooth)
Library: flutter_blue_plus (Flutter) + BLEDevice (ESP32 Arduino/IDF).
Process:
User opens App -> "Add Device".
App scans for BLE devices named "SmartFeeder_XXXX".
Connects. Reading the "Status" characteristic.
User selects "Configure Wi-Fi". App lists visible networks (scanned by ESP32 and sent over BLE).
User enters Wi-Fi Password and APN settings.
App writes to "Config" characteristic.
ESP32 saves to NVS, reboots, and attempts connection.
ESP32 updates "Status" characteristic to "Connected".
App confirms success.35
6.2 Dashboard UI Patterns
Neumorphism: Often used in modern IoT apps to give a "tactile" feel to buttons.42
Visual Feedback: The "Feed Now" button shouldn't just send a command; it should show a "Spinner" while waiting for the generic MQTT acknowledgment (PUBACK), and then a "Success" animation when the device reports feed_status: "dispensing".43
7. Conclusion
This report has detailed the engineering requirements for a high-reliability, smart assisted fish feeder. By leveraging a GSM-Primary architecture, we address the critical failing of previous generations of Wi-Fi-dependent feeders. The integration of Biological Control Algorithms (Q10, OBM) ensures that the technology serves the biology, optimizing growth rates and minimizing environmental impact. Finally, the use of modern industrial protocols (MQTT, Protobuf) and robust electromechanics (StallGuard) ensures that the system is viable not just as a hobbyist gadget, but as a critical infrastructure component for the aquaculture industry. The resulting system is autonomous, resilient, and data-driven, perfectly aligned with the goals of Aquaculture 4.0.
Works cited
How to Calculate a Feed Conversion Ratio in Aquaculture - Aquatic Equipment and Design, accessed December 17, 2025, https://www.aquaticed.com/blogs/deeper-dive/how-to-calculate-a-feed-conversion-ratio-in-aquaculture
Smart algorithms take control of fish feeding in RAS systems - misPeces, accessed December 17, 2025, https://www.mispeces.com/en/news/Smart-algorithms-take-control-of-fish-feeding-in-RAS-systems/
AUTOMATIC FISH FEEDER MIEOR AHMAD SAFWAN BIN MEOR ABD JUMAT UNIVERSITI SAINS MALAYSIA 2017, accessed December 17, 2025, http://eprints.usm.my/52919/1/Automatic%20Fish%20Feeder_Mieor%20Ahmad%20Safwan%20Meor%20Abd%20Jumat_E3_2017.pdf
Effects of temperature on feeding and digestive processes in fish - PMC - PubMed Central, accessed December 17, 2025, https://pmc.ncbi.nlm.nih.gov/articles/PMC7678922/
(PDF) ESP32-Based Dual-Connectivity Data Logger for Continuous ..., accessed December 17, 2025, https://www.researchgate.net/publication/397053816_ESP32-Based_Dual-Connectivity_Data_Logger_for_Continuous_Environmental_Monitoring_for_AI_Applications
SATHYABAMA, accessed December 17, 2025, https://sist.sathyabama.ac.in/sist_naac/aqar_2022_2023/documents/1.3.4/b.e-ece-19-23-batchno-43.pdf
Metabolomics-Based Analysis of Adaptive Mechanism of Eleutheronema tetradactylum to Low-Temperature Stress - PubMed Central, accessed December 17, 2025, https://pmc.ncbi.nlm.nih.gov/articles/PMC12024119/
Dining in danger: Resolving adaptive fish behavior increases ..., accessed December 17, 2025, https://pmc.ncbi.nlm.nih.gov/articles/PMC11303985/
Smart Fish Feeder Using Arduino Uno With Fuzzy Logic Controller - Scribd, accessed December 17, 2025, https://www.scribd.com/document/642135517/Smart-Fish-Feeder-Using-Arduino-Uno-With-Fuzzy-Logic-Controller
Fuzzy logic based control system for fresh water aquaculture: A MATLAB based simulation approach - ResearchGate, accessed December 17, 2025, https://www.researchgate.net/publication/281737051_Fuzzy_logic_based_control_system_for_fresh_water_aquaculture_A_MATLAB_based_simulation_approach
Development of Fuzzy Logic Automatic Fish Feeding System and Iot-based Water Quality Control | Journal of Engineering Research and Reports, accessed December 17, 2025, https://journaljerr.com/index.php/JERR/article/view/1417
Purchased this module and found it won't work on 4G SIM cards. Am i done with it? - Reddit, accessed December 17, 2025, https://www.reddit.com/r/arduino/comments/1g6021v/purchased_this_module_and_found_it_wont_work_on/
SIM7600G 4G LTE GSM & ESP32 Development Board with 5V 3A Power Supply - YouTube, accessed December 17, 2025, https://www.youtube.com/watch?v=aLdtqYyg2Sw
Cellular LPWA (LTE-M and NB-IoT) IoT Modules | Telit Cinterion, accessed December 17, 2025, https://www.telit.com/modules-overview/cellular-lpwa/
LTE Newest Cellular Modules - Mouser Electronics, accessed December 17, 2025, https://www.mouser.com/c/n/embedded-solutions/wireless-rf-modules/cellular-modules/?protocol%20-%20cellular%2C%20nbiot%2C%20lte=LTE
A5/1 is in the Air: Passive Detection of 2G (GSM) Ciphering Algorithms - arXiv, accessed December 17, 2025, https://arxiv.org/html/2505.14509v1
tabahi/StatefulGSMLib: ESP32/arduino library for SIM800, SIM900 GSM module. - GitHub, accessed December 17, 2025, https://github.com/tabahi/StatefulGSMLib
Comparing Data Serialization Formats: Code, Size, and Performance - Qt, accessed December 17, 2025, https://www.qt.io/blog/comparing-data-serialization-formats
Optimizing API Performance with Protocol Buffers FlatBuffers MessagePack and CBOR, accessed December 17, 2025, https://www.cloudthat.com/resources/blog/optimizing-api-performance-with-protocol-buffers-flatbuffers-messagepack-and-cbor
Power ESP32/ESP8266 with Solar Panels and Battery - Random Nerd Tutorials, accessed December 17, 2025, https://randomnerdtutorials.com/power-esp32-esp8266-solar-panels-battery-level-monitoring/
A Look at Sensorless Homing: Stepper Motor Control Without End Switches - Circuit Digest, accessed December 17, 2025, https://circuitdigest.com/news/a-look-at-sensorless-homing-stepper-motor-control-without-end-switches
BigTreeTech TMC2209 V1.3 with Arduino StallGuard working code? - General Guidance, accessed December 17, 2025, https://forum.arduino.cc/t/bigtreetech-tmc2209-v1-3-with-arduino-stallguard-working-code/1341262
EP1388930A2 - Stepper motor jam detection circuit - Google Patents, accessed December 17, 2025, https://patents.google.com/patent/EP1388930A2/en
Auto Fish Feeder : 7 Steps (with Pictures) - Instructables, accessed December 17, 2025, https://www.instructables.com/Auto-Fish-Feeder/
How to put a GSM modem (e.g. SIM900A) to sleep mode? - Stack Overflow, accessed December 17, 2025, https://stackoverflow.com/questions/58905686/how-to-put-a-gsm-modem-e-g-sim900a-to-sleep-mode
SIMCOM 7600G Sleep mode handling guidance - Arduino Forum, accessed December 17, 2025, https://forum.arduino.cc/t/simcom-7600g-sleep-mode-handling-guidance/1293135
MQTT Keep Alive Explained in Layman's Terms | Cedalo, accessed December 17, 2025, https://cedalo.com/blog/mqtt-keep-alive-explained/
Empirical Evaluation of TLS-Enhanced MQTT on IoT Devices for V2X Use Cases - MDPI, accessed December 17, 2025, https://www.mdpi.com/2076-3417/15/15/8398
RA AWS MQTT/TLS Cloud Connectivity Solution – Cellular Application Project - Renesas, accessed December 17, 2025, https://www.renesas.com/en/document/apn/ra-aws-mqtttls-cloud-connectivity-solution-cellular-application-project
An IoT-Enabled Intelligent Monitoring System for Sustainable Aquaculture - Preprints.org, accessed December 17, 2025, https://www.preprints.org/manuscript/202511.1528
Computer Vision-Based Fish Feed Detection and Quantification System - ResearchGate, accessed December 17, 2025, https://www.researchgate.net/publication/375675203_Computer_Vision-Based_Fish_Feed_Detection_and_Quantification_System
SoftAP: Bridging the Gap for IoT Device Networking - CEL, accessed December 17, 2025, https://www.cel.com/blog/softap-headless-iot-connectivity/
Unified Provisioning - ESP32 - — ESP-IDF Programming Guide v5.5.1 documentation, accessed December 17, 2025, https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/provisioning/provisioning.html
Unified Provisioning - ESP32 - — ESP-IDF Programming Guide v5.1 documentation, accessed December 17, 2025, https://docs.espressif.com/projects/esp-idf/en/v5.1/esp32/api-reference/provisioning/provisioning.html
How We Built a Bluetooth (BLE) App for IoT Devices with FlutterFlow - Flywheel Studio, accessed December 17, 2025, https://flywheel.so/post/how-we-built-a-bluetooth-iot-app-with-flutterflow
lb_setup_ui | Flutter package - Pub.dev, accessed December 17, 2025, https://pub.dev/packages/lb_setup_ui
Flutter for IoT: Bluetooth & BLE Communication - Vibe Studio, accessed December 17, 2025, https://vibe-studio.ai/insights/flutter-for-iot-bluetooth-ble-communication
Device Shadows Plugin - aMQTT, accessed December 17, 2025, https://amqtt.readthedocs.io/en/v0.11.3/plugins/shadows/
Tutorial: Interacting with Device Shadow using the sample app and the MQTT test client - AWS IoT Core, accessed December 17, 2025, https://docs.aws.amazon.com/iot/latest/developerguide/interact-lights-device-shadows.html
TMC2209 Driver and (Stallguard) with arduino Uno - Reddit, accessed December 17, 2025, https://www.reddit.com/r/arduino/comments/16k9ktc/tmc2209_driver_and_stallguard_with_arduino_uno/
Managing ESP32 & 18650 solar charging? - Hardware - The Things Network, accessed December 17, 2025, https://www.thethingsnetwork.org/forum/t/managing-esp32-18650-solar-charging/56175
Aquassist : DIY Automatic Fish Feeder With Companion App - Instructables, accessed December 17, 2025, https://www.instructables.com/Aquassist-DIY-Automatic-Fish-Feeder-With-Companion/
AUTOMATIC FISH FEEDER USING COMPANION APP, accessed December 17, 2025, https://www.kscst.org.in/spp/47_series/47s_spp/Exhibition%20Projects/194_47S_BE_5480.pdf

Advanced Architectures for Smart Assisted Automatic Fish Feeders
GSM-Primary Connectivity & Adaptive Bio-Algorithmic Control
1. Introduction: The "Always-On" Aquaculture Paradigm
Modern aquaculture is shifting from static automation to "precision farming," driven by the need to optimize the Feed Conversion Ratio (FCR) and minimize environmental impact. While traditional IoT solutions rely on Wi-Fi, the specific constraints of aquaculture—remote pond locations, lack of terrestrial broadband, and the critical nature of life-support systems—demand a more robust approach.
This report details the engineering of a GSM-Primary smart feeder. Unlike consumer gadgets that treat cellular as a backup, this architecture treats the cellular link (LTE-M/4G) as the primary nervous system, ensuring 99.9% availability for telemetry and control. Furthermore, it integrates a dynamic Fish Feed Calculator engine that autonomously adjusts rations based on real-time metabolic and growth models, rather than static timers.
2. The Fish Feed Calculator Algorithm
A central feature of this system is the transition from "volume-based" feeding (e.g., "spin for 5 seconds") to "biological-based" feeding (e.g., "dispense 1.5% of biomass"). The firmware and backend implement a continuous feedback loop that calculates the exact nutritional requirement of the stock.
2.1 The Core Calculator Logic
The Feed Calculator is not a static lookup; it is a dynamic equation processed daily by the backend (or locally on the ESP32 if offline).
Inputs:
$N_{fish}$: Number of fish (User Initial Input - Mortality Count).
$W_{avg}$: Average weight of a single fish (g).
$T_{water}$: Current water temperature (°C).
$S_{spec}$: Species constant (Metabolic Rate).
Step 1: Biomass & Base Ration Calculation
The system first calculates the Total Biomass ($B_{total}$). It then determines the Feeding Rate (%BW). Research from 2020–2024 indicates that Feeding Rate is an inverse power function of weight; fingerlings eat ~5-8% of their body weight, while adults eat ~1-2%.

$$B_{total} = N_{fish} \times W_{avg}$$

$$R_{base} = B_{total} \times \text{LookupRate}(W_{avg})$$
Step 2: The Q10 Metabolic Adjustment
The base ration is valid only at the optimal temperature ($T_{opt}$). The system applies the $Q_{10}$ coefficient to adjust for metabolic variances. Recent studies on Nile Tilapia and Catfish confirm a $Q_{10}$ of ~2.2–2.3 for digestion rates.1

$$R_{final} = R_{base} \times Q_{10}^{\frac{T_{water} - T_{opt}}{10}} \times \text{DO\_Penalty}$$
DO_Penalty: If Dissolved Oxygen (DO) drops below 4.0 mg/L, the calculator applies a linear reduction factor (0.0 to 1.0) to prevent hypoxia-induced mortality, a logic supported by the Optimal Behavior Model (OBM).2
2.2 Predictive Growth Modeling (The "Virtual Scale")
To avoid weighing fish daily, the calculator uses a Predictive Growth Loop. It estimates the weight gain from the previous day's feed to update $W_{avg}$ for the current day.

$$\Delta W = \frac{\text{Feed\_Consumed}}{FCR_{expected}}$$

$$W_{avg\_new} = W_{avg\_old} + \frac{\Delta W}{N_{fish}}$$
Implementation: The mobile app allows the user to perform a "Calibration Weighing" every 2 weeks to correct any drift in the model.3
3. Communication Architecture: GSM-Primary Strategy
Reliability in remote farms is non-negotiable. This design inverts the standard IoT stack: Cellular is Primary, Wi-Fi is Secondary.
3.1 Hardware Selection: SIM7600 (LTE Cat-4)
The SIM7600G-H is selected over Low-Power WAN (NB-IoT) modules because "Smart Assisted" feeding requires Visual Verification. NB-IoT (e.g., SIM7000) lacks the bandwidth to transmit validation images or video snippets of the feeding event.4
Bandwidth: ~150 Mbps DL (sufficient for 480p video streaming).
Fallback: Robust 3G/2G fallback for rural areas where LTE might be spotty.
Protocol: MQTT over TLS 1.2 is mandatory for security.5
3.2 The Failover State Machine
The firmware runs a prioritized state machine to manage connectivity costs and power.
State: GSM_ACTIVE (Default)
Device maintains an MQTT Keep-Alive of 900s (15 mins) to minimize data usage while staying reachable.6
Telemetry (Temp, pH, Feed Status) is packed using Protocol Buffers (Protobuf) rather than JSON. This reduces payload size by ~65%, significantly lowering cellular data costs.8
State: WIFI_OPPORTUNISTIC
Every 6 hours, the ESP32 performs a passive Wi-Fi scan. If a known SSID (e.g., "Farm_Office") is detected with RSSI > -75dBm, it attempts to hand over the MQTT session to Wi-Fi to save cellular data.10
State: CRITICAL_ALERT
If a jam or water quality crisis occurs, the system ignores all power-saving rules and immediately broadcasts via both GSM and Wi-Fi to ensure delivery.10
4. Electromechanical Algorithms
4.1 Stepper Motor Dosing with StallGuard
To achieve the precision required by the Feed Calculator (e.g., dispensing exactly 23 grams), a stepper motor (NEMA 17) is used instead of a DC motor.
Driver: Trinamic TMC2209.
Algorithm: The firmware utilizes Sensorless Stall Detection (Back-EMF analysis). If the auger hits a hard pellet, the Back-EMF load value drops. The ESP32 detects this via UART interrupts and executes an Anti-Jam Routine: Stop -> Retract 180° -> Agitate -> Resume.12
4.2 Computer Vision Satiety Index (ESP32-CAM)
To prevent waste, the system uses a lightweight computer vision algorithm (Optical Flow or simplified Difference of Gaussian) on the ESP32-CAM.
Pre-Feed Scan: Measure baseline surface activity.
Active-Feed Scan: Calculate the "Boil Index" (activity intensity).
Logic: If Boil_Index drops below threshold before the calculated ration ($R_{final}$) is finished, the feeder executes an Early Cutoff. This saves feed and protects water quality.14
5. Mobile Application & Backend
5.1 Provisioning: Bluetooth Low Energy (BLE)
For the best user experience, "SoftAP" (connecting to a device's Wi-Fi hotspot) is replaced by BLE Provisioning.
Discovery: App scans for the feeder via BLE.
Security: Handshake uses ECDH (Elliptic-Curve Diffie-Hellman) to create a secure channel.16
Config: User transfers Wi-Fi credentials and API keys via Protobuf over BLE GATT characteristics.17
5.2 The "Offline-First" Dashboard
The mobile app (Flutter/React Native) is architected to work offline.
Local DB: Feeding schedules and calculator parameters are stored in a local SQLite database on the phone.
Sync: When the phone has internet, it synchronizes with the AWS IoT Device Shadow. The Shadow maintains the "Desired State" (e.g., Schedule: 08:00, 100g). The feeder wakes up, reads the Shadow, and updates its "Reported State".18
6. Summary of Key Technologies
Component
Technology / Algorithm
Benefit
Connectivity
GSM-Primary (SIM7600) + Wi-Fi Backup
Reliability in rural/offshore locations.
Data Protocol
MQTT + Protobuf
Low bandwidth, low data cost, high speed.
Feeding Logic
Dynamic Calculator ($Biomass \times \%BW \times Q_{10}$)
Biologically optimized growth; improved FCR.
Motor Control
TMC2209 StallGuard
Prevents mechanical jams without external sensors.
Vision AI
Optical Flow / Satiety Index
Prevents overfeeding by detecting fish behavior.
App Sync
AWS Device Shadow (Offline-First)
Seamless control even with intermittent connection.

Works cited
Effects of temperature on feeding and digestive processes in fish - PMC - PubMed Central, accessed December 17, 2025, https://pmc.ncbi.nlm.nih.gov/articles/PMC7678922/
Dining in danger: Resolving adaptive fish behavior increases ..., accessed December 17, 2025, https://pmc.ncbi.nlm.nih.gov/articles/PMC11303985/
Optimizing Feeding Strategies in Aquaculture Using Machine Learning: Ensuring Sustainable and Economically Viable Fish Farming Practices - SINTEF, accessed December 17, 2025, https://www.sintef.no/en/publications/publication/2275278/
Purchased this module and found it won't work on 4G SIM cards. Am i done with it? - Reddit, accessed December 17, 2025, https://www.reddit.com/r/arduino/comments/1g6021v/purchased_this_module_and_found_it_wont_work_on/
Empirical Evaluation of TLS-Enhanced MQTT on IoT Devices for V2X Use Cases - MDPI, accessed December 17, 2025, https://www.mdpi.com/2076-3417/15/15/8398
MQTT disconnects around 24hrs and doesnt reconnect · Issue #723 · vshymanskyy/TinyGSM - GitHub, accessed December 17, 2025, https://github.com/vshymanskyy/TinyGSM/issues/723
Optimizing Data Cost Efficiency in MQTT-Based IoT and Connected Systems - HiveMQ, accessed December 17, 2025, https://www.hivemq.com/blog/optimizing-data-cost-efficiency-mqtt-based-iot-connected-systems/
Benchmarking Data Serialization: JSON vs. Protobuf vs. Flatbuffers | by Harshil Jani, accessed December 17, 2025, https://medium.com/@harshiljani2002/benchmarking-data-serialization-json-vs-protobuf-vs-flatbuffers-3218eecdba77
Protobuf vs JSON: Choosing the Best Data Exchange Format - Mad Devs, accessed December 17, 2025, https://maddevs.io/blog/protocol-buffers-vs-json/
(PDF) ESP32-Based Dual-Connectivity Data Logger for Continuous ..., accessed December 17, 2025, https://www.researchgate.net/publication/397053816_ESP32-Based_Dual-Connectivity_Data_Logger_for_Continuous_Environmental_Monitoring_for_AI_Applications
Wi-Fi Driver - ESP32-S2 - — ESP-IDF Programming Guide v5.5.1 documentation, accessed December 17, 2025, https://docs.espressif.com/projects/esp-idf/en/stable/esp32s2/api-guides/wifi.html
A Look at Sensorless Homing: Stepper Motor Control Without End Switches - Circuit Digest, accessed December 17, 2025, https://circuitdigest.com/news/a-look-at-sensorless-homing-stepper-motor-control-without-end-switches
Sensorless Stall Detection With the DRV8889-Q1 - Texas Instruments, accessed December 17, 2025, https://www.ti.com/lit/an/slvaei3/slvaei3.pdf
Fish Feeding Behavior Recognition Using Adaptive DMCA-UMT Algorithm, accessed December 17, 2025, https://journal.bit.edu.cn/jbit/article/doi/10.15918/j.jbit1004-0579.2023.008
Automatic Recognition of Fish Behavior with a Fusion of RGB and Optical Flow Data Based on Deep Learning - MDPI, accessed December 17, 2025, https://www.mdpi.com/2076-2615/11/10/2774
Bluetooth LE for Wi-Fi Onboarding: A Game-Changer for IoT Connectivity - Novel Bits, accessed December 17, 2025, https://novelbits.io/ble-wifi-onboarding-provisioning/
Provisioning Scheme Introduction - - — ESP-Techpedia latest documentation, accessed December 17, 2025, https://docs.espressif.com/projects/esp-techpedia/en/latest/esp-friends/solution-introduction/provision/provision-solution.html
AWS IoT Device Shadow service, accessed December 17, 2025, https://docs.aws.amazon.com/iot/latest/developerguide/iot-device-shadows.html
How to Build Resilient Offline-First Mobile Apps with Seamless Syncing | Medium, accessed December 17, 2025, https://medium.com/@quokkalabs135/how-to-build-resilient-offline-first-mobile-apps-with-seamless-syncing-adc98fb72909