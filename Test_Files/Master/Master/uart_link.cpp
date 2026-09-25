// uart_link.cpp -- Master MCU
#include "uart_link.h"
#include "config.h"

static String lineBuf;

void uartLinkInit() {
    Serial2.begin(LINK_BAUD, SERIAL_8N1, LINK_RX_PIN, LINK_TX_PIN);
    lineBuf.reserve(64);
}

void uartLinkSendStatus(uint8_t mode, uint8_t target, long remainingSec,
                         float azDeg, float altDeg, uint8_t ledState) {
    Serial2.print("S,");
    Serial2.print(mode);
    Serial2.print(',');
    Serial2.print(target);
    Serial2.print(',');
    Serial2.print(remainingSec);
    Serial2.print(',');
    Serial2.print(azDeg, 2);
    Serial2.print(',');
    Serial2.print(altDeg, 2);
    Serial2.print(',');
    Serial2.print(ledState);
    Serial2.print('\n');
}

UartCommand uartLinkPollCommand() {
    while (Serial2.available()) {
        char c = (char)Serial2.read();
        if (c == '\n' || c == '\r') {
            String line = lineBuf;
            lineBuf = "";
            line.trim();
            if (line == "CMD,SWITCH_MODE") return CMD_SWITCH_MODE;
            if (line == "CMD,SWITCH_TARGET") return CMD_SWITCH_TARGET;
            // anything else (partial/garbled line) is silently dropped
        } else if (lineBuf.length() < 60) {
            lineBuf += c;
        }
    }
    return CMD_NONE;
}
