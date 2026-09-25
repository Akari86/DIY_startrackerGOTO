// uart_link.h -- Master MCU
// Serial2 link to the CYD slave board. Newline-delimited text so a dropped
// byte just corrupts one line instead of desyncing the link permanently.
//
// Master -> CYD, every STATUS_BROADCAST_INTERVAL_MS:
//   S,<mode 0|1>,<target 0|1>,<remaining_sec>,<az_deg>,<alt_deg>,<led 0-3>\n
// CYD -> Master, on touch:
//   CMD,SWITCH_MODE\n
//   CMD,SWITCH_TARGET\n
#pragma once
#include <Arduino.h>

enum UartCommand { CMD_NONE, CMD_SWITCH_MODE, CMD_SWITCH_TARGET };

void uartLinkInit();
void uartLinkSendStatus(uint8_t mode, uint8_t target, long remainingSec,
                         float azDeg, float altDeg, uint8_t ledState);
// Non-blocking, call every loop. Returns CMD_NONE most of the time.
UartCommand uartLinkPollCommand();
