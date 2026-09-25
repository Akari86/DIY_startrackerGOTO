#pragma once
// ESP32-2432S028R: TFT and XPT2046 have separate SPI wiring.
constexpr int TOUCH_CLK=25, TOUCH_MISO=39, TOUCH_MOSI=32, TOUCH_CS=33, TOUCH_IRQ=36;
// XPT2046 pressure polling; affine calibration learns swap/flip/range on-device.
// First boot asks for 3 corners + a center verification and saves them in NVS.
constexpr int IMU_SDA=27, IMU_SCL=22, IMU_ADDRESS=0x29;
// Mount the IMU with its +X direction along the optical axis, then calibrate
// azimuth/altitude offsets on the phone. Heading must reference true north.
constexpr int BATTERY_ADC=-1, CHARGE_PIN=-1; // disabled until wiring is known
constexpr int CHARGE_ACTIVE=LOW;
constexpr float BATTERY_DIVIDER=2.0f, BATTERY_EMPTY=3.3f, BATTERY_FULL=4.2f;
// Optional UART instead of TCP. Do not reuse IMU/touch/TFT pins.
constexpr bool LX_SERIAL=false;
constexpr int LX_RX=-1, LX_TX=-1, LX_BAUD=9600;
constexpr const char *AP_NAME="StarTracker", *AP_PASSWORD="startrack24";
