#include <Arduino.h>
#include <TFT_eSPI.h>
#include <Wire.h>
#include <SPI.h>
#include <XPT2046_Touchscreen.h>
#include <Adafruit_BNO055.h>
#include <WebServer.h>
#include <Preferences.h>
#include <esp_wifi.h>
#include <sys/time.h>
#include "Config.h"
#include "UiAssets.h"
#include "charging_battery_32_32_28f.h"
#include "wifi_search_32_32_28f.h"
#include "compass_32_32_28f.h"
// TFT_eSPI already includes FreeSerifBold18pt7b with the identical glyph data.
#include "Org_01.h"
#include "TouchMath.h"
#include "CompassMath.h"
#include "BandCanvas.h"
#include "Astro.h"
#include "Catalog.h"
#include "Lx200.h"
#include "PhonePage.h"

TFT_eSPI tft;
BandCanvas canvas(&tft);
int bandHeight=40;
uint32_t frameNow=0;
SPIClass touchSPI(VSPI);
// Poll pressure instead of gating all input on IRQ; some CYD revisions differ.
XPT2046_Touchscreen touchscreen(TOUCH_CS);
TouchMath::Calibration touchCalibration;
TouchMath::Gesture touchGesture;
bool touchCalibrated=false;
Adafruit_BNO055 bno(55,IMU_ADDRESS,&Wire);
WebServer server(80);
Preferences prefs;
Lx200 mount;
enum Page { BOOT,HOME,CATALOG,CONFIRM,SETTINGS,NIGHT,LOCATION,CALIBRATE };
Page page=BOOT;
uint32_t bootAt=0,lastFrame=0,lastSensor=0,lastBattery=0,lastWifi=0;
bool imuOK=false,timeOK=false,locationOK=false,aligned=false,night=false,charging=false;
double latitude=0,longitude=0,azOffset=0,altOffset=0,azimuth=0,altitude=0,rollDeg=0,pitchDeg=0,raHours=0,decDeg=0;
int timezoneMinutes=0,batteryPercent=-1,wifiBars=0,clients=0,category=0,scrollY=0,selected=-1;
uint8_t imuSys=0,imuGyro=0,imuAccel=0,imuMag=0;
constexpr uint16_t CYAN=0x05FA,GREEN=0x04B1,ORANGE=0xFA60;
uint16_t ink() { return CYAN; } // Original Lopaka palette is never recolored.

void text(const String &s,int x,int y,uint16_t color=TFT_WHITE,int font=2) {
  canvas.setFreeFont(nullptr);canvas.setTextSize(1);
  canvas.setTextDatum(TL_DATUM); canvas.setTextColor(color,TFT_BLACK); canvas.drawString(s,x,y,font);
}
void button(const char *s,int x,int y,int w,int h,uint16_t color=CYAN) {
  canvas.setFreeFont(nullptr);canvas.setTextSize(1);
  canvas.drawRoundRect(x,y,w,h,4,color); canvas.setTextDatum(MC_DATUM);
  canvas.setTextColor(color,TFT_BLACK); canvas.drawString(s,x+w/2,y+h/2,2); canvas.setTextDatum(TL_DATUM);
}
void changePage(Page next) { page=next;touchGesture.reset(); }

const TouchMath::Point calibrationTargets[]={{20,20},{299,20},{160,219},{160,120}};
TouchMath::Point calibrationRaw[3];
int calibrationStep=0,calibrationCount=0;
double calibrationSumX=0,calibrationSumY=0;
int calibrationAnchorX=0,calibrationAnchorY=0;
uint32_t calibrationLast=0;
String calibrationMessage="Hold cross, then release";
void startCalibration() {
  calibrationStep=0;calibrationCount=0;calibrationSumX=calibrationSumY=0;
  calibrationMessage="Hold cross, then release";changePage(CALIBRATE);
}
void calibrateTouch(bool touching,TS_Point p,uint32_t now) {
  if(touching) {
    calibrationLast=now;
    if(calibrationCount==0||abs(p.x-calibrationAnchorX)>90||abs(p.y-calibrationAnchorY)>90) {
      calibrationCount=0;calibrationSumX=calibrationSumY=0;calibrationAnchorX=p.x;calibrationAnchorY=p.y;
    }
    if(calibrationCount<30){calibrationSumX+=p.x;calibrationSumY+=p.y;calibrationCount++;}
    return;
  }
  if(!calibrationCount||now-calibrationLast<90)return;
  int count=calibrationCount;calibrationCount=0;
  TouchMath::Point raw={(float)(calibrationSumX/count),(float)(calibrationSumY/count)};
  calibrationSumX=calibrationSumY=0;
  if(count<8){calibrationMessage="Hold a little longer";return;}
  if(calibrationStep<3) {
    calibrationRaw[calibrationStep++]=raw;
    calibrationMessage="Hold cross, then release";
    if(calibrationStep==3 && !touchCalibration.fit(calibrationRaw,calibrationTargets)) {
      startCalibration();calibrationMessage="Try again: touch each cross";
    }
  } else {
    TouchMath::Point check=touchCalibration.map(raw);
    if(fabs(check.x-160)>12||fabs(check.y-120)>12) {startCalibration();calibrationMessage="Check failed; try again";return;}
    prefs.putBytes("touch-v1",&touchCalibration,sizeof(touchCalibration));touchCalibrated=true;
    Serial.println("Touch calibrated and saved.");changePage(HOME);
  }
}

int filtered(int ordinal) {
  for(int i=0;i<TARGET_COUNT;i++) if(category==0||targets[i].category==category) if(ordinal--==0) return i;
  return -1;
}
int rowCount() { int n=0;while(filtered(n)>=0)n++;return n; }
void selectTarget(int i) { selected=i; mount.select(targets[i].messier); changePage(CONFIRM); }

void tap(int x,int y) {
  if(page==BOOT) return;
  if(page==HOME && y<60) {
    if(x<70) changePage(HOME);
    else if(x<140) changePage(NIGHT);
    else if(x<210) changePage(CATALOG);
    else if(x<278) changePage(SETTINGS);
    else changePage(LOCATION);
    return;
  }
  if(page!=HOME && x<50 && y<60) {
    if(page==CONFIRM) { mount.cancel();changePage(CATALOG); }
    else changePage(HOME);
    return;
  }
  if(page==CATALOG) {
    if(x<50 && y>=80) { category=constrain((y-80)/32,0,4);scrollY=0; }
    else if(x>=51&&y>=60) { int i=filtered((y-60+scrollY)/36); if(i>=0)selectTarget(i); }
  } else if(page==CONFIRM) {
    if(y>=194&&y<233) {
      if(x>=55&&x<177) mount.start();
      else if(x>=185) mount.stop();
    }
  } else if(page==NIGHT) {
    if(x>=60&&y>=90&&y<132) {night=!night;ledcWrite(0,night?45:220);}
    if(x>=60&&y>=150&&y<193) {
      static bool dim=false;dim=!dim;ledcWrite(0,dim?45:220);
    }
  } else if(page==HOME&&x<69&&y>=212) { mount.stop(); }
  else if(page==SETTINGS&&x>=58&&x<310&&y>=207)startCalibration();
}

void pollTouch() {
  static uint32_t sampled=0,debugAt=0;
  if(millis()-sampled<16)return;sampled=millis();
  TS_Point p=touchscreen.getPoint();bool touching=p.z>=300;
  if(page==CALIBRATE){calibrateTouch(touching,p,sampled);return;}
  if(page==BOOT||!touchCalibrated){touchGesture.reset();return;}
  TouchMath::Point pos=touchCalibration.map({(float)p.x,(float)p.y});
  int x=constrain((int)lround(pos.x),0,319),y=constrain((int)lround(pos.y),0,239);
  if(touching&&sampled-debugAt>250) {
    debugAt=sampled;Serial.printf("Touch raw=%d,%d z=%d screen=%d,%d\n",p.x,p.y,p.z,x,y);
  }
  TouchMath::Event event=touchGesture.update(touching,x,y,sampled);
  if(event.drag&&page==CATALOG&&event.x>=51&&event.y>=60)
    scrollY=constrain(scrollY+event.dy,0,max(0,rowCount()*36-180));
  if(event.tap)tap(event.x,event.y);
}

bool numberArg(const char *key,double lo,double hi,double &v) {
  if(!server.hasArg(key))return false;
  String value=server.arg(key);char *end=nullptr;
  v=strtod(value.c_str(),&end);
  return value.length()>0&&end&&*end==0&&isfinite(v)&&v>=lo&&v<=hi;
}
void setupPhone() {
  prefs.begin("star-ui",false);
  if(prefs.getBytesLength("touch-v1")==sizeof(touchCalibration)) {
    prefs.getBytes("touch-v1",&touchCalibration,sizeof(touchCalibration));touchCalibrated=touchCalibration.valid();
  }
  latitude=prefs.getDouble("lat",0);longitude=prefs.getDouble("lon",0);
  azOffset=prefs.getDouble("az",0);altOffset=prefs.getDouble("alt",0);
  locationOK=prefs.getBool("loc",false);aligned=prefs.getBool("aligned",false);
  mount.host=prefs.getString("host","");mount.port=prefs.getUShort("port",4030);
  WiFi.mode(WIFI_AP);WiFi.softAP(AP_NAME,AP_PASSWORD);
  server.on("/",HTTP_GET,[]{server.send_P(200,"text/html; charset=utf-8",PHONE_PAGE);});
  server.on("/touch/calibrate",HTTP_POST,[]{
    if(mount.busy){server.send(409,"text/plain","Wait for LX200 operation");return;}
    startCalibration();server.send(200,"text/plain; charset=utf-8","แตะกากบาทบนจออุปกรณ์ตามลำดับ แล้วปล่อยนิ้ว");
  });
  server.on("/time",HTTP_POST,[]{
    double epoch,offset;
    if(!numberArg("epoch",1577836800,4102444799,epoch)||!numberArg("offset",-840,840,offset)) {
      server.send(400,"text/plain","Invalid time");return;
    }
    timeval tv={(time_t)epoch,0};settimeofday(&tv,nullptr);timezoneMinutes=(int)offset;timeOK=true;
    server.send(200,"text/plain; charset=utf-8","ตั้งเวลาจากมือถือสำเร็จ");
  });
  server.on("/settings",HTTP_GET,[]{
    String result="{\"lat\":"+String(latitude,6)+",\"lon\":"+String(longitude,6)+",\"az\":"+String(azOffset,3)+",\"alt\":"+String(altOffset,3)+",\"aligned\":"+(aligned?"true":"false")+",\"host\":\""+mount.host+"\",\"port\":"+String(mount.port)+"}";
    server.send(200,"application/json",result);
  });
  server.on("/settings",HTTP_POST,[]{
    double lat,lon,az,alt,port;IPAddress ip;
    String host=server.arg("host");host.trim();
    if(!numberArg("lat",-90,90,lat)||!numberArg("lon",-180,180,lon)||!numberArg("az",-360,360,az)||!numberArg("alt",-90,90,alt)||!numberArg("port",1,65535,port)||floor(port)!=port||(host.length()&&!ip.fromString(host))) {
      server.send(400,"text/plain","Invalid coordinates, offsets or endpoint");return;
    }
    if(mount.busy) {server.send(409,"text/plain","Wait for LX200 operation");return;}
    mount.cancel();latitude=lat;longitude=lon;azOffset=az;altOffset=alt;mount.host=host;mount.port=(uint16_t)port;
    aligned=server.hasArg("aligned");locationOK=true;
    prefs.putDouble("lat",lat);prefs.putDouble("lon",lon);prefs.putDouble("az",az);prefs.putDouble("alt",alt);
    prefs.putBool("loc",true);prefs.putBool("aligned",aligned);prefs.putString("host",host);prefs.putUShort("port",mount.port);
    server.send(200,"text/plain; charset=utf-8","บันทึกแล้ว");
  });
  server.begin();
}

void sampleSensors() {
  uint32_t now=millis();
  if(now-lastSensor>=40) {
    lastSensor=now;
    if(imuOK) {
      sensors_event_t event;
      bno.getEvent(&event);
      azimuth=Astro::wrap(event.orientation.x+azOffset);
      rollDeg=event.orientation.y;pitchDeg=event.orientation.z;
      altitude=constrain(-pitchDeg+altOffset,-90.,90.);
      bno.getCalibration(&imuSys,&imuGyro,&imuAccel,&imuMag);
      if(timeOK&&locationOK&&aligned) Astro::equatorial(azimuth,altitude,latitude,longitude,time(nullptr),raHours,decDeg);
    }
  }
  if(now-lastBattery>=1000) {
    lastBattery=now;
    charging=CHARGE_PIN>=0&&digitalRead(CHARGE_PIN)==CHARGE_ACTIVE;
    if(BATTERY_ADC>=0) {
      double volts=analogReadMilliVolts(BATTERY_ADC)*BATTERY_DIVIDER/1000.0;
      batteryPercent=constrain((int)lround((volts-BATTERY_EMPTY)/(BATTERY_FULL-BATTERY_EMPTY)*100),0,100);
    }
  }
  if(now-lastWifi>=600) {
    lastWifi=now;wifi_sta_list_t list={};clients=WiFi.softAPgetStationNum();wifiBars=0;
    if(esp_wifi_ap_get_sta_list(&list)==ESP_OK && list.num>0) {
      int rssi=list.sta[0].rssi;
      for(int i=1;i<list.num;i++)rssi=min(rssi,(int)list.sta[i].rssi);
      wifiBars=rssi>=-55?4:rssi>=-67?3:rssi>=-78?2:1;
    }
  }
}

void drawBoot(uint32_t elapsed) {
  const int xs[]={87,117,144,166,232},ys[]={94,106,85,112,75};
  const uint8_t *bits[]={image_star_bits,image_star_1_bits,image_star_2_bits,image_star_2_bits,image_star_2_bits};
  for(int i=0;i<5;i++) {
    float p=constrain(((int)elapsed-i*230)/400.0f,0.f,1.f);
    if(p<=0)continue;
    uint8_t c=(uint8_t)(255*p);
    canvas.drawBitmap(xs[i],ys[i]+(int)(18*(1-p)),bits[i],i==1?32:30,i==1?30:32,tft.color565(c,c,c));
  }
  if(elapsed>=1400) {
    canvas.drawRect(71,155,188,10,TFT_WHITE);
    canvas.fillRect(73,157,min(184,(int)((elapsed-1400)*184/1400)),6,0xFFFF);
  }
}

void drawCompass() {
  // Preserve original 105x83 white compass region, not the separate logo.
  canvas.setViewport(28,106,105,83,false);
  for(int ring=0;ring<3;ring++) {
    int ox=0,oy=0;
    for(int j=0;j<=48;j++) {
      double a=j*2*PI/48;
      auto p=CompassMath::project(ring==1?0:cos(a),ring==2?0:sin(a),ring==0?0:ring==1?cos(a):sin(a),azimuth,pitchDeg,rollDeg);
      if(j)canvas.drawLine(ox,oy,p.x,p.y,0xFFFF);
      ox=p.x;oy=p.y;
    }
  }
  const char *labels[]={"N","E","S","W"};
  const double east[]={0,1,0,-1},north[]={1,0,-1,0};
  for(int i=0;i<4;i++) {
    auto p=CompassMath::project(east[i],north[i],0,azimuth,pitchDeg,rollDeg);
    int x=constrain(p.x-3,28,126),y=constrain(p.y-4,106,180);
    canvas.fillRect(x-1,y-1,8,10,TFT_BLACK);text(labels[i],x,y,0xFFFF,1);
  }
  // Fixed forward arrow points to the top of the device, independent of IMU.
  constexpr int x=CompassMath::ARROW_X;
  canvas.fillRect(x-5,CompassMath::ARROW_TIP_Y-2,11,28,TFT_BLACK);
  canvas.drawLine(x,CompassMath::ARROW_TAIL_Y,x,CompassMath::ARROW_TIP_Y,0xFE00);
  canvas.drawLine(x-5,CompassMath::ARROW_TIP_Y+7,x,CompassMath::ARROW_TIP_Y,0xFE00);
  canvas.drawLine(x+5,CompassMath::ARROW_TIP_Y+7,x,CompassMath::ARROW_TIP_Y,0xFE00);
  canvas.resetViewport();
}
void drawBattery() {
  bool low=batteryPercent>=0&&batteryPercent<15&&!charging;
  if(low&&(frameNow/450)%2)return;
  uint16_t color=low?ORANGE:GREEN;
  if(charging) {
    canvas.setBitmapColor(GREEN,TFT_BLACK);
    canvas.drawBitmap(284,211,charging_battery_32_32_28f_frames[(frameNow/42)%28],32,32,0x04B1);
  } else {
    canvas.drawRect(285,217,27,14,color);canvas.fillRect(312,221,3,6,color);
    if(batteryPercent>=0)canvas.fillRect(287,219,batteryPercent*23/100,10,color);
    text(batteryPercent<0?"--%":String(batteryPercent)+"%",284,202,color,1);
  }
}
void drawWifi() {
  if(!clients) {
    canvas.setBitmapColor(GREEN,TFT_BLACK);
    canvas.drawBitmap(246,209,wifi_search_32_32_28f_frames[(frameNow/42)%28],32,32,0x04B1);
  } else {
    for(int i=0;i<4;i++)canvas.fillRect(248+i*7,233-(i+1)*5,5,(i+1)*5,i<wifiBars?GREEN:TFT_DARKGREY);
  }
}
void drawHome() {
  // Original Lopaka image data is already byte-swapped for TFT_eSPI.
  canvas.setSwapBytes(false);canvas.pushImage(0,0,320,60,image_paint_1_pixels);
  for(int x: {140,210,278})canvas.drawLine(x,58,x,1,0xFB20);
  canvas.pushImage(70,1,1,58,image_paint_7_pixels);
  canvas.drawBitmap(11,5,image_menu_home_bits,45,48,0x05FA);
  canvas.drawBitmap(82,6,image_moon_bits,48,48,0x05FA);
  canvas.drawBitmap(156,8,image_Pin_star_bits,42,42,0x05FA);
  canvas.drawBitmap(219,6,image_menu_settings_gear_bits,48,48,0x05FA);
  canvas.setTextDatum(TL_DATUM);canvas.setTextColor(0x04B1,TFT_BLACK);
  canvas.setTextSize(1);canvas.setFreeFont(&FreeSerifBold18pt7b);
  canvas.drawString(locationOK&&latitude<0?"S":"N",288,18);
  canvas.setTextColor(0x05FA,TFT_BLACK);canvas.setTextSize(4);canvas.setFreeFont(&Org_01);
  canvas.drawString("RA:",170,131);canvas.drawString("DEC:",146,167);
  canvas.setTextSize(2);canvas.setTextColor(0xF206,TFT_BLACK);
  if(timeOK) {time_t local=time(nullptr)+timezoneMinutes*60;tm t;gmtime_r(&local,&t);char buf[20];strftime(buf,sizeof(buf),"%I:%M %p",&t);canvas.drawString(buf,231,67);}
  else canvas.drawString("--:-- --",231,67);
  drawCompass();
  // RA/DEC are derived only from BNO055 attitude + time/site, never LX200.
  bool valid=imuOK&&timeOK&&locationOK&&aligned;
  canvas.setFreeFont(&Org_01);canvas.setTextSize(2);canvas.setTextColor(0x05FA,TFT_BLACK);
  if(valid) {
    int s=((int)lround(raHours*3600))%86400;char ra[20],dec[20];
    snprintf(ra,sizeof(ra),"%02d:%02d:%02d",s/3600,s/60%60,s%60);snprintf(dec,sizeof(dec),"%+.2f",decDeg);
    canvas.drawString(ra,232,136);canvas.drawString(dec,232,172);
  } else {canvas.drawString("--:--:--",232,136);canvas.drawString("--.--",232,172);}
  // Exact original 32x32 logo, location, 28 frames and 42ms frame interval.
  canvas.drawBitmap(2,61,compass_32_32_28f_frames[(frameNow/42)%28],32,32,0x04B1);
  button("STOP",5,212,64,25,ORANGE);
  if(mount.message!="Select a target")text(mount.message.substring(0,27),75,221,ORANGE,1);
  drawWifi();drawBattery();
}
void backHeader(const char *title) {
  canvas.drawBitmap(8,28,image_arrow_curved_up_left_bits,32,20,TFT_WHITE);
  canvas.drawRect(50,1,270,238,0xE8EC);canvas.drawFastHLine(51,58,269,TFT_WHITE);text(title,61,20,ink());
}
void drawCatalog() {
  backHeader("OBJECT CATALOG");
  const char *labels[]={"ALL","CONST","CLUST","GALAX","NEB"};
  for(int i=0;i<5;i++) {
    canvas.fillRect(1,80+i*32,48,30,category==i?0x3204:0x0841);
    text(labels[i],5,90+i*32,category==i?TFT_WHITE:ink(),1);
  }
  canvas.setViewport(51,60,265,179,false);
  int rows=rowCount();
  for(int n=scrollY/36;n<rows;n++) {
    int y=60+n*36-scrollY;if(y>=239)break;
    int i=filtered(n);text(targets[i].name,58,y+1,ink());text(targets[i].detail,58,y+21,TFT_LIGHTGREY,1);
    canvas.drawFastHLine(54,y+35,259,TFT_DARKGREY);
  }
  canvas.resetViewport();
  if(rows*36>180) {int h=max(10,180*180/(rows*36));int y=60+scrollY*(180-h)/(rows*36-180);canvas.fillRect(317,y,2,h,ink());}
}
void drawConfirm() {
  backHeader("TARGET / LX200");
  if(selected>=0) {text(targets[selected].name,59,66,ink());text(targets[selected].detail,59,86,TFT_LIGHTGREY,1);}
  text(mount.info.substring(0,35),59,104,TFT_LIGHTGREY,1);
  text("RA  "+mount.ra,59,122,ink());text("DEC "+mount.dec,59,143,ink());
  text(mount.message.substring(0,36),59,173,ORANGE,1);
  button(mount.ready?"START?":mount.busy?"WAIT":"NOT READY",55,194,121,38,mount.ready?GREEN:TFT_DARKGREY);
  button("STOP",185,194,123,38,ORANGE);
}
void drawSettings(bool site) {
  backHeader(site?"SITE / HEMISPHERE":"SETTINGS");
  text("Wi-Fi: StarTracker",60,67,ink());text("Key: startrack24",60,89,ink());text("Open 192.168.4.1",60,111,ORANGE);
  if(site) {
    text("Lat: "+String(latitude,4),60,142);text("Lon: "+String(longitude,4),60,164);
    text(locationOK?(latitude<0?"South celestial pole":"North celestial pole"):"Set site on phone",60,194,ink());
  } else {
    text("IMU: "+String(imuOK?"OK":"not found"),60,141);
    text("Cal SYS/G/A/M: "+String(imuSys)+"/"+String(imuGyro)+"/"+String(imuAccel)+"/"+String(imuMag),60,163,TFT_LIGHTGREY,1);
    text("LX: "+(LX_SERIAL?String("Serial"):mount.host.length()?mount.host:String("configure on phone")),60,185,TFT_LIGHTGREY,1);
    button("CALIBRATE TOUCH",58,207,251,27,ink());
  }
}
void drawCalibration() {
  text("TOUCH CALIBRATION",64,65,CYAN);
  text("Point "+String(calibrationStep+1)+" / 4",106,87,TFT_WHITE);
  text(calibrationMessage,64,155,TFT_WHITE,1);
  text("Hold ~0.5s, release, next cross",64,171,TFT_WHITE,1);
  int x=calibrationTargets[calibrationStep].x,y=calibrationTargets[calibrationStep].y;
  canvas.drawCircle(x,y,9,0xFE00);canvas.drawFastHLine(x-13,y,27,0xFFFF);canvas.drawFastVLine(x,y-13,27,0xFFFF);
  if(calibrationCount>=8)canvas.drawCircle(x,y,11,0x04B1);
}
void render() {
  frameNow=millis();
  if(page==BOOT && frameNow-bootAt>=3000) {
    if(touchCalibrated)changePage(HOME);else startCalibration();
  }
  for(int bandY=0;bandY<240;bandY+=bandHeight) {
  canvas.beginBand(bandY);canvas.setFreeFont(nullptr);canvas.setTextSize(1);canvas.setTextDatum(TL_DATUM);
  switch(page) {
    case BOOT:drawBoot(frameNow-bootAt);break;
    case HOME:drawHome();break;
    case CATALOG:drawCatalog();break;
    case CONFIRM:drawConfirm();break;
    case SETTINGS:drawSettings(false);break;
    case LOCATION:drawSettings(true);break;
    case NIGHT:backHeader("DISPLAY BRIGHTNESS");button(night?"DIM: ON":"DIM: OFF",60,90,245,42);button("TOGGLE BRIGHTNESS",60,150,245,42);break;
    case CALIBRATE:drawCalibration();break;
  }
  canvas.pushSprite(0,bandY);
  yield();
  }
}
void setup() {
  Serial.begin(115200);tft.init();tft.setRotation(1);tft.fillScreen(TFT_BLACK);
  ledcSetup(0,5000,8);ledcAttachPin(21,0);ledcWrite(0,220);
  canvas.setColorDepth(16);
  // 25,600 bytes instead of a contiguous 153,600-byte full-screen allocation.
  // All candidate heights divide 240; no final strip can extend off screen.
  for(int h: {40,20,10}) {
    if(canvas.createSprite(320,h)){bandHeight=h;break;}
    canvas.deleteSprite();
  }
  if(!canvas.created()) {tft.setTextColor(TFT_RED);tft.drawString("Display buffer allocation failed",5,100,2);while(true)delay(100);}
  Serial.printf("RGB565 strip: 320x%d, %d bytes (+ library overhead)\n",bandHeight,320*bandHeight*2);
  pinMode(TOUCH_CS,OUTPUT);digitalWrite(TOUCH_CS,HIGH);pinMode(TOUCH_IRQ,INPUT);
  touchSPI.begin(TOUCH_CLK,TOUCH_MISO,TOUCH_MOSI,TOUCH_CS);
  touchscreen.begin(touchSPI);touchscreen.setRotation(1);
  if(CHARGE_PIN>=0)pinMode(CHARGE_PIN,INPUT_PULLUP);
  if(BATTERY_ADC>=0) {pinMode(BATTERY_ADC,INPUT);analogSetPinAttenuation(BATTERY_ADC,ADC_11db);}
  Wire.begin(IMU_SDA,IMU_SCL);Wire.setTimeOut(25);
  imuOK=bno.begin();if(imuOK)bno.setExtCrystalUse(true);
  setupPhone();mount.begin();bootAt=millis();
  Serial.println("CYD ILI9341 + XPT2046 ready. Send C to recalibrate touch.");
}
void loop() {
  while(Serial.available()){char c=Serial.read();if((c=='c'||c=='C')&&!mount.busy)startCalibration();}
  server.handleClient();mount.tick();sampleSensors();pollTouch();
  if(millis()-lastFrame>=42) {lastFrame=millis();render();}
  delay(1);
}

