#include <cassert>
#include <iostream>
#include "../Lx200.h"
#include "../TouchMath.h"
#include "../CompassMath.h"
void near(double a,double b,double eps=1e-6){assert(fabs(a-b)<eps);}
void load(Lx200 &m) {
  m.host="192.168.4.2";m.select(42);assert(m.busy&&!m.ready);
  assert(Fake::tx.back()==":LM0042#");
  fakeTime+=151;m.tick();assert(Fake::tx.back()==":LI#");
  Fake::feed("M42 Orion#");m.tick();assert(Fake::tx.back()==":Gr#");
  Fake::feed("05:35:17#");m.tick();assert(Fake::tx.back()==":Gd#");
  Fake::feed("-05*23:28#");m.tick();assert(m.ready&&!m.busy);
  for(auto &s:Fake::tx)assert(s!=":MS#");
}
int main(){
  const TouchMath::Point screen[]={{20,20},{299,20},{160,219}};
  // Check normal, mirrored, swapped and rotated panel wiring/calibration.
  for(int swap=0;swap<2;swap++)for(int flipX=0;flipX<2;flipX++)for(int flipY=0;flipY<2;flipY++) {
    auto raw=[&](TouchMath::Point p){float x=200+p.x*11,y=250+p.y*14;if(flipX)x=4095-x;if(flipY)y=4095-y;return swap?TouchMath::Point{y,x}:TouchMath::Point{x,y};};
    TouchMath::Point samples[]={raw(screen[0]),raw(screen[1]),raw(screen[2])};
    TouchMath::Calibration c;assert(c.fit(samples,screen));
    // Top nav centers, back button and bottom-right: all must map to actual UI.
    for(auto p:{TouchMath::Point{35,30},{105,30},{175,30},{244,30},{299,30},{25,35},{160,120},{315,235}}) {
      auto q=c.map(raw(p));near(q.x,p.x,0.001);near(q.y,p.y,0.001);
    }
  }
  TouchMath::Calibration bad;TouchMath::Point same[]={{100,100},{100,100},{100,100}};assert(!bad.fit(same,screen));
  TouchMath::Gesture gesture;
  gesture.update(true,175,30,0);gesture.update(true,181,35,20);gesture.update(false,0,0,40);
  auto ev=gesture.update(false,0,0,100);assert(ev.tap&&ev.x==175&&ev.y==30);
  assert(!gesture.update(false,0,0,200).tap); // no repeated tap after release
  gesture.update(true,200,180,300);ev=gesture.update(true,200,145,325);assert(ev.drag&&ev.dy==35);
  assert(!gesture.update(false,0,0,410).tap); // swipe cannot select an object
  gesture.update(true,200,40,500);assert(!gesture.update(false,0,0,600).tap); // single-sample noise
  for(int heading=0;heading<360;heading+=90) {
    double a=heading*CompassMath::RAD;
    auto forward=CompassMath::project(sin(a),cos(a),0,heading,0,0);
    assert(forward.x==80&&forward.y<147);
    auto right=CompassMath::project(cos(a),-sin(a),0,heading,0,0);assert(right.x>80);
  }
  assert(CompassMath::ARROW_TIP_Y<CompassMath::ARROW_TAIL_Y);
  assert(Astro::parseRA("23:59:59"));assert(Astro::parseRA("00:01.5"));
  for(auto s:{"24:00:00","12:60:00","12:00:60","12:00x","-1:00",""})assert(!Astro::parseRA(s));
  for(auto s:{"+90*00","-90*00:00","-05*23:28","+12*34'56"})assert(Astro::parseDec(s));
  for(auto s:{"+90*00:01","+91*00","05*23","+05*60","+12*00garbage",""})assert(!Astro::parseDec(s));
  double ra,dec; // J2000 noon UTC: Greenwich sidereal angle 280.46061837.
  Astro::equatorial(0,90,0,0,946728000,ra,dec);near(ra,280.46061837/15);near(dec,0);
  Astro::equatorial(90,0,0,0,946728000,ra,dec);near(ra,10.46061837/15);near(dec,0);
  Astro::equatorial(0,0,30,0,946728000,ra,dec);near(dec,60);
  Astro::equatorial(180,0,-30,0,946728000,ra,dec);near(dec,-60);
  Astro::equatorial(0,90,30,45,946728000,ra,dec);near(ra,325.46061837/15);near(dec,30);
  Fake::reset();Lx200 m;load(m);m.start();assert(Fake::tx.back()==":Sr05:35:17#");
  size_t count=Fake::tx.size();m.start();assert(Fake::tx.size()==count);
  Fake::feed("1");m.tick();assert(Fake::tx.back()==":Sd-05*23:28#");
  Fake::feed("1");m.tick();assert(Fake::tx.back()==":MS#");
  Fake::feed("0");m.tick();assert(!m.busy&&!m.ready&&m.message.s=="Slew accepted");
  Fake::reset();load(m);m.start();Fake::feed("0");m.tick();assert(!m.busy&&!m.ready);assert(Fake::tx.back()!=":MS#");
  Fake::reset();load(m);m.start();Fake::feed("1");m.tick();Fake::feed("1");m.tick();Fake::feed("1Below horizon#");m.tick();assert(m.message.s=="Slew rejected: Below horizon");
  Fake::reset();m.host="192.168.4.2";m.select(31);fakeTime+=151;m.tick();fakeTime+=1801;m.tick();assert(!m.busy&&!m.ready&&!Fake::connected);
  Fake::reset();load(m);m.cancel();count=Fake::tx.size();m.start();assert(Fake::tx.size()==count);
  Fake::reset();m.host="192.168.4.2";m.select(42);fakeTime+=151;m.tick();Fake::feed("Info#");m.tick();Fake::feed("99:00:00#");m.tick();assert(!m.busy&&!m.ready);
  Fake::reset();load(m);Fake::connected=false;Fake::canConnect=false;m.start();assert(!m.ready&&!m.busy);
  Fake::reset();m.host="192.168.4.2";m.stop();assert(Fake::tx.back()==":Q#");
  std::cout<<"PASS: touch mapping (8 orientations), tap/swipe/noise, cardinal headings, coordinate reference cases, RA/DEC validation, LX200 selection/start/rejection/timeout/cancel/disconnect/stop\n";
}
