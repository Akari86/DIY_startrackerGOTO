// motion.h -- Master MCU
// Wraps two AccelStepper axes: manual constant-speed jog, and
// position-controlled auto-tracking with shortest-path pan wraparound.
#pragma once
#include <Arduino.h>

void motionInit();
void motionEnableDrivers(bool enable);

// Call every loop iteration while in MANUAL mode. Raw ADC joystick values
// (0-4095) are mapped to a signed jog speed with a center deadzone.
void motionServiceManualJog(int joyXRaw, int joyYRaw);

// Call every loop iteration while in AUTO mode -- services AccelStepper's
// internal step timing toward whatever target was last commanded.
void motionServiceAutoRun();

// Command a new Pan/Tilt target (degrees). For a continuous-rotation pan
// stage this takes the shortest path (handles the 350->10 wraparound
// case); for a mechanically limited stage it clamps and reports that in
// outOfRange. outOfRange for tilt uses the same clamp-and-report behavior.
void motionCommandTarget(float azDeg, float altDeg, bool &panOutOfRange, bool &tiltOutOfRange);

// Re-derive the tracked software angles from actual stepper step counts.
// Call this when leaving MANUAL mode so AUTO tracking resumes from wherever
// the joystick actually left the mount, not from stale state.
void motionSyncCurrentFromSteppers();

void motionStop();

float motionGetCurrentPanDeg();
float motionGetCurrentTiltDeg();
