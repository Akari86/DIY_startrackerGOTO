// bt_link.cpp -- Master MCU
// Requires the "ArduinoJson" library (Benoit Blanchon) for the online
// -angles JSON packets, and the ESP32 core's built-in BluetoothSerial for
// classic SPP.
#include "bt_link.h"
#include "config.h"
#include <BluetoothSerial.h>
#include <ArduinoJson.h>

static BluetoothSerial SerialBT;
static String lineBuf;

void btLinkInit() {
    SerialBT.begin("SkyTracker"); // Bluetooth device name shown when pairing
    lineBuf.reserve(128);
}

static bool parseLine(const String &line, BtPacket &out) {
    String trimmed = line;
    trimmed.trim();
    if (trimmed.length() == 0) return false;

    if (trimmed.charAt(0) == '{') {
        // ---- Online: pre-calculated angles from the phone/API ----
        StaticJsonDocument<192> doc;
        DeserializationError err = deserializeJson(doc, trimmed);
        if (err) return false;
        if (!doc.containsKey("azimuth") || !doc.containsKey("altitude")) return false;

        out.type = BT_ONLINE_ANGLES;
        out.azDeg = doc["azimuth"].as<double>();
        out.altDeg = doc["altitude"].as<double>();
        return true;
    } else {
        // ---- Offline: lat,lon,unixtime for local ephemeris calc ----
        int c1 = trimmed.indexOf(',');
        if (c1 < 0) return false;
        int c2 = trimmed.indexOf(',', c1 + 1);
        if (c2 < 0) return false;

        String latStr = trimmed.substring(0, c1);
        String lonStr = trimmed.substring(c1 + 1, c2);
        String timeStr = trimmed.substring(c2 + 1);
        if (latStr.length() == 0 || lonStr.length() == 0 || timeStr.length() == 0) return false;

        out.type = BT_OFFLINE_LOCATION;
        out.lat = latStr.toDouble();
        out.lon = lonStr.toDouble();
        out.unixTime = (unsigned long)timeStr.toDouble();
        return true;
    }
}

static bool feedChar(char c, BtPacket &out) {
    if (c == '\n' || c == '\r') {
        if (lineBuf.length() == 0) return false; // ignore blank/CRLF pairs
        bool ok = parseLine(lineBuf, out);
        lineBuf = "";
        return ok;
    }
    if (lineBuf.length() < 120) lineBuf += c; // hard cap so a runaway
                                                // stream can't grow unbounded
    return false;
}

bool btLinkPoll(BtPacket &outPacket) {
    while (SerialBT.available()) {
        char c = (char)SerialBT.read();
        if (feedChar(c, outPacket)) return true;
    }

#ifdef ALLOW_SERIAL_AS_BT_INPUT
    while (Serial.available()) {
        char c = (char)Serial.read();
        if (feedChar(c, outPacket)) return true;
    }
#endif

    return false;
}
