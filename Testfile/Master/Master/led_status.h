// led_status.h -- Master MCU
#pragma once

enum LedStatus {
    LED_MANUAL,       // orange -- manual jog mode
    LED_WAIT_DATA,     // blinking blue -- AUTO, waiting for first BT packet
    LED_TRACKING,      // green -- AUTO, actively tracking
    LED_ALERT          // red -- timer expired / motors halted
};

void ledInit();
void ledSetStatus(LedStatus status);
// Call every loop -- handles the WAIT_DATA blink timing internally.
void ledService();
