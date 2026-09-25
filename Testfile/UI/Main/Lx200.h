#pragma once
#include <WiFi.h>
#include "Astro.h"
#include "Config.h"
class Lx200 {
 public:
  String host,info,ra,dec,message="Select a target";
  uint16_t port=4030;
  bool ready=false,busy=false;
  void begin() { if(LX_SERIAL && LX_RX>=0 && LX_TX>=0) Serial2.begin(LX_BAUD,SERIAL_8N1,LX_RX,LX_TX); }
  void cancel() { busy=false; ready=false; phase=0; client.stop(); }
  bool connect() {
    if(LX_SERIAL) return LX_RX>=0&&LX_TX>=0;
    if(client.connected()) return true;
    IPAddress ip;
    return ip.fromString(host)&&client.connect(ip,port,150);
  }
  void select(int id) {
    cancel(); ra=""; dec=""; info="";
    if(!connect()) { message="LX200 not connected"; return; }
    drain(); char command[16]; snprintf(command,sizeof(command),":LM%04d#",id);
    send(command); busy=true; phase=1; stamp=millis(); message="Reading LX200...";
  }
  void start() {
    if(!ready||busy) return;
    if(!connect()) { fail("LX200 disconnected"); return; }
    ready=false; busy=true;
    request(String(":Sr")+ra+"#",5); message="Setting target...";
  }
  void stop() {
    bool connected=connect();
    if(connected) send(":Q#");
    cancel(); message=connected?"Stop command sent":"Stop failed: no link";
  }
  void tick() {
    if(!busy) return;
    if(!LX_SERIAL&&!client.connected()) { fail("LX200 disconnected"); return; }
    if(phase==1) {
      if(millis()-stamp<150) return;
      request(":LI#",2); return;
    }
    while(available()) {
      char c=read();
      if(c==0x15) { fail("LX200 busy / rejected"); return; }
      if(c=='\r'||c=='\n') continue;
      if(phase==5||phase==6) {
        if(c!='1') { fail("Target rejected"); return; }
        if(phase==5) { String normalized=dec;normalized.replace('\'',':');normalized.replace((char)223,'*');request(String(":Sd")+normalized+"#",6); }
        else { request(":MS#",7); message="Starting slew..."; }
        return;
      }
      if(phase==7 && buffer.length()==0) {
        if(c=='0') { busy=false; phase=0; message="Slew accepted"; return; }
        if(c!='1'&&c!='2') { fail("Invalid slew reply"); return; }
        buffer+=c; continue;
      }
      if(c=='#') { complete(); return; }
      if(buffer.length()>=100) { fail("LX200 reply too long"); return; }
      buffer+=c;
    }
    if(millis()-stamp>1800) fail(phase==7?"Slew reply lost: use STOP":"LX200 timeout / unsupported");
  }
 private:
  WiFiClient client;
  uint8_t phase=0;
  uint32_t stamp=0;
  String buffer;
  int available() { return LX_SERIAL?Serial2.available():client.available(); }
  char read() { return LX_SERIAL?Serial2.read():client.read(); }
  void drain() { int n=0; while(available()&&n++<512) read(); }
  void send(const String &s) { if(LX_SERIAL) Serial2.print(s); else client.print(s); }
  void request(const String &s,int p) { drain(); buffer=""; phase=p; stamp=millis(); send(s); }
  void fail(const char *why) { busy=false; ready=false; phase=0; message=why; client.stop(); }
  void complete() {
    if(phase==2) {
      if(buffer.length()==0) { fail("No LX200 object info"); return; }
      info=buffer; request(":Gr#",3);
    } else if(phase==3) {
      if(!Astro::parseRA(buffer.c_str())) { fail("Invalid target RA"); return; }
      ra=buffer; request(":Gd#",4);
    } else if(phase==4) {
      if(!Astro::parseDec(buffer.c_str())) { fail("Invalid target DEC"); return; }
      dec=buffer; ready=true; busy=false; phase=0; message="Start working?";
    } else if(phase==7) { message="Slew rejected: "+buffer.substring(1); busy=false; ready=false; phase=0; }
  }
};
