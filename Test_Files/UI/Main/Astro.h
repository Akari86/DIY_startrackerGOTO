#pragma once
#include <math.h>
#include <stdio.h>
#include <string.h>
namespace Astro {
constexpr double RAD=0.017453292519943295;
inline double wrap(double x) { x=fmod(x,360.0); return x<0?x+360.0:x; }
inline void equatorial(double az, double alt, double lat, double lon, double unixTime, double &ra, double &dec) {
  double a=az*RAD,h=alt*RAD,p=lat*RAD;
  double s=sin(h)*sin(p)+cos(h)*cos(p)*cos(a);
  dec=asin(fmax(-1.,fmin(1.,s)))/RAD;
  double ha=atan2(-sin(a)*cos(h),sin(h)*cos(p)-cos(h)*sin(p)*cos(a))/RAD;
  double d=unixTime/86400.0+2440587.5-2451545.0;
  double t=d/36525.0;
  double lst=wrap(280.46061837+360.98564736629*d+0.000387933*t*t-t*t*t/38710000.0+lon);
  ra=wrap(lst-ha)/15.0;
}
inline bool parseRA(const char *s) {
  int h,m,z,n=0; double mm;
  if(sscanf(s,"%d:%d:%d%n",&h,&m,&z,&n)==3 && s[n]==0)
    return h>=0&&h<24&&m>=0&&m<60&&z>=0&&z<60;
  n=0;
  return sscanf(s,"%d:%lf%n",&h,&mm,&n)==2&&s[n]==0&&h>=0&&h<24&&mm>=0&&mm<60;
}
inline bool parseDec(const char *s) {
  if(*s!='+'&&*s!='-') return false;
  int d,m,z=0,n=0; char sep,secSep;
  bool ok=sscanf(s+1,"%d%c%d%c%d%n",&d,&sep,&m,&secSep,&z,&n)==5 && s[n+1]==0 && (secSep==':'||secSep=='\'');
  if(!ok) { n=0; z=0; ok=sscanf(s+1,"%d%c%d%n",&d,&sep,&m,&n)==3 && s[n+1]==0; }
  return ok&&(sep=='*'||(unsigned char)sep==223)&&d>=0&&d<=90&&m>=0&&m<60&&z>=0&&z<60&&(d!=90||(m==0&&z==0));
}
}
