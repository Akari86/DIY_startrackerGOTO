#pragma once
#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <TJpg_Decoder.h>
#include <SPIFFS.h>

extern TFT_eSPI tft;

class NasaApi {
public:
  String apiKey = "PHmXyqeHAaMb00fH1fWySRPqICNN69adEpOYwC80";
  String apodTitle = "";
  String apodUrl = "";
  bool apodFetched = false;
  
  // For JPL Horizons Ephemeris
  struct Ephemeris {
    bool valid = false;
    double raHours;
    double decDeg;
  };
  
  static bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
    if (y >= tft.height()) return 0;
    tft.pushImage(x, y, w, h, bitmap);
    return 1;
  }
  
  void initDecoder() {
    TJpgDec.setJpgScale(2); // Scale down by 2 (e.g. 800x600 -> 400x300 to fit better)
    TJpgDec.setSwapBytes(true);
    TJpgDec.setCallback(tft_output);
  }
  
  bool fetchAPOD() {
    if(WiFi.status() != WL_CONNECTED) return false;
    
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    
    String url = "https://api.nasa.gov/planetary/apod?api_key=" + apiKey;
    http.begin(client, url);
    int httpCode = http.GET();
    
    if (httpCode == 200) {
      String payload = http.getString();
      DynamicJsonDocument doc(2048);
      DeserializationError error = deserializeJson(doc, payload);
      
      if (!error) {
        if (doc["media_type"] == "image") {
          apodTitle = doc["title"].as<String>();
          apodUrl = doc["url"].as<String>(); // Use standard URL, not HD, for RAM limits
          apodFetched = true;
          http.end();
          return true;
        }
      }
    }
    http.end();
    return false;
  }
  
  bool drawAPOD() {
    if (!apodFetched) {
      if(!fetchAPOD()) return false;
    }
    
    if (apodUrl == "") return false;
    
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Loading NASA APOD...", 160, 120, 2);
    
    // Draw the image directly from URL
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.begin(client, apodUrl);
    int httpCode = http.GET();
    
    if (httpCode == 200) {
      tft.fillScreen(TFT_BLACK);
      
      // Download to SPIFFS
      File f = SPIFFS.open("/apod.jpg", FILE_WRITE);
      if (f) {
        http.writeToStream(&f);
        f.close();
        // Draw from SPIFFS
        TJpgDec.drawJpg(0, 0, "/apod.jpg");
      }
      
      // Draw Title Bar over it
      tft.fillRect(0, 0, 320, 30, 0x0000); // Solid black bg
      tft.setTextColor(TFT_ORANGE);
      tft.setTextDatum(MC_DATUM);
      tft.drawString(apodTitle, 160, 15, 2);
      
      http.end();
      return true;
    }
    
    http.end();
    return false;
  }
  
  // Fetch real-time RA/DEC of a planet using JPL Horizons API
  // commandID: '499' for Mars, '399' for Earth, '301' for Moon, etc.
  Ephemeris getPlanetEphemeris(String commandID) {
    Ephemeris result;
    if(WiFi.status() != WL_CONNECTED) return result;
    
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    
    // Construct Horizons API URL.
    // We request apparent RA/DEC (quantities=1) for the current moment.
    // Since we don't have exact UTC easily, we can request a 1-day step from today and just parse the first result.
    String url = "https://ssd.jpl.nasa.gov/api/horizons.api?format=text";
    url += "&COMMAND='" + commandID + "'";
    url += "&OBJ_DATA='NO'";
    url += "&MAKE_EPHEM='YES'";
    url += "&EPHEM_TYPE='OBSERVER'";
    url += "&CENTER='500@399'"; // Geocentric
    url += "&START_TIME='2026-09-26'"; // Hardcoded today for now, ideally dynamic
    url += "&STOP_TIME='2026-09-27'";
    url += "&STEP_SIZE='1%20d'";
    url += "&QUANTITIES='1'";
    
    http.begin(client, url);
    int httpCode = http.GET();
    
    if (httpCode == 200) {
      String payload = http.getString();
      
      // Parsing JPL Horizons Text Output
      // Look for $$SOE (Start of Ephemeris)
      int soeIndex = payload.indexOf("$$SOE");
      if (soeIndex > 0) {
        // Find the first line after $$SOE
        int startLine = payload.indexOf('\n', soeIndex) + 1;
        int endLine = payload.indexOf('\n', startLine);
        if (endLine > startLine) {
          String dataLine = payload.substring(startLine, endLine);
          // Example line:
          //  2026-Sep-26 00:00     05 53 43.19 +23 58 13.9 
          // It's fixed width.
          // Date string ends around char 23.
          // RA is after date.
          
          // Let's use a simple tokenizer based on spaces.
          // Horizons format can vary slightly but RA/DEC are the columns right after Date/Time and any presence flags (like * or m).
          // We will find the first number that isn't the date.
          
          // Actually, we can locate RA / DEC by knowing it's 3 numbers for RA and 3 for DEC.
          // But a more robust way is to just grab substrings if format is constant.
          // JPL output: " 2026-Sep-26 00:00     05 53 43.19 +23 58 13.9"
          // We can find the first space after the time, then parse.
          
          // Let's just find the colon in the time, then skip to the RA.
          int timeColon = dataLine.indexOf(':');
          if (timeColon > 0) {
            String coords = dataLine.substring(timeColon + 3);
            coords.trim();
            // coords = "05 53 43.19 +23 58 13.9"
            
            // Extract RA
            int sp1 = coords.indexOf(' ');
            int sp2 = coords.indexOf(' ', sp1 + 1);
            int sp3 = coords.indexOf(' ', sp2 + 1);
            
            String raH = coords.substring(0, sp1);
            String raM = coords.substring(sp1 + 1, sp2);
            String raS = coords.substring(sp2 + 1, sp3);
            
            // Extract DEC
            coords = coords.substring(sp3 + 1);
            coords.trim();
            sp1 = coords.indexOf(' ');
            sp2 = coords.indexOf(' ', sp1 + 1);
            
            String decD = coords.substring(0, sp1);
            String decM = coords.substring(sp1 + 1, sp2);
            String decS = coords.substring(sp2 + 1);
            
            double raHours = raH.toFloat() + (raM.toFloat() / 60.0) + (raS.toFloat() / 3600.0);
            
            double d = decD.toFloat();
            double sign = (d < 0 || decD.indexOf('-') != -1) ? -1.0 : 1.0;
            double decDeg = abs(d) + (decM.toFloat() / 60.0) + (decS.toFloat() / 3600.0);
            decDeg *= sign;
            
            result.raHours = raHours;
            result.decDeg = decDeg;
            result.valid = true;
          }
        }
      }
    }
    http.end();
    return result;
  }
};

extern NasaApi nasaApi;
