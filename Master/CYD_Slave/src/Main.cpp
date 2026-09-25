/*
 * DIY_startrackerGOTO
 * Module: CYD_Slave (Touchscreen UI)
 * Version: 2.1 (Added Abort Popups & Safety)
 */
#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <XPT2046_Touchscreen.h>
#include <WebServer.h>
#include <Preferences.h>
#include <esp_wifi.h>
#include <sys/time.h>
#include "core/Config.h"
#include "assets/UiAssets.h"
#include "assets/charging_battery_32_32_28f.h"
#include "assets/wifi_search_32_32_28f.h"
#include "assets/compass_32_32_28f.h"
// TFT_eSPI already includes FreeSerifBold18pt7b with the identical glyph data.
#include "assets/Org_01.h"
#include "core/TouchMath.h"
#include "core/CompassMath.h"
#include "ui/BandCanvas.h"
#include "core/Astro.h"
#include "core/Catalog.h"
#include "core/Lx200.h"
#include "ui/PhonePage.h"

TFT_eSPI tft;
BandCanvas canvas(&tft);
int bandHeight=40;
int bandY=0;
uint32_t frameNow=0;
SPIClass touchSPI(VSPI);
// Poll pressure instead of gating all input on IRQ; some CYD revisions differ.
XPT2046_Touchscreen touchscreen(TOUCH_CS);
TouchMath::Calibration touchCalibration;
TouchMath::Gesture touchGesture;
bool touchCalibrated=false;

WebServer server(80);
Preferences prefs;
Lx200 mount;
enum Page { BOOT,HOME,CATALOG,CONFIRM,SETTINGS,NIGHT,LOCATION,CALIBRATE,QUICKMODE,SET_TIMER,TRACKING,HOMING };
Page page=BOOT;
enum TrackSource { SRC_CATALOG, SRC_APP };
TrackSource currentTrackSource = SRC_CATALOG;
uint32_t bootAt=0,lastFrame=0,lastSensor=0,lastBattery=0,lastWifi=0;
bool imuOK=false,timeOK=false,locationOK=false,aligned=false,night=false,charging=false;
double latitude=0,longitude=0,azOffset=0,altOffset=0,azimuth=0,altitude=0,rollDeg=0,pitchDeg=0,raHours=0,decDeg=0;
int timezoneMinutes=0,batteryPercent=-1,wifiBars=0,clients=0,category=0,scrollY=0,selected=-1;
uint8_t imuSys=0,imuGyro=0,imuAccel=0,imuMag=0;
int cursorIndex=0; // 0=Home, 1=QuickMode, 2=Catalog, 3=Settings
bool showCursor=false;
TrackingMode trackMode = TRACK_SIDEREAL;

int trackHours = 1, trackMins = 30;
float timerScrollH = 0, timerScrollM = 0;
int trackDuration = 0;
time_t trackEndTime = 0, trackPauseTime = 0;
enum TrackState { TS_RUNNING, TS_PAUSED, TS_COMPLETE };
TrackState trackState = TS_RUNNING;
bool showCompletePopup = false;
bool showHomingErrorPopup = false;

constexpr uint16_t CYAN=0x05FA,GREEN=0x04B1,ORANGE=0xFA60;
uint16_t ink() { return night ? TFT_RED : CYAN; }
uint16_t white() { return night ? TFT_RED : TFT_WHITE; }
uint16_t green() { return night ? TFT_RED : GREEN; }
uint16_t orange() { return night ? TFT_RED : ORANGE; }
uint16_t grey() { return night ? TFT_RED : TFT_LIGHTGREY; }
uint16_t dark() { return night ? 0x8000 : TFT_DARKGREY; }
bool dimScreen = false;

void text(const String &s,int x,int y,uint16_t color=TFT_WHITE,int font=2) {
  if(night) color = TFT_RED;
  canvas.setFreeFont(nullptr);canvas.setTextSize(1);
  canvas.setTextDatum(TL_DATUM); canvas.setTextColor(color,TFT_BLACK); canvas.drawString(s,x,y,font);
}
void button(const char *s,int x,int y,int w,int h,uint16_t color=CYAN) {
  if(night) color = TFT_RED;
  canvas.setFreeFont(nullptr);canvas.setTextSize(1);
  canvas.drawRoundRect(x,y,w,h,4,color); canvas.setTextDatum(MC_DATUM);
  canvas.setTextColor(color,TFT_BLACK); canvas.drawString(s,x+w/2,y+h/2,2); canvas.setTextDatum(TL_DATUM);
}
void filledButton(const char *s,int x,int y,int w,int h,uint16_t bgColor, uint16_t txtColor) {
  if(night) { bgColor = TFT_DARKGREY; txtColor = TFT_RED; }
  canvas.setFreeFont(nullptr);canvas.setTextSize(1);
  canvas.fillRoundRect(x,y,w,h,4,bgColor);
  canvas.setTextDatum(MC_DATUM);
  canvas.setTextColor(txtColor,bgColor); 
  canvas.drawString(s,x+w/2,y+h/2,2); 
  canvas.setTextDatum(TL_DATUM);
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
void selectTarget(int i) { selected = i; changePage(CONFIRM); }

void tap(int x,int y) {
  if(page==BOOT) return;
  if(page==HOME && y<60) {
    if(x<70) { Serial2.println("C:HOME"); changePage(HOMING); }
    else if(x<140) { changePage(QUICKMODE); }
    else if(x<210) { changePage(CATALOG); }
    else if(x<278) { changePage(SETTINGS); }
    return;
  }
  if(page!=HOME && page!=TRACKING && x<50 && y<60) {
    if(page==CONFIRM) { changePage(CATALOG); }
    else changePage(HOME);
    return;
  }
  if(page==CATALOG) {
    if(x<50 && y>=80) { category=constrain((y-80)/32,0,4);scrollY=0; }
    else if(x>=51&&y>=60) { int i=filtered((y-60+scrollY)/36); if(i>=0)selectTarget(i); }
  }
  if(page==CONFIRM) {
    if(y>=194&&y<233) {
      if(x<110) {
        // APP button 
        currentTrackSource = SRC_APP;
        changePage(SET_TIMER);
      }
      else {
        // NASA API or OFFLINE
        currentTrackSource = SRC_CATALOG;
        changePage(SET_TIMER);
      }
    }
  } else if(page==SET_TIMER) {
    if(y>=50 && y<=90) { // Tapped top
      if(x<160) { trackHours = (trackHours+23)%24; timerScrollH = 45; }
      else { trackMins = (trackMins+59)%60; timerScrollM = 45; }
    } else if(y>=130 && y<=180) { // Tapped bottom
      if(x<160) { trackHours = (trackHours+1)%24; timerScrollH = -45; }
      else { trackMins = (trackMins+1)%60; timerScrollM = -45; }
    } else if(y>=200 && y<=240) {
      if(x>=40 && x<=280) {
        trackDuration = trackHours * 3600 + trackMins * 60;
        trackEndTime = time(nullptr) + trackDuration;
        trackState = TS_RUNNING;
        
        if(currentTrackSource == SRC_CATALOG && selected >= 0) {
          String cmd = "T:" + String(targets[selected].ra_h, 5) + "," + String(targets[selected].dec_d, 5);
          Serial2.println(cmd);
        } else if(currentTrackSource == SRC_APP) {
          // APP handled via Wi-Fi from Stellarium
        }
        changePage(TRACKING);
      }
    }
  } else if(page==TRACKING) {
    if(trackState == TS_COMPLETE) return; // ignore touch during complete popup
    if(y>=190 && x>=100 && x<=220) { // CANCEL
       trackState = TS_COMPLETE;
       mount.stop();
       Serial2.println("M:STOP");
       changePage(HOME);
    }
  } else if(page==QUICKMODE) {
    if(y>=70 && y<110) { trackMode = TRACK_SIDEREAL; Serial2.println("M:SIDEREAL"); changePage(HOME); }
    else if(y>=120 && y<160) { trackMode = TRACK_LUNAR; Serial2.println("M:LUNAR"); changePage(HOME); }
    else if(y>=170 && y<210) { trackMode = TRACK_SOLAR; Serial2.println("M:SOLAR"); changePage(HOME); }
  } else if(page==NIGHT) {
    if(x>=60&&y>=90&&y<132) {night=!night;ledcWrite(0,night?45:220);}
    if(x>=60&&y>=150&&y<193) {
      static bool dim=false;dim=!dim;ledcWrite(0,dim?45:220);
    }
  } else if(page==HOME&&x<69&&y>=212) { mount.stop(); 
  } else if(page==SETTINGS) {
    if(x>=60 && x<220 && y>=135 && y<170) {
      dimScreen = !dimScreen;
      ledcWrite(0, dimScreen ? 45 : 220);
    } else if(x>=60 && x<220 && y>=180 && y<215) {
      night = !night;
      // When night changes, force screen update
    } else if(x>=225 && x<315 && y>=135 && y<215) {
      Serial2.println("C:CALIMU");
    }
  }
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
  if(page==SET_TIMER) {
    if(event.drag) {
      if(event.x < 160) timerScrollH += event.dy;
      else timerScrollM += event.dy;
    }
    
    // Snap and wrap logic while dragging
    if(timerScrollH > 45) { trackHours = (trackHours+1)%24; timerScrollH -= 45; }
    else if(timerScrollH < -45) { trackHours = (trackHours+23)%24; timerScrollH += 45; }
    if(timerScrollM > 45) { trackMins = (trackMins+1)%60; timerScrollM -= 45; }
    else if(timerScrollM < -45) { trackMins = (trackMins+59)%60; timerScrollM += 45; }
    
    // Smooth auto-centering when touch is released
    static bool wasTouchingTimer = false;
    bool released = wasTouchingTimer && !touching;
    wasTouchingTimer = touching;
    
    if(released) {
       if(timerScrollH > 22.5) { trackHours = (trackHours+1)%24; timerScrollH -= 45; }
       else if(timerScrollH < -22.5) { trackHours = (trackHours+23)%24; timerScrollH += 45; }
       
       if(timerScrollM > 22.5) { trackMins = (trackMins+1)%60; timerScrollM -= 45; }
       else if(timerScrollM < -22.5) { trackMins = (trackMins+59)%60; timerScrollM += 45; }
    }
    if(!touching) {
       timerScrollH *= 0.8;
       timerScrollM *= 0.8;
    }
  }
  if(showCompletePopup || showHomingErrorPopup) {
    if(event.tap) {
      if(event.x >= 120 && event.x <= 200 && event.y >= 130 && event.y <= 160) {
        showCompletePopup = false;
        showHomingErrorPopup = false;
      }
    }
  } else {
    if(event.tap) { showCursor=false; tap(event.x,event.y); }
  }
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
  if(latitude==0 && longitude==0) {
    latitude=DEFAULT_LAT; longitude=DEFAULT_LON; locationOK=true;
  }
  timezoneMinutes = DEFAULT_TIMEZONE_MINS;
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  configTime(0, 0, "pool.ntp.org", "time.nist.gov"); // Timezone offset is applied when displaying
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

void pollSerial2() {
  static String serialBuffer = "";
  
  while(Serial2.available()) {
    char c = Serial2.read();
    if (c == '\n') {
      serialBuffer.trim();
      String line = serialBuffer;
      serialBuffer = ""; // Reset for next line
      
      if(line.startsWith("O:")) { // O:azimuth,pitch,roll,sys,gyro,acc,mag
        int c1 = line.indexOf(',');
        int c2 = line.indexOf(',', c1+1);
        int c3 = line.indexOf(',', c2+1);
        int c4 = line.indexOf(',', c3+1);
        int c5 = line.indexOf(',', c4+1);
        int c6 = line.indexOf(',', c5+1);
        if(c1>0 && c2>0) {
          azimuth = Astro::wrap(line.substring(2, c1).toDouble() + azOffset);
          pitchDeg = line.substring(c1+1, c2).toDouble();
          
          if(c3>0) {
            rollDeg = line.substring(c2+1, c3).toDouble();
            imuSys = line.substring(c3+1, c4).toInt();
            imuGyro = line.substring(c4+1, c5).toInt();
            imuAccel = line.substring(c5+1, c6).toInt();
            imuMag = line.substring(c6+1).toInt();
          } else {
            rollDeg = line.substring(c2+1).toDouble();
          }
          
          altitude = constrain(-pitchDeg+altOffset,-90.,90.);
          imuOK = true;
          if(timeOK&&locationOK) Astro::equatorial(azimuth,altitude,latitude,longitude,time(nullptr),raHours,decDeg);
        }
      } else if(line.startsWith("B:")) { // Button press
        String btn = line.substring(2);
        showCursor = true;
        if(page==HOME) {
          if(btn=="LEFT") { cursorIndex = max(0, cursorIndex-1); }
          else if(btn=="RIGHT") { cursorIndex = min(3, cursorIndex+1); }
          else if(btn=="ENTER") { 
            if(cursorIndex==0) { Serial2.println("C:HOME"); changePage(HOMING); }
            else if(cursorIndex==1) { changePage(QUICKMODE); }
            else if(cursorIndex==2) { changePage(CATALOG); }
            else if(cursorIndex==3) { changePage(SETTINGS); }
          }
        } else if(page==SET_TIMER) {
          if(btn=="LEFT") { cursorIndex = 0; }
          else if(btn=="RIGHT") { cursorIndex = 1; }
          else if(btn=="UP") {
             if(cursorIndex==0) { trackHours = (trackHours+1)%24; timerScrollH = -45; }
             else { trackMins = (trackMins+1)%60; timerScrollM = -45; }
          }
          else if(btn=="DOWN") {
             if(cursorIndex==0) { trackHours = (trackHours+23)%24; timerScrollH = 45; }
             else { trackMins = (trackMins+59)%60; timerScrollM = 45; }
          }
          else if(btn=="ENTER") { tap(160, 220); }
        }
      } else if(line.startsWith("R:")) { // RA/DEC from main board if needed
          // parse ra, dec
      } else if(line.startsWith("H:DONE")) {
          if(page == HOMING) changePage(HOME);
      } else if(line.startsWith("H:ERR")) {
          if(page == HOMING) {
             changePage(HOME);
             showHomingErrorPopup = true;
          }
      }
    } else if (c != '\r') {
      serialBuffer += c;
    }
  }
}

void sampleSensors() {
  uint32_t now=millis();
  // IMU polling is removed. The CYD waits for Serial2 orientation updates.
  if(now-lastBattery>=1000) {
    lastBattery=now;
    charging=CHARGE_PIN>=0&&digitalRead(CHARGE_PIN)==CHARGE_ACTIVE;
    if(BATTERY_ADC>=0) {
      double volts=analogReadMilliVolts(BATTERY_ADC)*BATTERY_DIVIDER/1000.0;
      batteryPercent=constrain((int)lround((volts-BATTERY_EMPTY)/(BATTERY_FULL-BATTERY_EMPTY)*100),0,100);
    }
  }
  if(now-lastWifi>=600) {
    lastWifi=now;
    clients= (WiFi.status() == WL_CONNECTED) ? 1 : 0;
    wifiBars=0;
    if(clients) {
      int rssi = WiFi.RSSI();
      wifiBars = rssi >= -55 ? 4 : rssi >= -67 ? 3 : rssi >= -78 ? 2 : 1;
      if(!timeOK) {
        time_t tnow = time(nullptr);
        if(tnow > 1600000000) timeOK = true;
      }
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
  canvas.setViewport(28,106,105,83,false);
  for(int ring=0;ring<3;ring++) {
    int ox=0,oy=0;
    for(int j=0;j<=48;j++) {
      double a=j*2*PI/48;
      auto p=CompassMath::project(ring==1?0:cos(a),ring==2?0:sin(a),ring==0?0:ring==1?cos(a):sin(a),azimuth,pitchDeg,rollDeg);
      if(j)canvas.drawLine(ox,oy,p.x,p.y,white());
      ox=p.x;oy=p.y;
    }
  }
  const char *labels[]={"N","E","S","W"};
  const double east[]={0,1,0,-1},north[]={1,0,-1,0};
  for(int i=0;i<4;i++) {
    auto p=CompassMath::project(east[i],north[i],0,azimuth,pitchDeg,rollDeg);
    int x=constrain(p.x-3,28,126),y=constrain(p.y-4,106,180);
    canvas.fillRect(x-1,y-1,8,10,TFT_BLACK);text(labels[i],x,y,white(),1);
  }
  auto center = CompassMath::projectLocal(0, 0, 0);
  auto arrow_tip = CompassMath::projectLocal(0, 0.7, 0); 
  auto arrow_left = CompassMath::projectLocal(-0.15, 0.5, 0);
  auto arrow_right = CompassMath::projectLocal(0.15, 0.5, 0);
  
  canvas.drawLine(center.x, center.y, arrow_tip.x, arrow_tip.y, orange());
  canvas.drawLine(arrow_left.x, arrow_left.y, arrow_tip.x, arrow_tip.y, orange());
  canvas.drawLine(arrow_right.x, arrow_right.y, arrow_tip.x, arrow_tip.y, orange());
  canvas.resetViewport();
}
void drawBattery() {
  bool low=batteryPercent>=0&&batteryPercent<15&&!charging;
  if(low&&(frameNow/450)%2)return;
  uint16_t color=low?orange():green();
  if(charging) {
    canvas.setBitmapColor(green(),TFT_BLACK);
    canvas.drawBitmap(284,211,charging_battery_32_32_28f_frames[(frameNow/42)%28],32,32,night?TFT_RED:0x04B1);
  } else {
    canvas.drawRect(285,217,27,14,color);canvas.fillRect(312,221,3,6,color);
    if(batteryPercent>=0)canvas.fillRect(287,219,batteryPercent*23/100,10,color);
    text(batteryPercent<0?"--%":String(batteryPercent)+"%",284,202,color,1);
  }
}
void drawWifi() {
  if(!clients) {
    canvas.setBitmapColor(green(),TFT_BLACK);
    canvas.drawBitmap(246,209,wifi_search_32_32_28f_frames[(frameNow/42)%28],32,32,night?TFT_RED:0x04B1);
  } else {
    for(int i=0;i<4;i++)canvas.fillRect(248+i*7,233-(i+1)*5,5,(i+1)*5,i<wifiBars?green():dark());
  }
}
void drawHome() {
  canvas.setSwapBytes(false);
  if(night) {
    // Fill top bar black in night mode so red icons are visible
    canvas.fillRect(0,0,320,60,TFT_BLACK);
  } else {
    canvas.pushImage(0,0,320,60,image_paint_1_pixels);
  }
  for(int x: {70,140,210,278})canvas.drawLine(x,58,x,1,night?TFT_BLACK:0xFB20);
  
  if(showCursor) {
    int cursorXs[] = {2, 72, 142, 212};
    int cursorWs[] = {66, 66, 66, 64};
    canvas.drawRoundRect(cursorXs[cursorIndex], 2, cursorWs[cursorIndex], 56, 4, orange());
  }

  canvas.drawBitmap(11,5,image_menu_home_bits,45,48,ink());
  if(trackMode == TRACK_LUNAR) {
    canvas.drawBitmap(82,6,image_moon_bits,48,48,ink());
  } else if(trackMode == TRACK_SIDEREAL) {
    // Custom constellation icon for Sidereal to differentiate from Catalog
    canvas.drawLine(85, 20, 105, 35, ink());
    canvas.drawLine(105, 35, 120, 15, ink());
    canvas.fillCircle(85, 20, 4, ink());
    canvas.fillCircle(105, 35, 5, ink());
    canvas.fillCircle(120, 15, 3, ink());
  } else {
    canvas.fillCircle(105, 30, 12, orange());
    for(int i=0;i<8;i++) {
      float a = i * PI / 4.0f;
      canvas.drawLine(105 + cos(a)*15, 30 + sin(a)*15, 105 + cos(a)*20, 30 + sin(a)*20, orange());
    }
  }
  canvas.drawBitmap(156,8,image_Pin_star_bits,42,42,ink());
  canvas.drawBitmap(219,6,image_menu_settings_gear_bits,48,48,ink());
  
  canvas.setTextDatum(TL_DATUM);canvas.setTextColor(green(),TFT_BLACK);
  canvas.setTextSize(1);canvas.setFreeFont(&FreeSerifBold18pt7b);
  canvas.drawString(locationOK&&latitude<0?"S":"N",288,18);
  canvas.setTextColor(ink(),TFT_BLACK);canvas.setTextSize(4);canvas.setFreeFont(&Org_01);
  canvas.drawString("RA:",170,131);canvas.drawString("DEC:",146,167);
  canvas.setTextSize(2);canvas.setTextColor(night?TFT_RED:0xF206,TFT_BLACK);
  if(timeOK) {time_t local=time(nullptr)+timezoneMinutes*60;tm t;gmtime_r(&local,&t);char buf[20];strftime(buf,sizeof(buf),"%I:%M %p",&t);canvas.drawString(buf,231,67);}
  else canvas.drawString("--:-- --",231,67);
  drawCompass();
  bool valid=imuOK&&timeOK&&locationOK;
  canvas.setFreeFont(&Org_01);canvas.setTextSize(2);canvas.setTextColor(ink(),TFT_BLACK);
  if(valid) {
    int s=((int)lround(raHours*3600))%86400;char ra[20],dec[20];
    snprintf(ra,sizeof(ra),"%02d:%02d:%02d",s/3600,s/60%60,s%60);snprintf(dec,sizeof(dec),"%+.2f",decDeg);
    canvas.drawString(ra,232,136);canvas.drawString(dec,232,172);
  } else {canvas.drawString("--:--:--",232,136);canvas.drawString("--.--",232,172);}
  canvas.setBitmapColor(ink(),TFT_BLACK);
  canvas.drawBitmap(2,61,compass_32_32_28f_frames[(frameNow/42)%28],32,32,night?TFT_RED:0x04B1);
  button("STOP",5,212,64,25,orange());
  if(mount.message!="Select a target")text(mount.message.substring(0,27),75,221,orange(),1);
  drawWifi();drawBattery();
  
  if(showCompletePopup || showHomingErrorPopup) {
     // Dimming effect using dither pattern
     for(int dy = 0; dy < bandHeight; dy++) {
        int absY = bandY + dy;
        for(int dx = absY % 2; dx < 320; dx += 2) {
           // Don't dim inside the popup
           if(dx >= 50 && dx <= 270 && absY >= 60 && absY <= 160) continue;
           canvas.drawPixel(dx, dy, TFT_BLACK);
        }
     }
     
     // Draw Popup Box
     canvas.fillRoundRect(50, 60, 220, 100, 8, TFT_DARKGREY);
     canvas.drawRoundRect(50, 60, 220, 100, 8, showHomingErrorPopup ? orange() : white());
     
     // Title
     canvas.setTextDatum(MC_DATUM);
     canvas.setFreeFont(nullptr);
     
     if (showHomingErrorPopup) {
       canvas.setTextSize(2);
       canvas.setTextColor(orange(), TFT_DARKGREY);
       canvas.drawString("HOMING ABORTED", 160, 80, 1);
       canvas.setTextSize(1);
       canvas.setTextColor(white(), TFT_DARKGREY);
       canvas.drawString("Sensor Timeout / Wire Break", 160, 105, 1);
     } else {
       canvas.setTextSize(2);
       canvas.setTextColor(white(), TFT_DARKGREY);
       canvas.drawString("TRACKING COMPLETE", 160, 90, 1);
     }
     
     canvas.setTextDatum(TL_DATUM);
     
     // Close Button
     filledButton("CLOSE", 120, 120, 80, 30, orange(), TFT_BLACK);
  }
}
void backHeader(const char *title) {
  canvas.drawBitmap(8,28,image_arrow_curved_up_left_bits,32,20,white());
  if (page != SET_TIMER) canvas.drawFastHLine(51,58,269,white());
  text(title,61,20,ink());
}
void drawCatalog() {
  canvas.drawRect(50,1,270,238,night?TFT_RED:0xE8EC);
  backHeader("OBJECT CATALOG");
  const char *labels[]={"ALL","CONST","CLUST","GALAX","NEB"};
  for(int i=0;i<5;i++) {
    canvas.fillRect(1,80+i*32,48,30,category==i?(night?0x3200:0x3204):(night?0x0800:0x0841));
    text(labels[i],5,90+i*32,category==i?white():ink(),1);
  }
  canvas.setViewport(51,60,265,179,false);
  int rows=rowCount();
  for(int n=scrollY/36;n<rows;n++) {
    int y=60+n*36-scrollY;if(y>=239)break;
    int i=filtered(n);text(targets[i].name,58,y+1,ink());text(targets[i].detail,58,y+21,grey(),1);
    canvas.drawFastHLine(54,y+35,259,dark());
  }
  canvas.resetViewport();
  if(rows*36>180) {int h=max(10,180*180/(rows*36));int y=60+scrollY*(180-h)/(rows*36-180);canvas.fillRect(317,y,2,h,ink());}
}


void drawConfirm() {
  backHeader("TARGET CONFIRM");
  if(selected>=0) {
    text(targets[selected].name, 59, 66, ink());
    text(targets[selected].detail, 59, 86, grey(), 1);
    
    char raBuf[16], decBuf[16];
    double ra = targets[selected].ra_h;
    double dec = targets[selected].dec_d;
    
    int ra_h = (int)ra;
    int ra_m = (int)((ra - ra_h) * 60);
    sprintf(raBuf, "%02d:%02d", ra_h, ra_m);
    
    char sign = dec < 0 ? '-' : '+';
    double dec_abs = abs(dec);
    int dec_d = (int)dec_abs;
    int dec_m = (int)((dec_abs - dec_d) * 60);
    sprintf(decBuf, "%c%02d*%02d", sign, dec_d, dec_m);
    
    text(String("RA  ") + raBuf, 59, 122, ink());
    text(String("DEC ") + decBuf, 59, 143, ink());
  }
  
  text("Ready to slew.", 59, 173, green(), 1);
  
  // 3 Solid Buttons - Centered layout. (320 - (90*3 + 20)) / 2 = 15
  filledButton("APP", 15, 194, 90, 38, TFT_DARKGREY, TFT_WHITE);
  filledButton("NASA", 115, 194, 90, 38, TFT_DARKGREY, TFT_WHITE);
  filledButton("OFFLINE", 215, 194, 90, 38, GREEN, TFT_BLACK);
}
void drawSettings(bool site) {
  backHeader(site?"SITE / HEMISPHERE":"SETTINGS");
  text(String("Wi-Fi: ") + WIFI_SSID, 60, 67, ink());
  text(clients ? "Connected" : "Connecting...", 60, 89, ink());
  text("IP: " + WiFi.localIP().toString(), 60, 111, orange());
  if(site) {
    text("Lat: "+String(latitude,4),60,142,white());text("Lon: "+String(longitude,4),60,164,white());
    text(locationOK?(latitude<0?"South celestial pole":"North celestial pole"):"Set site on phone",60,194,ink());
  } else {
    // New Settings Layout
    button(dimScreen?"BRIGHTNESS: LOW":"BRIGHTNESS: HIGH",60,135,160,35,ink());
    button(night?"RED MODE: ON":"RED MODE: OFF",60,180,160,35,ink());
    
    // Calibrate IMU button (split text to fit 90x80 box)
    uint16_t c = grey();
    canvas.drawRoundRect(225, 135, 90, 80, 4, c);
    canvas.setTextDatum(MC_DATUM);
    canvas.setTextColor(c, TFT_BLACK);
    canvas.drawString("CALIBRATE", 270, 165, 1); // Font 1 (small)
    canvas.drawString("IMU", 270, 185, 2);       // Font 2 (larger)
    canvas.setTextDatum(TL_DATUM);
  }
}

void drawSetTimer() {
  backHeader("SET TRACKING DURATION");
  
  // Highlight window
  canvas.drawFastHLine(50, 85, 220, ink());
  canvas.drawFastHLine(50, 135, 220, ink());
  
  char buf[8];
  canvas.setFreeFont(nullptr);
  canvas.setTextDatum(MC_DATUM);
  
  // HOURS Tumbler
  int hPrev = (trackHours + 23) % 24;
  int hNext = (trackHours + 1) % 24;
  int yOffH = (int)timerScrollH;
  
  canvas.setTextSize(2);
  canvas.setTextColor(grey(), TFT_BLACK);
  sprintf(buf, "%02d", hPrev); canvas.drawString(buf, 100, 65 - yOffH, 1);
  sprintf(buf, "%02d", hNext); canvas.drawString(buf, 100, 155 - yOffH, 1);
  
  canvas.setTextSize(4);
  canvas.setTextColor(white(), TFT_BLACK);
  sprintf(buf, "%02d", trackHours); canvas.drawString(buf, 100, 110 - yOffH, 1);
  
  // COLON
  canvas.setTextSize(4);
  canvas.setTextColor(grey(), TFT_BLACK);
  canvas.drawString(":", 160, 105, 1);
  
  // MINS Tumbler
  int mPrev = (trackMins + 59) % 60;
  int mNext = (trackMins + 1) % 60;
  int yOffM = (int)timerScrollM;
  
  canvas.setTextSize(2);
  canvas.setTextColor(grey(), TFT_BLACK);
  sprintf(buf, "%02d", mPrev); canvas.drawString(buf, 220, 65 - yOffM, 1);
  sprintf(buf, "%02d", mNext); canvas.drawString(buf, 220, 155 - yOffM, 1);
  
  canvas.setTextSize(4);
  canvas.setTextColor(white(), TFT_BLACK);
  sprintf(buf, "%02d", trackMins); canvas.drawString(buf, 220, 110 - yOffM, 1);

  
  // Estimated End Time
  time_t now = time(nullptr);
  time_t endT = now + (timezoneMinutes * 60) + (trackHours * 3600) + (trackMins * 60);
  struct tm tinfo;
  gmtime_r(&endT, &tinfo);
  char endBuf[32];
  if(timeOK) {
    sprintf(endBuf, "Est. finish -> %02d:%02d", tinfo.tm_hour, tinfo.tm_min);
  } else {
    sprintf(endBuf, "(Time not synced)");
  }
  canvas.setTextSize(1);
  canvas.setTextColor(orange(), TFT_BLACK);
  canvas.drawString(endBuf, 160, 185, 2);
  
  canvas.setTextDatum(TL_DATUM);
  
  // Start Button
  filledButton("START TRACKING", 40, 200, 240, 35, GREEN, TFT_BLACK);
}

void drawTracking() {
  canvas.fillRect(0,0,320,240,TFT_BLACK);
  canvas.setTextDatum(MC_DATUM);
  canvas.setTextColor(white(), TFT_BLACK);
  canvas.drawString("TRACKING IN PROGRESS", 160, 30, 2);
  canvas.setTextDatum(TL_DATUM);

  int cx = 160, cy = 110;
  float angle = (millis() % 2000) / 2000.0 * 2 * PI;
  
  for(int i=0; i<2; i++) {
     float a = angle + i * PI;
     int ax = cx + cos(a) * 45;
     int ay = cy + sin(a) * 45;
     
     // trailing dots
     for(int j=1; j<6; j++) {
        canvas.fillCircle(cx + cos(a - j*0.15) * 45, cy + sin(a - j*0.15) * 45, 3, dark());
     }
     
     // Arrow head pointing tangentially
     float ta = a + PI/2.0f;
     int px1 = ax + cos(ta + 2.5f) * 12;
     int py1 = ay + sin(ta + 2.5f) * 12;
     int px2 = ax + cos(ta - 2.5f) * 12;
     int py2 = ay + sin(ta - 2.5f) * 12;
     canvas.fillTriangle(ax, ay, px1, py1, px2, py2, ink());
  }

  // Handle completion
  time_t now = time(nullptr);
  if(trackState == TS_RUNNING && now >= trackEndTime) {
      trackState = TS_COMPLETE;
      showCompletePopup = true;
      mount.stop();
      changePage(HOME);
  }

  // Single CANCEL button at the bottom
  filledButton("CANCEL", 100, 190, 120, 40, orange(), TFT_BLACK);
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
void drawHoming() {
  canvas.fillRect(0,0,320,240,TFT_BLACK);
  // House icon (Centered)
  canvas.fillRect(135, 100, 50, 40, orange()); // House base
  canvas.fillTriangle(160, 70, 125, 100, 195, 100, green()); // Roof
  canvas.fillRect(150, 120, 20, 20, TFT_BLACK); // Door
  
  canvas.setTextDatum(MC_DATUM);
  canvas.setFreeFont(nullptr); // Use built-in font to fit screen!
  canvas.setTextSize(2); // Size 2 fits well
  canvas.setTextColor(TFT_WHITE);
  canvas.drawString("HOMING...", 160, 170);
  canvas.setTextDatum(TL_DATUM);
}

void drawCatalogMode() {
  backHeader("SELECT CATALOG MODE");
  // Centered: (320 - 260) / 2 = 30
  button("1. APP (STELLARIUM WIFI)", 30, 70, 260, 40, ink());
  button("2. ONLINE (NASA API)", 30, 120, 260, 40, ink());
  button("3. OFFLINE (INTERNAL)", 30, 170, 260, 40, ink());
}

void render() {
  frameNow=millis();
  if(page==BOOT && frameNow-bootAt>=3000) {
    if(touchCalibrated)changePage(HOME);else startCalibration();
  }
  for(bandY=0;bandY<240;bandY+=bandHeight) {
  canvas.beginBand(bandY);canvas.setFreeFont(nullptr);canvas.setTextSize(1);canvas.setTextDatum(TL_DATUM);
  switch(page) {
    case BOOT:drawBoot(frameNow-bootAt);break;
    case HOME:drawHome();break;
    case CATALOG:drawCatalog();break;
    case CONFIRM:drawConfirm();break;
    case SETTINGS:drawSettings(false);break;
    case LOCATION:drawSettings(true);break;
    case SET_TIMER:drawSetTimer();break;
    case TRACKING:drawTracking();break;
    case HOMING:drawHoming();break;
    case QUICKMODE:
      backHeader("QUICK TRACKING");
      // Centered: (320 - 240) / 2 = 40
      button(trackMode==TRACK_SIDEREAL?"> SIDEREAL (STARS)":"SIDEREAL (STARS)", 40, 70, 240, 40, trackMode==TRACK_SIDEREAL?green():ink());
      button(trackMode==TRACK_LUNAR?"> LUNAR (MOON)":"LUNAR (MOON)", 40, 120, 240, 40, trackMode==TRACK_LUNAR?green():ink());
      button(trackMode==TRACK_SOLAR?"> SOLAR (SUN)":"SOLAR (SUN)", 40, 170, 240, 40, trackMode==TRACK_SOLAR?green():ink());
      break;
    case NIGHT:backHeader("DISPLAY BRIGHTNESS");button(night?"DIM: ON":"DIM: OFF",60,90,245,42);button("TOGGLE BRIGHTNESS",60,150,245,42);break;
    case CALIBRATE:drawCalibration();break;
  }
  canvas.pushSprite(0,bandY);
  yield();
  }
}
void setup() {
  Serial.begin(115200);tft.init();tft.setRotation(1);tft.fillScreen(TFT_BLACK);
  ledcSetup(0,20000,8);ledcAttachPin(21,0);ledcWrite(0,220);
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
  
  // Initialize Serial2 for communicating with Main Board
  Serial2.begin(MAIN_BAUD, SERIAL_8N1, MAIN_RX, MAIN_TX);
  Serial2.setTimeout(10); // Prevent blocking/flickering!
  
  setupPhone();mount.begin();bootAt=millis();
  Serial.println("CYD ILI9341 + XPT2046 ready. Send C to recalibrate touch.");
}
void loop() {
  while(Serial.available()){char c=Serial.read();if((c=='c'||c=='C')&&!mount.busy)startCalibration();}
  pollSerial2(); // Read commands from Main Board
  server.handleClient();mount.tick();sampleSensors();pollTouch();
  if(millis()-lastFrame>=42) {lastFrame=millis();render();}
  delay(1);
}

