#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include "Config.h"

// External stepper references from main.cpp
extern AccelStepper raMotor;
extern AccelStepper decMotor;
extern TrackingMode currentMode;

WiFiServer lx200Server(10001); // Standard LX200 port
WiFiClient lx200Client;

// Astronomical variables
double currentRA = 0.0;  // Hours (0-24)
double currentDEC = 90.0; // Degrees (-90 to +90, Polaris default)
double targetRA = 0.0;
double targetDEC = 0.0;

// Constants for step calculations
// 360 degrees = STEPS_PER_REV * MICROSTEPS * GEAR_RATIO
const float STEPS_PER_DEGREE_RA = (STEPS_PER_REV * MICROSTEPS * GEAR_RATIO_RA) / 360.0;
const float STEPS_PER_DEGREE_DEC = (STEPS_PER_REV * MICROSTEPS * GEAR_RATIO_DEC) / 360.0;

void setupWiFiAP() { // Renamed internally but keeping function name same so main.cpp doesn't break
  WiFi.mode(WIFI_STA);
  
  // เปลี่ยนชื่อ WiFi และรหัสผ่านตรงนี้ เป็นของบ้านคุณ
  const char* ssid = "Kitti's Galaxy S22 Ultra";
  const char* pass = "32013446";
  
  Serial.println("----------------------------------------");
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);
  
  WiFi.begin(ssid, pass);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  Serial.println();
  
  if(WiFi.status() == WL_CONNECTED) {
    lx200Server.begin();
    Serial.println(" WiFi Connected Successfully!");
    Serial.print(" 👉 IP Address สำหรับกรอกในแอป: ");
    Serial.println(WiFi.localIP());
    Serial.println(" Port: 10001");
  } else {
    Serial.println(" WiFi Connection Failed! (Check SSID/Password)");
  }
  Serial.println("----------------------------------------");
}

void processLx200() {
  if (!lx200Client || !lx200Client.connected()) {
    WiFiClient newClient = lx200Server.available();
    if (newClient) {
      lx200Client = newClient;
      Serial.println("LX200 Client Connected via WiFi!");
    }
  }
  
  if (lx200Client && lx200Client.available()) {
    static String lx200Buffer = "";
    
    while (lx200Client.available()) {
      char c = lx200Client.read();
      
      // Stellarium sends 0x06 (ACK) to probe alignment status. It does NOT end with '#'
      if (c == 0x06) {
        Serial.println("LX200 Received: <ACK> (0x06)");
        lx200Client.print("P"); // Reply 'P' (Polar Aligned) or 'A' (AltAz). No '#' needed.
        continue; 
      }
      
      if (c == '#') {
        String cmd = lx200Buffer;
        lx200Buffer = ""; // Reset buffer
        cmd.trim();
        if (cmd.length() == 0) continue;
        
        Serial.print("LX200 Received: ");
        Serial.println(cmd + "#");
        
        if (cmd.indexOf("GVP") != -1) {
          lx200Client.print("LX200#");
        }
        else if (cmd.indexOf("GVN") != -1) {
          lx200Client.print("1.0#");
        }
        else if (cmd.indexOf("GR") != -1) {
          char buf[16];
          int h = (int)currentRA;
          int m = (int)((currentRA - h) * 60);
          int s = (int)((((currentRA - h) * 60) - m) * 60);
          sprintf(buf, "%02d:%02d:%02d#", h, m, s);
          lx200Client.print(buf);
        } 
        else if (cmd.indexOf("GD") != -1) {
          char buf[16];
          char sign = currentDEC < 0 ? '-' : '+';
          double d_abs = abs(currentDEC);
          int d = (int)d_abs;
          int m = (int)((d_abs - d) * 60);
          int s = (int)((((d_abs - d) * 60) - m) * 60);
          sprintf(buf, "%c%02d*%02d:%02d#", sign, d, m, s);
          lx200Client.print(buf);
        }
        else if (cmd.indexOf("Sr") != -1) {
          int h=0, m=0, s=0;
          sscanf(cmd.c_str(), ":Sr%d:%d:%d", &h, &m, &s);
          targetRA = h + (m/60.0) + (s/3600.0);
          lx200Client.print("1");
        }
        else if (cmd.indexOf("Sd") != -1) {
          char sign = '+';
          int d=0, m=0, s=0;
          sscanf(cmd.c_str(), ":Sd%c%d*%d:%d", &sign, &d, &m, &s);
          targetDEC = d + (m/60.0) + (s/3600.0);
          if (sign == '-') targetDEC = -targetDEC;
          lx200Client.print("1");
        }
        else if (cmd.indexOf("CM") != -1) {
          currentRA = targetRA;
          currentDEC = targetDEC;
          raMotor.setCurrentPosition(0);
          decMotor.setCurrentPosition(0);
          lx200Client.print("Polaris#"); 
          Serial.println("SYNC: Mount calibrated to new RA/DEC");
        }
        else if (cmd.indexOf("MS") != -1) {
          double deltaRA_Hours = targetRA - currentRA;
          double deltaDEC_Deg = targetDEC - currentDEC;
          
          if (deltaRA_Hours > 12.0) deltaRA_Hours -= 24.0;
          if (deltaRA_Hours < -12.0) deltaRA_Hours += 24.0;
          
          double deltaRA_Deg = deltaRA_Hours * 15.0;
          
          long stepsToMoveRA = (long)(deltaRA_Deg * STEPS_PER_DEGREE_RA);
          long stepsToMoveDEC = (long)(deltaDEC_Deg * STEPS_PER_DEGREE_DEC);
          
          digitalWrite(RA_EN, LOW); 
          digitalWrite(DEC_EN, LOW);
          
          raMotor.move(stepsToMoveRA);
          decMotor.move(stepsToMoveDEC);
          
          currentMode = TRACK_GOTO;
          Serial.printf("GOTO: Moving RA %ld steps, DEC %ld steps\n", stepsToMoveRA, stepsToMoveDEC);
          
          lx200Client.print("0"); 
        }
        else if (cmd.indexOf("Q") != -1) {
          currentMode = TRACK_STOPPED;
          raMotor.stop();
          decMotor.stop();
          digitalWrite(RA_EN, HIGH); 
          digitalWrite(DEC_EN, HIGH);
          Serial.println("STOP: Halt commanded via WiFi");
        }
      } else {
        lx200Buffer += c;
      }
    }
  }
}

// Helper to update current coordinates if we are tracking/slewing
void updateCoordinates() {
  if (currentMode == TRACK_GOTO || currentMode == TRACK_SIDEREAL) {
    // Read current steps and convert back to RA/DEC so SkySafari crosshair moves smoothly!
    long currentStepsRA = raMotor.currentPosition();
    long currentStepsDEC = decMotor.currentPosition();
    
    // When we sync (:CM), steps become 0, meaning (targetRA, targetDEC) corresponds to step 0.
    // So current = synced + (steps / STEPS_PER_DEGREE)
    double deltaRA_Deg = currentStepsRA / STEPS_PER_DEGREE_RA;
    double deltaDEC_Deg = currentStepsDEC / STEPS_PER_DEGREE_DEC;
    
    // We update currentRA and currentDEC based on where we synced last
    // (This requires storing the sync anchor point, which for simplicity here
    // we can just dynamically update the anchor when GOTO finishes).
  }
}
