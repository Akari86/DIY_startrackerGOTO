#pragma once
#include <cmath>
#include <cstdint>
#include <cstdlib>
namespace TouchMath {
struct Point {float x,y;};
struct Calibration {
  float ax=0,bx=0,cx=0,ay=0,by=0,cy=0;
  Point map(Point p) const {return {ax*p.x+bx*p.y+cx,ay*p.x+by*p.y+cy};}
  bool valid() const {
    const float v[]={ax,bx,cx,ay,by,cy};
    for(float n:v)if(!std::isfinite(n))return false;
    return std::fabs(ax*by-ay*bx)>0.0001f && std::fabs(ax)<1 && std::fabs(bx)<1 && std::fabs(ay)<1 && std::fabs(by)<1;
  }
  bool fit(const Point raw[3],const Point screen[3]) {
    float x0=raw[0].x,y0=raw[0].y,x1=raw[1].x,y1=raw[1].y,x2=raw[2].x,y2=raw[2].y;
    float d=x0*(y1-y2)+x1*(y2-y0)+x2*(y0-y1);
    if(std::fabs(d)<100000)return false;
    auto solve=[&](float z0,float z1,float z2,float &a,float &b,float &c) {
      a=(z0*(y1-y2)+z1*(y2-y0)+z2*(y0-y1))/d;
      b=(z0*(x2-x1)+z1*(x0-x2)+z2*(x1-x0))/d;
      c=(z0*(x1*y2-x2*y1)+z1*(x2*y0-x0*y2)+z2*(x0*y1-x1*y0))/d;
    };
    solve(screen[0].x,screen[1].x,screen[2].x,ax,bx,cx);
    solve(screen[0].y,screen[1].y,screen[2].y,ay,by,cy);
    return valid();
  }
};
struct Event {bool tap=false,drag=false;int x=0,y=0,dy=0;};
// Press/release debounce, small ADC jitter tolerated, swipes never become taps.
class Gesture {
  bool down=false,moved=false;
  int firstX=0,firstY=0,lastY=0,count=0;
  uint32_t pressed=0,lastSeen=0;
 public:
  void reset(){down=false;moved=false;count=0;}
  Event update(bool touching,int x,int y,uint32_t now) {
    Event e;
    if(touching) {
      lastSeen=now;
      if(!down){down=true;moved=false;firstX=x;firstY=y;lastY=y;pressed=now;count=1;}
      else {
        count++;
        if(std::abs(x-firstX)>14||std::abs(y-firstY)>14)moved=true;
        if(moved){e.drag=true;e.x=firstX;e.y=firstY;e.dy=lastY-y;}
        lastY=y;
      }
    } else if(down && uint32_t(now-lastSeen)>=70) {
      if(!moved&&count>=2&&uint32_t(lastSeen-pressed)>=16){e.tap=true;e.x=firstX;e.y=firstY;}
      reset();
    }
    return e;
  }
};
}
