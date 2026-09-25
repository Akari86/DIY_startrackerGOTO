# User Manual - DIY Star Tracker GoTo

This manual provides instructions for operating the touchscreen interface (CYD_Slave) to control the GoTo Star Tracker system.
**CYD Latest Firmware Version: 3.0 (NASA API Integration)**

---

## 1. Home Screen
Upon startup, the system displays the Home Screen with 4 navigation icons at the bottom:
1. **Homing (House Icon) :** Commands the motors to seek their physical zero positions.
2. **Quick Tracking (Clock Icon) :** Engages standard tracking rates without specific coordinates.
3. **Catalog & NASA (Star Icon) :** Selects targets from the offline database or fetches live data from NASA.
4. **Settings (Gear Icon) :** Adjusts display brightness, toggles Night Mode, and calibrates the IMU.

---

## 2. Basic Operations

### 2.1 Homing (CRITICAL: Must be performed on startup)
*The system must establish a mechanical zero point before any GoTo operations can be calculated accurately.*
1. Tap the **Homing** icon.
2. The display will show `HOMING...`
3. Both axes will slew until the Hall Effect sensors are triggered. Upon successful triggering, coordinates zero out and the screen displays `H:DONE`.
4. **Safety Timeout (Homing Abort):** If a motor steps more than 200,000 times (slightly over 360 degrees) without triggering a sensor, the system will execute a hard abort. The display will show an orange "Homing Aborted" popup. This prevents cable snagging and motor damage.

### 2.2 Quick Tracking Mode
Used for standard sidereal tracking without GoTo:
1. Tap the **Quick Tracking** icon.
2. Select the desired tracking rate:
   - **Sidereal:** Standard rate for stars and deep-sky objects.
   - **Lunar:** Tracking rate optimized for the Moon.
   - **Solar:** Tracking rate optimized for the Sun.
3. The Right Ascension (RA) motor will immediately begin tracking. You can tap the **STOP** button on the Home Screen at any time.

### 2.3 Catalog & NASA API Mode (Automated GoTo)
1. Tap the **Catalog** icon. The system will prompt you to select a target source:
   - **APP (Stellarium WiFi):** Wait for external Wi-Fi commands.
   - **NASA HORIZONS (LIVE):** Fetch real-time data from the internet.
   - **OFFLINE (INTERNAL):** Use the internal deep-sky database.
2. **If NASA HORIZONS (LIVE) is selected:**
   - Choose a planetary target (Moon, Mars, Jupiter).
   - The CYD will connect to the NASA JPL Horizons API and fetch the absolute real-time RA/DEC coordinates (Requires Wi-Fi connection).
3. **If OFFLINE is selected:** 
   - Scroll and select an internal target (e.g., Orion Nebula, Andromeda).
4. After target acquisition, the system opens the **Timer** page.
   - Swipe the Hour and Minute tumblers to define your intended tracking duration (Exposure Time).
5. Tap **START TRACKING**.
6. The mount will rapidly slew to the target coordinates and seamlessly transition into the sidereal tracking state until the timer expires.

### 2.4 NASA APOD Screensaver
- Navigate to Catalog -> NASA HORIZONS -> NASA APOD.
- The display will download and render NASA's Astronomy Picture of the Day.
- Tap anywhere on the screen to exit and return to the menu.

---

## 3. System Settings
- **Brightness:** Toggle backlight intensity.
- **Red Mode (Night Mode):** Shifts the entire UI color palette to red to preserve dark adaptation for visual astronomy.
- **Calibrate IMU:** Calibrates the BNO055 9-DOF sensor (if equipped) to establish precise azimuth and altitude references.
