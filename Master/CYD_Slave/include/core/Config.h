#pragma once
// ESP32-2432S028R: TFT and XPT2046 have separate SPI wiring.
constexpr int TOUCH_CLK=25, TOUCH_MISO=39, TOUCH_MOSI=32, TOUCH_CS=33, TOUCH_IRQ=36;
// XPT2046 pressure polling; affine calibration learns swap/flip/range on-device.
// First boot asks for 3 corners + a center verification and saves them in NVS.
// Communication with Main Board (38-pin)
constexpr int MAIN_RX = 22; // CYD RX (formerly IMU SCL)
constexpr int MAIN_TX = 27; // CYD TX (formerly IMU SDA)
constexpr int MAIN_BAUD = 115200;

// Tracking Modes
enum TrackingMode { TRACK_SIDEREAL, TRACK_LUNAR, TRACK_SOLAR };

constexpr int BATTERY_ADC=-1, CHARGE_PIN=-1; // disabled until wiring is known
constexpr int CHARGE_ACTIVE=LOW;
constexpr float BATTERY_DIVIDER=2.0f, BATTERY_EMPTY=3.3f, BATTERY_FULL=4.2f;
// Optional UART instead of TCP. Do not reuse IMU/touch/TFT pins.
constexpr bool LX_SERIAL=false;
constexpr int LX_RX=-1, LX_TX=-1, LX_BAUD=9600;
constexpr const char *WIFI_SSID= "Kitti's Galaxy S22 Ultra";
constexpr const char *WIFI_PASSWORD= "32013446";
// ค่าเริ่มต้นเวลาและพิกัด (ประเทศไทย)
constexpr int DEFAULT_TIMEZONE_MINS = 420; // +7 hours
constexpr double DEFAULT_LAT = 13.7563; // Bangkok
constexpr double DEFAULT_LON = 100.5018; // Bangkok
