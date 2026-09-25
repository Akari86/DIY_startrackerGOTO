#pragma once
#include <cmath>
namespace CompassMath {
constexpr double RAD=0.017453292519943295;
// World north/east directions rotate opposite the device heading.
// The original compass footprint remains x28..132, y106..188 (105x83).
struct Point {int x,y;double depth;};
inline Point project(double east,double north,double up,double yaw,double pitch,double roll) {
  double a=yaw*RAD,p=pitch*RAD,r=roll*RAD;
  double right=east*std::cos(a)-north*std::sin(a);
  double forward=east*std::sin(a)+north*std::cos(a);
  double f=forward*std::cos(p)+up*std::sin(p);
  double z=-forward*std::sin(p)+up*std::cos(p);
  double x=right*std::cos(r)+z*std::sin(r);
  z=-right*std::sin(r)+z*std::cos(r);
  // An oblique view keeps all three rings visible at a level orientation.
  double y=f*0.8660254037844386-z*0.5;
  double depth=f*0.5+z*0.8660254037844386;
  double perspective=2.8/(2.8+depth);
  return {80+(int)std::lround(44*x*perspective),147-(int)std::lround(34*y*perspective),depth};
}
constexpr int ARROW_X=80,ARROW_TIP_Y=134,ARROW_TAIL_Y=158;
}
