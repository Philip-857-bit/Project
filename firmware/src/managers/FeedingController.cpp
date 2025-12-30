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
    , _stepDelayUs(1000)
    , _scheduleCount(0)
    , _scheduleEnabled(true)
    , _lastExecutedSchedule(-1)
    , _lastScheduleCheck(0) {
    
    _speciesParams.q10Coefficient = Q10_TILAPIA;
    _speciesParams.referenceTemp = Q10_REFERENCE_TEMP;
    _speciesParams.minTemp = 18.0f;
    _speciesParams.maxTemp = 32.0f;
    
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
    pinMode(PIN_ENABLE, OUTPUT);
    digitalWrite(PIN_ENABLE, HIGH);
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
#else
    pinMode(PIN_MS1, OUTPUT);
    pinMode(PIN_MS2, OUTPUT);
    pinMode(PIN_MS3, OUTPUT);
    setMicrostepPins();
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


FeedingResult FeedingController::dispense(float grams, FeedingTrigger trigger) {
    _feedingActive = true;
    _feedingStartTime = millis();
    _targetGrams = grams;
    _dispensedGrams = 0;
#ifdef USE_TMC2209
    _stallDetected = false;
#endif
    float temperature = Q10_REFERENCE_TEMP, q10Factor = 1.0f;
    if (_sensorManager) {
        SensorData data = _sensorManager->getCurrentData();
        if (data.temperatureValid) { temperature = data.temperature; q10Factor = calculateQ10Adjustment(1.0f, temperature); }
    }
    float adjustedGrams = grams * q10Factor;
    digitalWrite(PIN_ENABLE, LOW); delay(10);
    bool completed = moveSteps(gramsToSteps(adjustedGrams), true);
    digitalWrite(PIN_ENABLE, HIGH);
    _dispensedGrams = adjustedGrams;
    _feedingActive = false;
    FeedingResult result = FeedingResult::SUCCESS;
#ifdef USE_TMC2209
    if (_stallDetected) result = FeedingResult::STALL_DETECTED;
    else if (!completed) result = FeedingResult::PARTIAL;
#else
    if (!completed) result = FeedingResult::PARTIAL;
#endif
    _lastEvent = {millis(), grams, _dispensedGrams, (uint32_t)(millis() - _feedingStartTime), trigger, result, temperature, 0, q10Factor, 1.0f, ""};
    logEvent(_lastEvent);
    return result;
}

bool FeedingController::moveSteps(long steps, bool direction) {
    digitalWrite(PIN_DIR, direction ? HIGH : LOW);
    delayMicroseconds(5);
    unsigned long stepDelay = 1000000 / MOTOR_MAX_SPEED;
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
    if (_feedingActive) { _feedingActive = false; digitalWrite(PIN_ENABLE, HIGH); _lastEvent.result = FeedingResult::CANCELLED; }
}

float FeedingController::calculateQ10Adjustment(float baseAmount, float temperature) {
    float tempDiff = temperature - _speciesParams.referenceTemp;
    float q10Factor = pow(_speciesParams.q10Coefficient, tempDiff / 10.0f);
    if (temperature < _speciesParams.minTemp) q10Factor *= max(0.0f, (temperature - (_speciesParams.minTemp - 5)) / 5.0f);
    else if (temperature > _speciesParams.maxTemp) q10Factor *= max(0.0f, ((_speciesParams.maxTemp + 5) - temperature) / 5.0f);
    return baseAmount * constrain(q10Factor, 0.3f, 2.5f);
}

long FeedingController::gramsToSteps(float grams) {
    float revolutions = grams / _gramsPerRevolution;
#ifdef USE_TMC2209
    return (long)(revolutions * MOTOR_STEPS_PER_REV * MOTOR_MICROSTEPS);
#else
    return (long)(revolutions * MOTOR_STEPS_PER_REV * (int)_microstepMode);
#endif
}

void FeedingController::calibrateGramsPerRev(float grams) { _gramsPerRevolution = grams; _storage->putFloat("grams_per_rev", grams); }
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
