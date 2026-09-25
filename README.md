# DIY_startrackerGOTO

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![Platform: ESP32](https://img.shields.io/badge/Platform-ESP32-green.svg)](https://www.espressif.com/en/products/socs/esp32)

> **⚠️ Project Status: Experimental / Demo Phase**
> *This project is currently in the active prototyping and development stage. While core functionalities such as dual-microcontroller communication and basic kinematics are operational, the firmware and hardware configurations may contain bugs, lack final calibration, or exhibit instability. It is not yet fully optimized for production-level astrophotography. Future updates will focus on algorithmic refinement and system stability. Contributions, testing, and feedback are highly welcomed.*

## 1. Project Overview

This repository contains the firmware and documentation for a custom-built, GoTo-capable equatorial star tracking mount. Originally developed as a university engineering capstone project, this system is now open-sourced to provide a foundation for custom astrophotography mounts and motorized tracking applications.

The system utilizes a dual-microcontroller architecture to separate the graphical user interface (GUI) processing from real-time stepper motor kinematics, ensuring deterministic motor pulse generation and zero UI-induced jitter.

## 2. Technical Specifications

### 2.1 Hardware Architecture
The control system is distributed across two discrete ESP32 microcontrollers:
- **Display Controller (HMI):** ESP32-2432S028R (Cheap Yellow Display - CYD) featuring a 2.8" TFT touchscreen interface.
- **Motion Controller (Main Board):** Standard ESP32 NodeMCU/DevKit.
- **Actuators:** NEMA 17 Stepper Motors (Right Ascension and Declination axes).
- **Motor Drivers:** Compatible with standard step/dir drivers (e.g., TMC2209, A4988).
- **Sensors:** 
  - Hall Effect sensors for precise hardware zero-positioning (Homing).
  - BNO055 9-DOF IMU for orientation and leveling metrics (Optional).

### 2.2 Software Architecture
- **FreeRTOS Integration:** The Motion Controller leverages the ESP32's dual-core architecture. Core 1 is strictly dedicated to high-frequency stepper motor pulse generation, while Core 0 handles serial parsing and asynchronous tasks.
- **Coordinate System:** Implements an internal celestial catalog for offline GoTo operations, eliminating the requirement for active network connections in the field.
- **Tracking Rates:** Mathematically derived tracking speeds for Sidereal, Lunar, and Solar rates.

## 3. System Topology & Communication

The Display Controller and Motion Controller communicate asynchronously via UART (`Serial2`).

**Serial Configuration:**
- **Baud Rate:** 115200 bps
- **Protocol:** Custom ASCII string commands (e.g., `M:SIDEREAL`, `C:HOME`, `H:DONE`)

**Wiring Diagram:**
| ESP32-2432S028R (CYD) | ESP32 (Motion Controller) |
| :--- | :--- |
| TX (Transmission) | RX (Reception) |
| RX (Reception) | TX (Transmission) |
| GND (Logic Ground) | GND (Common Logic Ground) |

*Warning: Galvanic isolation is not implemented by default. Ensure a robust common logic ground between both microcontrollers to prevent UART frame errors.*

## 4. Installation & Deployment

The firmware is developed and managed using the PlatformIO ecosystem.

### 4.1 Motion Controller Deployment
1. Open the `MainBoard` directory in PlatformIO.
2. Compile and flash the firmware to the primary ESP32 module.

### 4.2 Display Controller (HMI) Deployment
1. Open the `CYD_Display` directory in PlatformIO.
2. **Configuration Requirement:** The `TFT_eSPI` library requires hardware-specific pin mapping. You must replace the default `User_Setup.h` within the library directory with the configuration specific to the ESP32-2432S028R (ILI9341 driver). Failure to do so will result in a blank display.
3. Compile and flash the firmware to the CYD module.

## 5. Operational Procedures

- **Initialization:** Upon power-up, the system initializes the HMI and awaits user input.
- **Homing Sequence:** Initiating the homing command engages the motors at high traversal speeds until the physical Hall Effect limit switches are triggered. The internal step counters are subsequently zeroed, establishing an absolute mechanical reference frame.
- **GoTo Execution:** Users may select a target from the internal catalog and define an exposure duration via the touchscreen interface. The Motion Controller will slew to the calculated Right Ascension and Declination coordinates and automatically transition into Sidereal tracking.
- **Manual Override:** The control loop can be interrupted at any time via software stop commands, triggering an immediate deceleration of all active axes.

## 6. License

This project is licensed under the GNU General Public License v3.0. You are permitted to use, modify, and distribute the software, provided that any derivative works are also open-sourced under the same license. See the `LICENSE` file for complete terms and conditions.
