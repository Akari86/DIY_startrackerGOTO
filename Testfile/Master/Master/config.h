// config.h -- Master MCU (plain ESP32 DevKit)
// Adjust pins to match your actual wiring.
#pragma once

// ---------- Joystick (manual jog + Enter button) ----------
// ADC1 pins (32-39) stay reliable once BluetoothSerial (radio) is running;
// ADC2 pins do not.
#define JOY_X_PIN      34   // Pan jog
#define JOY_Y_PIN      35   // Tilt jog
#define JOY_BTN_PIN    32   // spare / future use, INPUT_PULLUP, active LOW

// ---------- Potentiometer (tracking duration, 0-120 min) ----------
#define POT_PIN        33

// ---------- Stepper drivers (A4988 / DRV8825 style STEP+DIR) ----------
#define PAN_STEP_PIN   25
#define PAN_DIR_PIN    26
#define TILT_STEP_PIN  27
#define TILT_DIR_PIN   14
#define STEPPERS_ENABLE_PIN 13   // shared /ENABLE, active LOW on most drivers
                                   // (kept off GPIO12: it's a strapping pin
                                   // that affects flash voltage at boot)

// Mechanical conversion: motor full steps/rev * microstep setting * gear ratio / 360.
// Example below: 200 steps/rev, 1/16 microstepping, 1:1 drive -> 8.888 steps/deg.
// EDIT to match your actual driver microstep jumpers and any gearing/belt ratio.
#define PAN_STEPS_PER_DEGREE   ((200.0f * 16.0f) / 360.0f)
#define TILT_STEPS_PER_DEGREE  ((200.0f * 16.0f) / 360.0f)

#define STEPPER_MAX_SPEED_SPS      2000.0f  // steps/sec, auto-tracking moves
#define STEPPER_MAX_ACCEL_SPS2     4000.0f  // steps/sec^2
#define STEPPER_JOG_MAX_SPEED_SPS  1200.0f  // steps/sec, manual joystick jog

// Steppers are open-loop: there is no absolute position sensor here.
// This firmware assumes the mount is physically at Pan=0deg / Tilt=0deg
// at power-on. Add limit switches + a real homing routine before trusting
// AUTO mode on hardware -- see motion.cpp HOMING NOTE.
#define TILT_MIN_DEG   0.0f
#define TILT_MAX_DEG   90.0f
#define PAN_MIN_DEG    0.0f
#define PAN_MAX_DEG    360.0f   // continuous-rotation pan stage assumed; if
                                  // yours is geared/limited, tighten this and
                                  // motion.cpp will clamp + report out-of-range.

// ---------- RGB status LED (common-cathode, PWM) ----------
// Avoid ESP32 strapping pins (0, 2, 5, 12, 15).
#define LED_R_PIN      4
#define LED_G_PIN      18
#define LED_B_PIN      19

// ---------- UART2 link to the CYD slave ----------
// Generic DevKit boards are free to use the default UART2 pins.
#define LINK_RX_PIN    16
#define LINK_TX_PIN    17
#define LINK_BAUD      115200

// ---------- Observer default location (OFFLINE calc fallback) ----------
#define DEFAULT_LAT_DEG   35.68
#define DEFAULT_LON_DEG   139.77

// ---------- Timing ----------
#define BT_SYNC_INTERVAL_MS     (2UL * 60UL * 1000UL)  // 2-minute API cadence
#define BT_STALE_TIMEOUT_MS     (5UL * 60UL * 1000UL)  // no packet -> alert
#define CONTROL_LOOP_INTERVAL_MS 20UL                   // ~50 Hz stepper service
#define STATUS_BROADCAST_INTERVAL_MS 150UL              // -> CYD update rate

// ---------- Simulation helper ----------
// Type e.g. {"azimuth":180.0,"altitude":45.0} or 35.68,139.77,1755000000
// into the Serial Monitor and it's treated as an incoming Bluetooth packet.
// Useful in Wokwi, which can't pair a real phone over classic Bluetooth.
#define ALLOW_SERIAL_AS_BT_INPUT

// ---------- Fixed deep-sky target ----------
// Galactic Center (Sgr A*), J2000 equatorial coordinates -- stands in for
// "Milky Way Center / Scorpius" since it needs no orbital math, just RA/Dec.
#define GALACTIC_CENTER_RA_DEG   266.4168
#define GALACTIC_CENTER_DEC_DEG  -29.0078
