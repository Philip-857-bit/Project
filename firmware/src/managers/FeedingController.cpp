/**
 * @file FeedingController.cpp
 * @brief Feeding control implementation with TMC2209 or A4988 driver
 */

#include "FeedingController.h"
#include <time.h>

#define TMC_DRIVER_ADDRESS 0b00

FeedingController::FeedingController()
    : _sensorManager(nullptr)
    , _storage(nullptr)
#ifdef USE_TMC2209
    , _driver(nullptr)
    , _motorCurrentMA(MOTOR_CURRENT_MA)
    , _stallThreshold(MOTOR_STALL_THRESHOLD)
    , _stallDetected(false)
#endif
#ifdef USE_A4988
    , _microstepMode(MicrostepMode::SIXTEENTH_STEP)
#endif
    , _motorInitialized(false)
    , _feedingActive(false)
    , _targetGrams(0)
    , _dispensedGrams(0)
    , _feedingStartTime(0)
    , _gramsPerRevolution(GRAMS_PER_REVOLUTION)
    , _microSteps(MOTOR_MICROSTEPS)
    , _stepDelayUs(1000)
    , _scheduleCount(0)
    , _scheduleEnabled(true)
    , _lastExecutedSchedule(-1)
    , _lastScheduleCheck(0) {
    
    // Clarias gariepinus - post-juvenile 50g+ - Akure Nigeria field trial
    _speciesParams.q10Coefficient = Q10_CLARIAS;
    _speciesParams.referenceTemp   = CLARIAS_REFERENCE_TEMP;
    _speciesParams.minTemp         = CLARIAS_TEMP_MIN;
    _speciesParams.maxTemp         = CLARIAS_LETHAL_MAX;
    
    memset(&_lastEvent, 0, sizeof(_lastEvent));
    memset(_schedule, 0, sizeof(_schedule));
}

FeedingController::~FeedingController() {
#ifdef USE_TMC2209
    if (_driver) delete _driver;
#endif
}

bool FeedingController::begin(SensorManager* sensorManager, NVSStorage* storage) {
    _sensorManager = sensorManager;
    _storage = storage;
    _motorInitialized = initMotor();
    loadSchedule();
    
    float savedGramsPerRev = _storage->getFloat("grams_per_rev", 0);
    if (savedGramsPerRev > 0) _gramsPerRevolution = savedGramsPerRev;
    
    Serial.printf("[FeedingController] Init: %s\n", _motorInitialized ? "OK" : "FAIL");
    return _motorInitialized;
}


bool FeedingController::initMotor() {
    pinMode(PIN_STEP, OUTPUT);
    pinMode(PIN_DIR, OUTPUT);
    digitalWrite(PIN_STEP, LOW);
    digitalWrite(PIN_DIR, LOW);

#ifdef USE_TMC2209
    Serial2.begin(115200, SERIAL_8N1, PIN_TMC_RX, PIN_TMC_TX);
    _driver = new TMC2209Stepper(&Serial2, 0.11f, TMC_DRIVER_ADDRESS);
    _driver->begin();
    if (_driver->test_connection() != 0) return false;
    _driver->toff(4);
    _driver->blank_time(24);
    _driver->rms_current(_motorCurrentMA);
    _driver->microsteps(MOTOR_MICROSTEPS);
    _driver->TCOOLTHRS(0xFFFFF);
    _driver->semin(5);
    _driver->semax(2);
    _driver->sedn(0b01);
    _driver->SGTHRS(_stallThreshold);
    _driver->en_spreadCycle(true);
    _driver->pwm_autoscale(true);
    _driver->pwm_autograd(true);
    pinMode(PIN_DIAG, INPUT);
    return true;
#elif defined(USE_A4988)
    pinMode(PIN_MS1, OUTPUT);
    pinMode(PIN_MS2, OUTPUT);
    pinMode(PIN_MS3, OUTPUT);
    setMicrostepPins();
    return true;
#else
    // DM542/TB6600 only need STEP/DIR pins.
    return true;
#endif
}

#ifdef USE_A4988
void FeedingController::setMicrostepPins() {
    switch (_microstepMode) {
        case MicrostepMode::FULL_STEP:
            digitalWrite(PIN_MS1, LOW); digitalWrite(PIN_MS2, LOW); digitalWrite(PIN_MS3, LOW); break;
        case MicrostepMode::HALF_STEP:
            digitalWrite(PIN_MS1, HIGH); digitalWrite(PIN_MS2, LOW); digitalWrite(PIN_MS3, LOW); break;
        case MicrostepMode::QUARTER_STEP:
            digitalWrite(PIN_MS1, LOW); digitalWrite(PIN_MS2, HIGH); digitalWrite(PIN_MS3, LOW); break;
        case MicrostepMode::EIGHTH_STEP:
            digitalWrite(PIN_MS1, HIGH); digitalWrite(PIN_MS2, HIGH); digitalWrite(PIN_MS3, LOW); break;
        default:
            digitalWrite(PIN_MS1, HIGH); digitalWrite(PIN_MS2, HIGH); digitalWrite(PIN_MS3, HIGH); break;
    }
}
void FeedingController::setMicrostepMode(MicrostepMode mode) { _microstepMode = mode; setMicrostepPins(); }
#endif

#ifdef USE_TMC2209
void FeedingController::setMotorCurrent(uint16_t currentMA) { _motorCurrentMA = currentMA; if (_driver) _driver->rms_current(currentMA); }
void FeedingController::setStallThreshold(uint8_t threshold) { _stallThreshold = threshold; if (_driver) _driver->SGTHRS(threshold); }
bool FeedingController::isStallDetected() const { return _stallDetected; }
uint16_t FeedingController::getMotorLoad() const { return _driver ? _driver->SG_RESULT() : 0; }
#endif

void FeedingController::update() {
    unsigned long now = millis();
    if (now - _lastScheduleCheck >= 60000) {
        _lastScheduleCheck = now;
        if (_scheduleEnabled && !_feedingActive) checkSchedule();
    }
    if (_feedingActive && now - _feedingStartTime > FEEDING_TIMEOUT_MS) {
        stopFeeding();
        _lastEvent.result = FeedingResult::TIMEOUT;
    }
#ifdef USE_TMC2209
    if (_feedingActive && digitalRead(PIN_DIAG) == HIGH) {
        _stallDetected = true;
        stopFeeding();
        _lastEvent.result = FeedingResult::STALL_DETECTED;
    }
#endif
}

void FeedingController::checkSchedule() {
    time_t now; struct tm ti; time(&now); localtime_r(&now, &ti);
    int dayBit = 1 << ti.tm_wday;
    for (int i = 0; i < _scheduleCount; i++) {
        if (!_schedule[i].enabled || !(_schedule[i].daysOfWeek & dayBit)) continue;
        if (_schedule[i].hour == ti.tm_hour && _schedule[i].minute == ti.tm_min && _lastExecutedSchedule != i) {
            _lastExecutedSchedule = i;
            dispense(_schedule[i].quantityGrams, FeedingTrigger::SCHEDULED);
            break;
        }
    }
    if (ti.tm_hour == 0 && ti.tm_min == 0) _lastExecutedSchedule = -1;
}

bool FeedingController::feedNow(float grams) {
    if (_feedingActive || grams < MIN_FEED_GRAMS || grams > MAX_FEED_GRAMS) return false;
    dispense(grams, FeedingTrigger::MANUAL);
    return true;
}

bool FeedingController::feedRemote(float adjustedGrams) {
    if (_feedingActive || adjustedGrams < MIN_FEED_GRAMS || adjustedGrams > MAX_FEED_GRAMS) return false;
    // Backend already applied Q10/OBM; use REMOTE trigger to bypass firmware Q10
    dispense(adjustedGrams, FeedingTrigger::REMOTE);
    return true;
}


FeedingResult FeedingController::dispense(float grams, FeedingTrigger trigger) {
    _feedingActive = true;
    _feedingStartTime = millis();
    _targetGrams = grams;
    _dispensedGrams = 0;
#ifdef USE_TMC2209
    _stallDetected = false;
#endif
    float temperature = Q10_REFERENCE_TEMP, q10Factor = 1.0f;
    if (trigger != FeedingTrigger::REMOTE) {
        // REMOTE commands come pre-adjusted from the backend; skip firmware Q10
        if (_sensorManager) {
            SensorData data = _sensorManager->getCurrentData();
            if (data.temperatureValid) { temperature = data.temperature; q10Factor = calculateQ10Adjustment(1.0f, temperature); }
        }
    } else if (_sensorManager) {
        temperature = _sensorManager->getCurrentData().temperature;
    }
    float adjustedGrams = grams * q10Factor;
    bool completed = moveSteps(gramsToSteps(adjustedGrams), true);
    _dispensedGrams = adjustedGrams;
    _feedingActive = false;
    FeedingResult result = FeedingResult::SUCCESS;
#ifdef USE_TMC2209
    if (_stallDetected) result = FeedingResult::STALL_DETECTED;
    else if (!completed) result = FeedingResult::PARTIAL;
#else
    if (!completed) result = FeedingResult::PARTIAL;
#endif
    _lastEvent.timestamp = millis();
    _lastEvent.quantityGrams = grams;
    _lastEvent.actualDispensed = _dispensedGrams;
    _lastEvent.durationMs = (uint32_t)(millis() - _feedingStartTime);
    _lastEvent.trigger = trigger;
    _lastEvent.result = result;
    _lastEvent.temperature = temperature;
    _lastEvent.q10Factor = q10Factor;
    _lastEvent.obmSafetyFactor = 1.0f;
    _lastEvent.errorMessage = "";
    logEvent(_lastEvent);
    return result;
}

bool FeedingController::moveSteps(long steps, bool direction) {
    digitalWrite(PIN_DIR, direction ? HIGH : LOW);
    delayMicroseconds(5);
    unsigned long stepDelay = _stepDelayUs;
    for (long i = 0; i < steps; i++) {
        stepPulse();
        delayMicroseconds(stepDelay);
        if (i % 100 == 0) yield();
#ifdef USE_TMC2209
        if (digitalRead(PIN_DIAG) == HIGH) { _stallDetected = true; return false; }
#endif
        if (millis() - _feedingStartTime > FEEDING_TIMEOUT_MS) return false;
    }
    return true;
}

void FeedingController::stepPulse() {
    digitalWrite(PIN_STEP, HIGH);
    delayMicroseconds(MOTOR_PULSE_WIDTH_US);
    digitalWrite(PIN_STEP, LOW);
}

void FeedingController::stopFeeding() {
    if (_feedingActive) { _feedingActive = false; _lastEvent.result = FeedingResult::CANCELLED; }
}

float FeedingController::calculateQ10Adjustment(float baseAmount, float temperature) {
    // Clarias gariepinus thermal safety gates (post-juvenile stage)
    if (temperature >= CLARIAS_LETHAL_MAX) {
        Serial.printf("[Q10] EMERGENCY STOP - lethal temp %.1fC\n", temperature);
        return 0.0f;
    }
    if (temperature >= CLARIAS_CRITICAL_MAX) {
        float reduction = 1.0f - ((temperature - CLARIAS_CRITICAL_MAX) /
                          (CLARIAS_LETHAL_MAX - CLARIAS_CRITICAL_MAX));
        Serial.printf("[Q10] Critical temp %.1fC - reduced to %.0f%%\n",
                      temperature, max(0.0f, reduction) * 100.0f);
        return baseAmount * max(0.0f, reduction);
    }
    if (temperature < CLARIAS_TEMP_MIN) {
        Serial.printf("[Q10] Low temp %.1fC - reduced 80%%\n", temperature);
        return baseAmount * 0.2f;
    }

    // Standard Q10 adjustment within viable range
    // Clarias: higher temp toward optimum = better FCR (Kasihmuddin 2021)
    float tempDiff  = temperature - _speciesParams.referenceTemp;
    float q10Factor = pow(_speciesParams.q10Coefficient, tempDiff / 10.0f);
    q10Factor = constrain(q10Factor, 0.3f, 2.0f);

    Serial.printf("[Q10] Temp %.1fC factor %.3f adjusted %.2fg\n",
                  temperature, q10Factor, baseAmount * q10Factor);
    return baseAmount * q10Factor;
}

long FeedingController::gramsToSteps(float grams) {
    float revolutions = grams / _gramsPerRevolution;
#ifdef USE_TMC2209
    return (long)(revolutions * MOTOR_STEPS_PER_REV * MOTOR_MICROSTEPS);
#else
    return (long)(revolutions * MOTOR_STEPS_PER_REV * _microSteps);
#endif
}

void FeedingController::calibrateGramsPerRev(float grams) { _gramsPerRevolution = grams; _storage->putFloat("grams_per_rev", grams); }
void FeedingController::setMicrosteps(int microsteps) {
    if (microsteps < 1) {
        microsteps = 1;
    }
    _microSteps = microsteps;
}
void FeedingController::setMaxSpeed(int stepsPerSecond) {
    if (stepsPerSecond < 1) {
        stepsPerSecond = 1;
    }
    _stepDelayUs = 1000000UL / (unsigned long)stepsPerSecond;
}

bool FeedingController::jogSteps(long steps, bool direction) {
    if (!_motorInitialized || _feedingActive || steps <= 0) {
        return false;
    }

    _feedingActive = true;
    _feedingStartTime = millis();
    bool completed = moveSteps(steps, direction);
    _feedingActive = false;
    return completed;
}

void FeedingController::printMotorDiagnostics() const {
    Serial.println();
    Serial.println("[MotorTest] ======== MOTOR CONFIG ========");
#ifdef USE_DM542
    Serial.println("[MotorTest] Driver: DM542");
#elif defined(USE_TB6600)
    Serial.println("[MotorTest] Driver: TB6600");
#elif defined(USE_TMC2209)
    Serial.println("[MotorTest] Driver: TMC2209");
#elif defined(USE_A4988)
    Serial.println("[MotorTest] Driver: A4988");
#else
    Serial.println("[MotorTest] Driver: Step/Dir");
#endif
    Serial.printf("[MotorTest] STEP pin: GPIO%d\n", (int)PIN_STEP);
    Serial.printf("[MotorTest] DIR pin: GPIO%d\n", (int)PIN_DIR);
    Serial.println("[MotorTest] ENABLE pin: not connected/skipped");
    Serial.printf("[MotorTest] Motor initialized: %s\n", _motorInitialized ? "yes" : "no");
    Serial.printf("[MotorTest] Full steps/rev: %d\n", MOTOR_STEPS_PER_REV);
    Serial.printf("[MotorTest] Microsteps: %d\n", _microSteps);
    Serial.printf("[MotorTest] Test steps/rev: %ld\n", getStepsPerRevolution());
    Serial.printf("[MotorTest] Step delay: %lu us\n", _stepDelayUs);
    Serial.printf("[MotorTest] Pulse width: %d us\n", MOTOR_PULSE_WIDTH_US);
    Serial.printf("[MotorTest] Calibration: %.2f g/rev\n", _gramsPerRevolution);
    Serial.println("[MotorTest] DM542 wiring: ESP32 STEP/DIR -> PUL-/DIR-, driver PUL+/DIR+ -> 5V or driver logic V+");
    Serial.println("[MotorTest] ==============================");
    Serial.println();
}

long FeedingController::getStepsPerRevolution() const {
    return (long)MOTOR_STEPS_PER_REV * (long)_microSteps;
}

bool FeedingController::setSchedule(ScheduleEntry* entries, int count) {
    if (count > SCHEDULE_MAX_ENTRIES) count = SCHEDULE_MAX_ENTRIES;
    memcpy(_schedule, entries, count * sizeof(ScheduleEntry));
    _scheduleCount = count;
    saveSchedule();
    return true;
}
void FeedingController::loadSchedule() { size_t size = _storage->getBytes(NVS_KEY_SCHEDULE, _schedule, sizeof(_schedule)); if (size > 0) _scheduleCount = size / sizeof(ScheduleEntry); }
void FeedingController::saveSchedule() { _storage->putBytes(NVS_KEY_SCHEDULE, _schedule, _scheduleCount * sizeof(ScheduleEntry)); }
void FeedingController::logEvent(const FeedingEvent& event) { Serial.printf("[Feed] %.1fg %s\n", event.actualDispensed, event.result == FeedingResult::SUCCESS ? "OK" : "ERR"); }

bool FeedingController::isFeedingActive() const { return _feedingActive; }
FeedingEvent FeedingController::getLastEvent() const { return _lastEvent; }
int FeedingController::getScheduleCount() const { return _scheduleCount; }
bool FeedingController::isScheduleEnabled() const { return _scheduleEnabled; }
void FeedingController::setScheduleEnabled(bool enabled) { _scheduleEnabled = enabled; }
void FeedingController::setSpeciesParams(const SpeciesParams& params) { _speciesParams = params; }
ScheduleEntry FeedingController::getScheduleEntry(int index) const { return (index >= 0 && index < _scheduleCount) ? _schedule[index] : ScheduleEntry(); }
uint64_t FeedingController::getTimeToNextFeeding() const {
    time_t now; struct tm ti; time(&now); localtime_r(&now, &ti);
    int currentMin = ti.tm_hour * 60 + ti.tm_min, minDiff = INT_MAX;
    for (int i = 0; i < _scheduleCount; i++) {
        if (!_schedule[i].enabled) continue;
        int diff = _schedule[i].hour * 60 + _schedule[i].minute - currentMin;
        if (diff <= 0) diff += 24 * 60;
        if (diff < minDiff) minDiff = diff;
    }
    return (minDiff == INT_MAX) ? DEEP_SLEEP_DURATION_US : (uint64_t)minDiff * 60 * 1000000;
}
