// motion.cpp -- Master MCU
// Requires the "AccelStepper" library (Mike McCauley / adaptions), available
// via Arduino Library Manager.
//
// HOMING NOTE: steppers are open-loop. currentPanDeg/currentTiltDeg below
// are *believed* positions, not measured ones. This firmware assumes the
// mount is physically parked at Pan=0, Tilt=0 at power-on. For real
// hardware, add limit switches and a homing routine (drive toward the
// switch at low speed, zero the counters on trigger) before trusting AUTO
// mode -- open-loop steppers accumulate silent position error if they ever
// stall or get bumped, and nothing here can detect that.

#include "motion.h"
#include "config.h"
#include <AccelStepper.h>
#include <math.h>

static AccelStepper panStepper(AccelStepper::DRIVER, PAN_STEP_PIN, PAN_DIR_PIN);
static AccelStepper tiltStepper(AccelStepper::DRIVER, TILT_STEP_PIN, TILT_DIR_PIN);

// Software-tracked believed position, degrees. Pan is free-running (not
// wrapped into 0-360) so the step count stays monotonic and easy to reason
// about; wrap only happens when computing shortest-path deltas.
static float currentPanDeg = 0.0f;
static float currentTiltDeg = 0.0f;

static const bool PAN_IS_CONTINUOUS = (PAN_MAX_DEG - PAN_MIN_DEG) >= 360.0f;

void motionInit() {
    pinMode(STEPPERS_ENABLE_PIN, OUTPUT);
    motionEnableDrivers(true);

    panStepper.setMaxSpeed(STEPPER_MAX_SPEED_SPS);
    panStepper.setAcceleration(STEPPER_MAX_ACCEL_SPS2);
    tiltStepper.setMaxSpeed(STEPPER_MAX_SPEED_SPS);
    tiltStepper.setAcceleration(STEPPER_MAX_ACCEL_SPS2);

    panStepper.setCurrentPosition(0);
    tiltStepper.setCurrentPosition(0);
}

void motionEnableDrivers(bool enable) {
    // Most STEP/DIR drivers (A4988/DRV8825) enable on LOW.
    digitalWrite(STEPPERS_ENABLE_PIN, enable ? LOW : HIGH);
}

static float applyDeadzone(int raw, int center, int deadzone) {
    int delta = raw - center;
    if (abs(delta) < deadzone) return 0.0f;
    int sign = (delta > 0) ? 1 : -1;
    int magnitude = abs(delta) - deadzone;
    int maxMagnitude = 2048 - deadzone; // raw is a 12-bit ADC, center ~2048
    if (maxMagnitude < 1) maxMagnitude = 1;
    float norm = (float)magnitude / (float)maxMagnitude;
    if (norm > 1.0f) norm = 1.0f;
    return sign * norm;
}

void motionServiceManualJog(int joyXRaw, int joyYRaw) {
    const int center = 2048, deadzone = 150;

    float panNorm = applyDeadzone(joyXRaw, center, deadzone);
    float tiltNorm = applyDeadzone(joyYRaw, center, deadzone);

    panStepper.setSpeed(panNorm * STEPPER_JOG_MAX_SPEED_SPS);
    tiltStepper.setSpeed(tiltNorm * STEPPER_JOG_MAX_SPEED_SPS);

    // runSpeed() must be called as often as possible for smooth constant
    // -speed motion; it self-paces internally against micros().
    panStepper.runSpeed();
    tiltStepper.runSpeed();
}

void motionServiceAutoRun() {
    panStepper.run();
    tiltStepper.run();
}

void motionSyncCurrentFromSteppers() {
    currentPanDeg = panStepper.currentPosition() / PAN_STEPS_PER_DEGREE;
    currentTiltDeg = tiltStepper.currentPosition() / TILT_STEPS_PER_DEGREE;
}

static float shortestDelta(float fromDeg, float toDeg) {
    // Wrap the difference into [-180, 180) -- shortest path around a circle.
    float d = fmodf(toDeg - fromDeg + 540.0f, 360.0f) - 180.0f;
    return d;
}

void motionCommandTarget(float azDeg, float altDeg, bool &panOutOfRange, bool &tiltOutOfRange) {
    panOutOfRange = false;
    tiltOutOfRange = false;

    // ---- Pan / azimuth ----
    if (PAN_IS_CONTINUOUS) {
        float delta = shortestDelta(currentPanDeg, azDeg);
        float newTargetDeg = currentPanDeg + delta;
        long targetSteps = lroundf(newTargetDeg * PAN_STEPS_PER_DEGREE);
        panStepper.moveTo(targetSteps);
        currentPanDeg = newTargetDeg;
    } else {
        float clamped = azDeg;
        if (clamped < PAN_MIN_DEG) { clamped = PAN_MIN_DEG; panOutOfRange = true; }
        if (clamped > PAN_MAX_DEG) { clamped = PAN_MAX_DEG; panOutOfRange = true; }
        long targetSteps = lroundf((clamped - PAN_MIN_DEG) * PAN_STEPS_PER_DEGREE);
        panStepper.moveTo(targetSteps);
        currentPanDeg = clamped;
    }

    // ---- Tilt / altitude (never wraps) ----
    float clampedTilt = altDeg;
    if (clampedTilt < TILT_MIN_DEG) { clampedTilt = TILT_MIN_DEG; tiltOutOfRange = true; }
    if (clampedTilt > TILT_MAX_DEG) { clampedTilt = TILT_MAX_DEG; tiltOutOfRange = true; }
    long tiltSteps = lroundf((clampedTilt - TILT_MIN_DEG) * TILT_STEPS_PER_DEGREE);
    tiltStepper.moveTo(tiltSteps);
    currentTiltDeg = clampedTilt;
}

void motionStop() {
    panStepper.stop();
    tiltStepper.stop();
}

float motionGetCurrentPanDeg() { return currentPanDeg; }
float motionGetCurrentTiltDeg() { return currentTiltDeg; }
