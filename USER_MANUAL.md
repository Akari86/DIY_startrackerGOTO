# DIY_startrackerGOTO: Operator Manual

## 1. Introduction
This document serves as the official operating manual for the DIY_startrackerGOTO system. It outlines the operational procedures for utilizing the HMI (Human-Machine Interface) via the ESP32-2432S028R (CYD) touchscreen and explains the corresponding mechanical behaviors executed by the Main Motor Controller.

---

## 2. Interface Overview
The system boots directly into the **Home Menu**, which is the central hub for all tracker operations. The interface is divided into four primary interactive zones accessible via the top navigation bar or direct screen tap:
1. **Homing (Radar/House Icon):** Initiates mechanical zero-positioning.
2. **Quick Tracking (Star Icon):** Accesses standard tracking rate profiles.
3. **GoTo Catalog (Book/List Icon):** Accesses the offline celestial database for automated targeting.
4. **Settings (Gear Icon):** System configuration and display adjustments.

---

## 3. Operational Procedures

### 3.1 Startup & Initialization
1. Ensure the stepper motors are free from physical obstruction.
2. Power on the Main Board and the CYD Display. 
3. The display will show a boot sequence and establish UART serial communication with the Main Board.
4. Once initialized, the system will enter the **Home Menu**.

### 3.2 Homing Calibration (Mechanical Zeroing)
*It is highly recommended to perform Homing prior to any GoTo operations to establish an accurate mechanical reference frame.*
1. From the Home Menu, tap the **Homing Icon**.
2. The screen will display `HOMING...` and lock user input.
3. **Mechanical Behavior:** The Main Board will engage both the Right Ascension (RA) and Declination (DEC) motors at high traversal speed (`SPEED_HOMING`).
4. The motors will continue to slew until the physical Hall Effect limit switches are triggered (logic `LOW`).
5. Upon triggering, the internal coordinate counters are reset to zero. The system will report `H:DONE` to the display, and the interface will automatically return to the Home Menu.

### 3.3 Quick Tracking (Constant Rate)
Use this mode for general astrophotography where the telescope is already manually aligned to a target.
1. Tap the **Star Icon** to enter the Quick Tracking menu.
2. Select one of the predefined kinematic profiles:
   - **Sidereal (Stars):** Standard Earth rotation compensation.
   - **Lunar (Moon):** Adjusted rate for lunar motion.
   - **Solar (Sun):** Adjusted rate for solar tracking.
3. **Mechanical Behavior:** The Main Board will instantly apply the selected frequency to the RA motor driver. The mount will continuously track until a manual cancellation is issued.
4. Tap the **Cancel/Stop** button on the UI to halt the motors.

### 3.4 GoTo Catalog Tracking (Automated Targeting)
Use this mode to automatically slew the mount to a specific celestial object from the internal database.
1. Tap the **Catalog Icon**.
2. Browse the celestial targets (e.g., Orion Nebula, Andromeda).
3. Tap the desired target. The system will transition to the **Timer/Confirmation** interface.
4. **Tumbler UI:** Use the kinetic scrolling interface (swipe up/down) to adjust the tracking duration (Hours and Minutes). 
   - *Note: The system automatically calculates and displays the Estimated Finish Time based on the local timezone.*
5. Tap **START TRACKING**.
6. **Mechanical Behavior:** 
   - The system calculates the shortest path from the current position to the target RA/DEC coordinates.
   - Both motors will slew at maximum safe velocity to the target (GoTo phase).
   - Once the target coordinates are reached, the system automatically seamlessly transitions into Sidereal tracking mode.
7. Upon completion of the specified timer duration, the tracking halts, the display dims, and a **"TRACKING COMPLETE"** notification is presented.

### 3.5 System Settings
Access the Settings menu via the **Gear Icon**.
- **Display Brightness:** Toggles the TFT backlight PWM between standard and dim states to preserve night vision.
- **Night Mode:** Inverts the UI color palette to red/black, eliminating blue light emission for optimal dark-site adaptation.
- **IMU Calibration:** Sends a command (`C:CALIMU`) to reset/calibrate the BNO055 inertial measurement unit (if installed).

---

## 4. Safety & Emergency Stop
- **Manual Halt:** Any active motor operation (Homing, GoTo, or Tracking) can be immediately aborted by tapping the **CANCEL** button on the touchscreen.
- **Physical Disconnect:** If the mount approaches physical collision limits and the software fails to respond, immediately disconnect the primary DC power supply to the stepper drivers.

## 5. Troubleshooting
- **No Response from Motors:** Ensure the TX/RX crossover connection between the CYD and Main Board is secure. Check the common logic ground.
- **Homing Never Completes:** Verify the Hall Effect sensors are properly seated and passing a logic `LOW` state to the ESP32 GPIO pins when a magnet is present.
- **Jumpy/Erratic Scrolling:** Ensure the screen is clean. The kinetic scrolling tumbler relies on smooth delta-Y touch calculation.
