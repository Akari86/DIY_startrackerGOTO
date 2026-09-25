// led_status.cpp -- Master MCU
// Plain digitalWrite on/off per channel (no PWM dimming) so this doesn't
// depend on which ledc API your installed ESP32 core version has -- that
// API changed between core 2.x and 3.x and is a common source of firmware
// that "compiled fine yesterday." Swap in ledcAttach/ledcWrite later if you
// want smoother color mixing or brightness control.
#include "led_status.h"
#include "config.h"
#include <Arduino.h>

static LedStatus currentStatus = LED_MANUAL;
static unsigned long lastBlinkMs = 0;
static bool blinkOn = false;

static void writeColor(bool r, bool g, bool b) {
    digitalWrite(LED_R_PIN, r ? HIGH : LOW);
    digitalWrite(LED_G_PIN, g ? HIGH : LOW);
    digitalWrite(LED_B_PIN, b ? HIGH : LOW);
}

void ledInit() {
    pinMode(LED_R_PIN, OUTPUT);
    pinMode(LED_G_PIN, OUTPUT);
    pinMode(LED_B_PIN, OUTPUT);
    writeColor(false, false, false);
}

void ledSetStatus(LedStatus status) {
    currentStatus = status;
    if (status != LED_WAIT_DATA) blinkOn = false; // reset blink state on exit
}

void ledService() {
    switch (currentStatus) {
        case LED_MANUAL:
            writeColor(true, true, false);  // orange (R+G)
            break;
        case LED_TRACKING:
            writeColor(false, true, false); // green
            break;
        case LED_ALERT:
            writeColor(true, false, false); // red
            break;
        case LED_WAIT_DATA: {
            unsigned long now = millis();
            if (now - lastBlinkMs >= 400) {
                lastBlinkMs = now;
                blinkOn = !blinkOn;
            }
            writeColor(false, false, blinkOn);  // blinking blue
            break;
        }
    }
}
