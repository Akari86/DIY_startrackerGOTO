// astro.h -- Master MCU
// Astrometry helpers, validated standalone before being ported here
// (see the g++ test harness notes in the write-up).
#pragma once
#include <Arduino.h>

double norm360(double deg);
double toJulianDate(int year, int month, int day, int hour, int minute, double second);
double lstDegrees(double jd, double lonDeg);

// Equatorial (RA/Dec, deg) -> Horizontal (Alt/Az, deg). Az from North through East.
void equatorialToHorizontal(double raDeg, double decDeg, double latDeg, double lstDeg,
                             double &altDeg, double &azDeg);

// Low-precision two-body Moon position (RA/Dec, deg) + geocentric distance
// in Earth radii. ~2-3 deg accuracy -- an OFFLINE fallback, not a precision
// ephemeris. Good enough to point a hobby mount; re-sync via the online API
// whenever Bluetooth is available for the real precision.
void moonRaDec(double jd, double &raDeg, double &decDeg, double &distEarthRadii);
