// bt_link.h -- Master MCU
// Classic Bluetooth SPP link to a phone. NOTE: classic SPP is Android-only
// -- iOS does not support it for third-party apps. If the companion app
// needs to run on iOS, swap this for ESP32 BLE (different API, same idea:
// feed complete lines into btLinkParseLine()).
//
// Wire protocol -- one line per packet, newline terminated:
//   Online (API pre-calculated angles), JSON:
//     {"azimuth":181.4,"altitude":52.1}
//   Offline (local ephemeris calc), CSV:
//     35.6812,139.7671,1755000000        <- lat,lon,unix_utc_seconds
//
// Adjust the JSON key names in bt_link.cpp to match whatever free API /
// phone app you actually wire up -- "azimuth"/"altitude" here are a
// placeholder convention, not a real API's schema.
#pragma once
#include <Arduino.h>

enum BtPacketType { BT_NONE, BT_ONLINE_ANGLES, BT_OFFLINE_LOCATION };

struct BtPacket {
    BtPacketType type = BT_NONE;
    double azDeg = 0, altDeg = 0;          // valid when type == BT_ONLINE_ANGLES
    double lat = 0, lon = 0;                // valid when type == BT_OFFLINE_LOCATION
    unsigned long unixTime = 0;             // valid when type == BT_OFFLINE_LOCATION
};

void btLinkInit();

// Non-blocking: call every loop. Returns true exactly on the call where a
// full, valid line was assembled and parsed; outPacket is only written
// then. Malformed lines are silently dropped (counts against the stale
// -data timeout the same as no data at all).
bool btLinkPoll(BtPacket &outPacket);
