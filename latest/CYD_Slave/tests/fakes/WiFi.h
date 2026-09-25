#pragma once
// Minimal host-only transport adapter: firmware uses the real ESP32 WiFi.h.
#include <string>
#include <vector>
#include <cstdint>
#include <algorithm>
constexpr int LOW=0,SERIAL_8N1=0;
class String {
 public:
  std::string s;
  String()=default;
  String(const char *p):s(p){}
  String(std::string p):s(p){}
  size_t length() const {return s.size();}
  const char *c_str() const {return s.c_str();}
  String substring(size_t n) const {return s.substr(n);}
  void replace(char a,char b){std::replace(s.begin(),s.end(),a,b);}
  String &operator+=(char c){s+=c;return *this;}
  friend String operator+(const String&a,const String&b){return a.s+b.s;}
};
inline uint32_t fakeTime=0;
inline uint32_t millis(){return fakeTime;}
namespace Fake {
inline bool connected=false,canConnect=true;
inline std::string rx;
inline std::vector<std::string> tx;
inline void reset(){connected=false;canConnect=true;rx.clear();tx.clear();fakeTime=0;}
inline void feed(const char *s){rx+=s;}
}
class IPAddress {public:bool fromString(const String &s){return !s.s.empty();}};
class WiFiClient {
 public:
  bool connected(){return Fake::connected;}
  bool connect(IPAddress,uint16_t,int){return Fake::connected=Fake::canConnect;}
  void stop(){Fake::connected=false;Fake::rx.clear();}
  int available(){return Fake::rx.size();}
  char read(){char c=Fake::rx.front();Fake::rx.erase(0,1);return c;}
  void print(const String&s){Fake::tx.push_back(s.s);}
};
class SerialFake:public WiFiClient {public:void begin(int,int,int,int){}};
inline SerialFake Serial2;
