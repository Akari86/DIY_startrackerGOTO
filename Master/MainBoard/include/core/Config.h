#pragma once
#include <Arduino.h>

// 1. 5-Way Switch (Left side contiguous)
constexpr int BTN_UP = 32;
constexpr int BTN_CENTER = 33;
constexpr int BTN_LEFT = 25;
constexpr int BTN_DOWN = 26;
constexpr int BTN_RIGHT = 27;
 
// 2. TMC2209 RA Axis (Right side contiguous)
constexpr int RA_EN = 19;
constexpr int RA_DIR = 18;
constexpr int RA_STEP = 5;

// 3. TMC2209 DEC Axis (Scattered but safe)
constexpr int DEC_EN = 14;
constexpr int DEC_DIR = 13;
constexpr int DEC_STEP = 4;

// 4. Serial2 for CYD Communication
constexpr int CYD_RX = 16;
constexpr int CYD_TX = 17;
constexpr int CYD_BAUD = 115200;

// 5. BNO055 I2C
constexpr int I2C_SDA = 21;
constexpr int I2C_SCL = 22;

// 6. Hall Effect Sensors (Input Only)
constexpr int RA_HALL = 34;
constexpr int DEC_HALL = 35;

// Motor Configuration Constants
constexpr float STEPS_PER_REV = 200.0;
constexpr float MICROSTEPS = 16.0;
constexpr float GEAR_RATIO_RA = 60.0; // เฟืองหนอน 1:60
constexpr float GEAR_RATIO_DEC = 60.0;

// Tracking Speeds (Steps per second)
// Sidereal day = 86164.0905 seconds
// Steps per degree = (STEPS_PER_REV * MICROSTEPS * GEAR_RATIO_RA) / 360.0
// Sidereal speed = (360.0 / 86164.0905) * Steps per degree
constexpr float SPEED_SIDEREAL = (STEPS_PER_REV * MICROSTEPS * GEAR_RATIO_RA) / 86164.0905;
constexpr float SPEED_LUNAR = (STEPS_PER_REV * MICROSTEPS * GEAR_RATIO_RA) / 89324.4; // approx
constexpr float SPEED_SOLAR = (STEPS_PER_REV * MICROSTEPS * GEAR_RATIO_RA) / 86400.0;
constexpr float SPEED_GOTO = 2000.0; // Max speed for slewing
constexpr float SPEED_HOMING = 2000.0; // Fast speed for homing calibration

enum TrackingMode { TRACK_STOPPED, TRACK_SIDEREAL, TRACK_LUNAR, TRACK_SOLAR, TRACK_GOTO, TRACK_HOMING };
