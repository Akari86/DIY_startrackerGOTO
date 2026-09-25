// ===================================================================
// MASTER MCU (ESP32 Dev Kit) -- Smart Celestial Tracker
// Kinematics, astrometry + interpolation, timer, Bluetooth, UART link
// to a CYD (Cheap Yellow Display) slave board that owns the touch UI.
//
// Required libraries (Arduino Library Manager):
//   AccelStepper      (Mike McCauley)
//   ArduinoJson        (Benoit Blanchon)
//   BluetoothSerial    (bundled with the ESP32 Arduino core)
//
// This follows the master/slave pseudocode you provided, with a few
// deliberate deviations called out inline:
//   1. Interpolation corrects BOTH azimuth and altitude (the pseudocode
//      only nudged azimuth). Altitude drifts too unless the target is
//      exactly on the meridian.
//   2. Online-mode interpolation derives an actual rate from two
//      consecutive API samples instead of a generic "earth drift"
//      constant -- more accurate, and it naturally handles the Moon's
//      own motion on top of sidereal rotation.
//   3. Steppers are open-loop -- see the HOMING NOTE in motion.cpp.
// ===================================================================

#include <math.h>
#include <time.h>
#include "config.h"
#include "astro.h"
#include "motion.h"
#include "bt_link.h"
#include "led_status.h"
#include "uart_link.h"

enum SystemState { STATE_MANUAL, STATE_AUTO };
enum TargetObject { TARGET_MOON, TARGET_MILKY_WAY };

static SystemState currentState = STATE_MANUAL;
static SystemState prevState = STATE_MANUAL;
static TargetObject selectedTarget = TARGET_MOON;
static TargetObject prevTarget = TARGET_MOON;

// ---- Timer ----
static unsigned long trackingDurationMs = 0;
static unsigned long autoStartMs = 0;
static bool autoTimerRunning = false;
static bool autoTimerExpired = false;

// ---- Sync / tracking state ----
static bool haveEverSynced = false;      // got at least one BT packet this AUTO session
static bool usingOnlineSource = true;    // which branch of the dual system is active
static unsigned long lastSyncMs = 0;

// Online-mode interpolation baseline (two most recent samples -> rate)
static bool haveOnlinePrev = false;
static double prevOnlineAz = 0, prevOnlineAlt = 0;
static unsigned long prevOnlineMs = 0;
static double onlineBaseAz = 0, onlineBaseAlt = 0;
static double azRatePerMs = 0, altRatePerMs = 0;
static unsigned long onlineBaseMs = 0;

// Offline-mode reference (lat/lon/time from the last "L" packet)
static double offlineLat = DEFAULT_LAT_DEG;
static double offlineLon = DEFAULT_LON_DEG;
static unsigned long offlineRefUnixTime = 0;
static unsigned long offlineRefMillis = 0;
static bool haveOfflineRef = false;

static double targetAzDeg = 0, targetAltDeg = 0;

// ---------------------------------------------------------------
static double shortestAzDelta(double fromDeg, double toDeg) {
    double d = fmod(toDeg - fromDeg + 540.0, 360.0) - 180.0;
    return d;
}

static void resetAutoSession() {
    haveEverSynced = false;
    haveOnlinePrev = false;
    haveOfflineRef = false;
    autoTimerRunning = false;
    autoTimerExpired = false;
    azRatePerMs = 0;
    altRatePerMs = 0;
}

// ---------------------------------------------------------------
// PROCESS_STATE_AND_TIMER equivalent
// ---------------------------------------------------------------
static void handleModeAndTimer() {
    UartCommand cmd = uartLinkPollCommand();
    if (cmd == CMD_SWITCH_MODE) {
        currentState = (currentState == STATE_MANUAL) ? STATE_AUTO : STATE_MANUAL;
    } else if (cmd == CMD_SWITCH_TARGET) {
        selectedTarget = (selectedTarget == TARGET_MOON) ? TARGET_MILKY_WAY : TARGET_MOON;
    }

    // Mode/target transition edges
    if (currentState != prevState) {
        if (currentState == STATE_MANUAL) {
            motionSyncCurrentFromSteppers(); // trust wherever the joystick left it
        } else {
            resetAutoSession(); // fresh AUTO session: wait for first BT packet again
        }
        prevState = currentState;
    }
    if (selectedTarget != prevTarget) {
        // A different body has completely different Az/Alt -- any stored
        // online-mode rate/baseline no longer applies.
        haveOnlinePrev = false;
        azRatePerMs = 0;
        altRatePerMs = 0;
        prevTarget = selectedTarget;
    }

    // Potentiometer -> tracking duration, 0-120 minutes, lightly smoothed
    // against ESP32 ADC noise.
    static float potSmoothed = 0;
    int potRaw = analogRead(POT_PIN);
    potSmoothed += (potRaw - potSmoothed) * 0.1f;
    long minutes = map((long)potSmoothed, 0, 4095, 0, 120);
    trackingDurationMs = (unsigned long)minutes * 60000UL;

    // Timer countdown, only meaningful once AUTO tracking has actually started
    if (autoTimerRunning) {
        unsigned long elapsed = millis() - autoStartMs;
        if (elapsed >= trackingDurationMs) {
            autoTimerRunning = false;
            autoTimerExpired = true;
            motionStop();
        }
    }

    // LED status
    if (currentState == STATE_MANUAL) {
        ledSetStatus(LED_MANUAL);
    } else if (autoTimerExpired) {
        ledSetStatus(LED_ALERT);
    } else if (!haveEverSynced) {
        ledSetStatus(LED_WAIT_DATA);
    } else if (millis() - lastSyncMs > BT_STALE_TIMEOUT_MS) {
        // Data went stale mid-track (no new BT packet in a long while).
        // Reusing LED_ALERT here too -- see note in astro comments below.
        ledSetStatus(LED_ALERT);
    } else {
        ledSetStatus(LED_TRACKING);
    }
}

// ---------------------------------------------------------------
// PROCESS_ASTROMETRY_INTERPOLATION equivalent
// ---------------------------------------------------------------
static void computeOfflineAzAlt(double &outAz, double &outAlt) {
    double elapsedSec = (millis() - offlineRefMillis) / 1000.0;
    unsigned long estUnixTime = offlineRefUnixTime + (unsigned long)elapsedSec;

    // Convert unix time -> a Julian Date via a plain UTC calendar breakdown.
    time_t t = (time_t)estUnixTime;
    struct tm *utc = gmtime(&t);
    double jd = toJulianDate(utc->tm_year + 1900, utc->tm_mon + 1, utc->tm_mday,
                              utc->tm_hour, utc->tm_min, (double)utc->tm_sec);

    double raDeg, decDeg;
    if (selectedTarget == TARGET_MOON) {
        double dist;
        moonRaDec(jd, raDeg, decDeg, dist);
    } else {
        raDeg = GALACTIC_CENTER_RA_DEG;
        decDeg = GALACTIC_CENTER_DEC_DEG;
    }

    double lst = lstDegrees(jd, offlineLon);
    equatorialToHorizontal(raDeg, decDeg, offlineLat, lst, outAlt, outAz);
}

static void handleAstrometry() {
    if (currentState != STATE_AUTO) return;

    BtPacket pkt;
    if (btLinkPoll(pkt)) {
        haveEverSynced = true;
        lastSyncMs = millis();
        if (!autoTimerRunning && !autoTimerExpired) {
            autoStartMs = millis(); // countdown starts at first sync, not at mode-switch
            autoTimerRunning = true;
        }

        if (pkt.type == BT_ONLINE_ANGLES) {
            usingOnlineSource = true;
            if (haveOnlinePrev) {
                unsigned long dtMs = lastSyncMs - prevOnlineMs;
                if (dtMs > 0) {
                    double dAz = shortestAzDelta(prevOnlineAz, pkt.azDeg);
                    double dAlt = pkt.altDeg - prevOnlineAlt;
                    azRatePerMs = dAz / (double)dtMs;
                    altRatePerMs = dAlt / (double)dtMs;
                }
            } else {
                azRatePerMs = 0; // no second sample yet -- hold static till next sync
                altRatePerMs = 0;
            }
            prevOnlineAz = pkt.azDeg;
            prevOnlineAlt = pkt.altDeg;
            prevOnlineMs = lastSyncMs;
            haveOnlinePrev = true;

            onlineBaseAz = pkt.azDeg;
            onlineBaseAlt = pkt.altDeg;
            onlineBaseMs = lastSyncMs;

        } else if (pkt.type == BT_OFFLINE_LOCATION) {
            usingOnlineSource = false;
            offlineLat = pkt.lat;
            offlineLon = pkt.lon;
            offlineRefUnixTime = pkt.unixTime;
            offlineRefMillis = millis();
            haveOfflineRef = true;
        }
    }

    if (!haveEverSynced) return; // nothing to track yet

    if (usingOnlineSource) {
        unsigned long elapsedMs = millis() - onlineBaseMs;
        double az = onlineBaseAz + azRatePerMs * elapsedMs;
        double alt = onlineBaseAlt + altRatePerMs * elapsedMs;
        targetAzDeg = fmod(fmod(az, 360.0) + 360.0, 360.0);
        targetAltDeg = alt;
    } else if (haveOfflineRef) {
        double az, alt;
        computeOfflineAzAlt(az, alt);
        targetAzDeg = az;
        targetAltDeg = alt;
    }

    if (targetAltDeg < TILT_MIN_DEG) targetAltDeg = TILT_MIN_DEG;
    if (targetAltDeg > TILT_MAX_DEG) targetAltDeg = TILT_MAX_DEG;

    bool panOOR, tiltOOR;
    motionCommandTarget(targetAzDeg, targetAltDeg, panOOR, tiltOOR);
    // panOOR/tiltOOR intentionally unused here -- surfaced to the CYD via
    // the status line's existing fields would need an extra flag byte;
    // left as a documented follow-up rather than silently ignored forever.
}

// ---------------------------------------------------------------
// EXECUTE_KINEMATICS equivalent
// ---------------------------------------------------------------
static void handleKinematics() {
    if (currentState == STATE_MANUAL) {
        motionServiceManualJog(analogRead(JOY_X_PIN), analogRead(JOY_Y_PIN));
    } else {
        motionServiceAutoRun();
    }
}

// ---------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    analogReadResolution(12);
    pinMode(JOY_BTN_PIN, INPUT_PULLUP);

    motionInit();
    ledInit();
    ledSetStatus(LED_MANUAL);
    btLinkInit();
    uartLinkInit();
}

void loop() {
    handleModeAndTimer();
    handleAstrometry();
    handleKinematics();   // every iteration, unthrottled -- steppers need it
    ledService();

    static unsigned long lastStatusMs = 0;
    unsigned long now = millis();
    if (now - lastStatusMs >= STATUS_BROADCAST_INTERVAL_MS) {
        lastStatusMs = now;
        long remainingSec = 0;
        if (autoTimerRunning) {
            long elapsedSec = (now - autoStartMs) / 1000;
            remainingSec = (long)(trackingDurationMs / 1000) - elapsedSec;
            if (remainingSec < 0) remainingSec = 0;
        }
        uint8_t ledCode = (currentState == STATE_MANUAL) ? 0
                         : (autoTimerExpired || (haveEverSynced && now - lastSyncMs > BT_STALE_TIMEOUT_MS)) ? 3
                         : (!haveEverSynced) ? 1
                         : 2;
        uartLinkSendStatus((uint8_t)currentState, (uint8_t)selectedTarget, remainingSec,
                           (float)targetAzDeg, (float)targetAltDeg, ledCode);
    }
}
