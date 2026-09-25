// astro.cpp -- Master MCU
#include "astro.h"
#include <math.h>

static const double DEG2RAD = M_PI / 180.0;
static const double RAD2DEG = 180.0 / M_PI;

double norm360(double deg) {
    double r = fmod(deg, 360.0);
    if (r < 0) r += 360.0;
    return r;
}

double toJulianDate(int year, int month, int day, int hour, int minute, double second) {
    if (month <= 2) { year -= 1; month += 12; }
    int A = year / 100;
    int B = 2 - A + A / 4;
    double dayFrac = day + (hour + minute / 60.0 + second / 3600.0) / 24.0;
    double jd = floor(365.25 * (year + 4716)) + floor(30.6001 * (month + 1)) + dayFrac + B - 1524.5;
    return jd;
}

static double gmstDegrees(double jd) {
    double T = (jd - 2451545.0) / 36525.0;
    double gmst = 280.46061837 + 360.98564736629 * (jd - 2451545.0)
                  + 0.000387933 * T * T - (T * T * T) / 38710000.0;
    return norm360(gmst);
}

double lstDegrees(double jd, double lonDeg) {
    return norm360(gmstDegrees(jd) + lonDeg);
}

void equatorialToHorizontal(double raDeg, double decDeg, double latDeg, double lstDeg,
                             double &altDeg, double &azDeg) {
    double H = DEG2RAD * norm360(lstDeg - raDeg);
    double dec = DEG2RAD * decDeg;
    double lat = DEG2RAD * latDeg;

    double sinAlt = sin(dec) * sin(lat) + cos(dec) * cos(lat) * cos(H);
    double alt = asin(sinAlt);

    double cosAz = (sin(dec) - sin(alt) * sin(lat)) / (cos(alt) * cos(lat));
    if (cosAz > 1.0) cosAz = 1.0;
    if (cosAz < -1.0) cosAz = -1.0;
    double az = acos(cosAz);
    if (sin(H) > 0) az = 2 * M_PI - az;

    altDeg = alt * RAD2DEG;
    azDeg = az * RAD2DEG;
}

void moonRaDec(double jd, double &raDeg, double &decDeg, double &distEarthRadii) {
    double d = jd - 2451543.5;

    double N = norm360(125.1228 - 0.0529538083 * d);
    double i = 5.1454;
    double w = norm360(318.0634 + 0.1643573223 * d);
    double a = 60.2666;
    double e = 0.054900;
    double M = norm360(115.3654 + 13.0649929509 * d);

    double Mrad = DEG2RAD * M;
    double E = Mrad + e * sin(Mrad) * (1.0 + e * cos(Mrad));
    for (int iter = 0; iter < 6; iter++) {
        double dE = (E - e * sin(E) - Mrad) / (1 - e * cos(E));
        E -= dE;
        if (fabs(dE) < 1e-9) break;
    }

    double xv = a * (cos(E) - e);
    double yv = a * (sqrt(1.0 - e * e) * sin(E));
    double v = atan2(yv, xv) * RAD2DEG;
    double r = sqrt(xv * xv + yv * yv);

    double Nr = DEG2RAD * N, ir = DEG2RAD * i, vw = DEG2RAD * (v + w);
    double xeclip = r * (cos(Nr) * cos(vw) - sin(Nr) * sin(vw) * cos(ir));
    double yeclip = r * (sin(Nr) * cos(vw) + cos(Nr) * sin(vw) * cos(ir));
    double zeclip = r * (sin(vw) * sin(ir));

    double lon = norm360(atan2(yeclip, xeclip) * RAD2DEG);
    double lat = atan2(zeclip, sqrt(xeclip * xeclip + yeclip * yeclip)) * RAD2DEG;

    double eps = 23.4393 - 3.563e-7 * d;
    double epsR = DEG2RAD * eps, lonR = DEG2RAD * lon, latR = DEG2RAD * lat;

    double ra = atan2(sin(lonR) * cos(epsR) - tan(latR) * sin(epsR), cos(lonR)) * RAD2DEG;
    double dec = asin(sin(latR) * cos(epsR) + cos(latR) * sin(epsR) * sin(lonR)) * RAD2DEG;

    raDeg = norm360(ra);
    decDeg = dec;
    distEarthRadii = r;
}
