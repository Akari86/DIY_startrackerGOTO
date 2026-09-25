/*
 * DIY_startrackerGOTO
 * Module: MainBoard (Motor & Hardware Controller)
 * Version: 1.1 (Safety & UART Overhaul)
 */
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BNO055.h>
#include <AccelStepper.h>
#include "core/Config.h"

// Hardware instances
Adafruit_BNO055 bno = Adafruit_BNO055(55, 0x29, &Wire);
AccelStepper raMotor(AccelStepper::DRIVER, RA_STEP, RA_DIR);
AccelStepper decMotor(AccelStepper::DRIVER, DEC_STEP, DEC_DIR);

// State variables
TrackingMode currentMode = TRACK_STOPPED;
float targetSpeedRA = 0.0;
float targetSpeedDEC = 0.0;
bool bnoOK = false;

#include "core/Lx200Server.h"

// Button state tracking (for debouncing and edge detection)
struct Button {
  int pin;
  bool lastState;
  uint32_t lastChange;
  const char* name;
};
Button btns[] = {
  {BTN_UP, true, 0, "UP"},
  {BTN_DOWN, true, 0, "DOWN"},
  {BTN_LEFT, true, 0, "LEFT"},
  {BTN_RIGHT, true, 0, "RIGHT"},
  {BTN_CENTER, true, 0, "ENTER"}
};

// Task handle for the sensor/comms core
TaskHandle_t CommsTask;

// Function prototypes
void commsTaskCode(void * pvParameters);
void handleCydCommands();
void readAndSendBNO();
void checkButtons();

void setup() {
  Serial.begin(115200);
  Serial2.begin(CYD_BAUD, SERIAL_8N1, CYD_RX, CYD_TX);
  
  // Setup button pins
  for(int i=0; i<5; i++) {
    pinMode(btns[i].pin, INPUT_PULLUP);
  }
  
  // Setup Hall effect pins
  pinMode(RA_HALL, INPUT); // External pullup required if bare sensor
  pinMode(DEC_HALL, INPUT);
  
  // Motor Enable pins
  pinMode(RA_EN, OUTPUT);
  pinMode(DEC_EN, OUTPUT);
  digitalWrite(RA_EN, HIGH); // Disable initially (usually LOW is enable)
  digitalWrite(DEC_EN, HIGH);
  
  // Motor parameters
  raMotor.setMaxSpeed(SPEED_GOTO);
  raMotor.setAcceleration(500.0);
  decMotor.setMaxSpeed(SPEED_GOTO);
  decMotor.setAcceleration(500.0);
  
  // I2C for BNO055 with Auto-Detect Address
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(100000); // Enforce 100kHz Standard Mode
  Wire.setTimeOut(100);  // Increase timeout for Clock Stretching (BNO055 requires this)
  delay(800); // Wait 800ms for BNO055 to fully boot up before querying
  
  // I2C Scanner
  Serial.println("Scanning I2C bus...");
  uint8_t bnoAddress = 0x00;
  for (byte i = 1; i < 127; i++) {
    Wire.beginTransmission(i);
    if (Wire.endTransmission() == 0) {
      Serial.print("Found I2C device at 0x");
      Serial.println(i, HEX);
      if (i == 0x28 || i == 0x29) {
        bnoAddress = i;
      }
    }
  }

  if (bnoAddress == 0x00) {
    Serial.println("ERROR: No BNO055 detected! Check wiring, 3.3V, and GND.");
  } else {
    Serial.print("BNO055 detected at 0x"); Serial.println(bnoAddress, HEX);
    // Re-initialize bno object with correct address (pointer approach is cleaner, but here we just assign it)
    bno = Adafruit_BNO055(55, bnoAddress, &Wire);
    
    if(!bno.begin()) {
      Serial.println("BNO055 begin() failed despite being detected.");
    } else {
      bnoOK = true;
      bno.setExtCrystalUse(true);
      Serial.println("BNO055 initialized successfully.");
    }
  }
  
  // Initialize WiFi and LX200 Server
  setupWiFiAP();
  
  // Create task pinned to Core 0 for Sensors and Communication
  // Core 1 (default for loop()) will be dedicated entirely to stepper pulse generation
  xTaskCreatePinnedToCore(
      commsTaskCode,   /* Task function */
      "CommsTask",     /* Name of task */
      10000,           /* Stack size of task */
      NULL,            /* Parameter of the task */
      1,               /* Priority of the task */
      &CommsTask,      /* Task handle */
      0);              /* Pin task to core 0 */
      
  Serial.println("Main Board Initialized.");
}

// =======================================================
// CORE 1: Dedicated to High-Speed Stepper Motor execution
// =======================================================
void loop() {
  if (currentMode == TRACK_HOMING) {
    // Check Hall effect sensors (Assuming LOW means magnet is detected)
    bool raHome = (digitalRead(RA_HALL) == LOW);
    bool decHome = (digitalRead(DEC_HALL) == LOW);
    
    if (!raHome) raMotor.runSpeed();
    if (!decHome) decMotor.runSpeed();
    
    // Safety soft-limit: if we've spun more than a full revolution (approx 200,000 steps), abort!
    if (abs(raMotor.currentPosition()) > 200000 || abs(decMotor.currentPosition()) > 200000) {
      currentMode = TRACK_STOPPED;
      digitalWrite(RA_EN, HIGH);
      digitalWrite(DEC_EN, HIGH);
      Serial.println("ERROR: Homing failed (Sensor timeout/wire break). Motors aborted.");
      Serial2.println("H:ERR"); // Notify CYD of error
    }
    
    if (raHome && decHome) {
      // Both axes have reached home position
      raMotor.setCurrentPosition(0);
      decMotor.setCurrentPosition(0);
      currentMode = TRACK_STOPPED;
      digitalWrite(RA_EN, HIGH); // Disable motors
      digitalWrite(DEC_EN, HIGH);
      Serial.println("Homing complete! Position reset to 0.");
      Serial2.println("H:DONE"); // Notify CYD
    }
  }
  else if (currentMode != TRACK_STOPPED && currentMode != TRACK_GOTO) {
    raMotor.runSpeed();
    decMotor.runSpeed();
  }
  else if (currentMode == TRACK_GOTO) {
    raMotor.run();
    decMotor.run();
    if (raMotor.distanceToGo() == 0 && decMotor.distanceToGo() == 0) {
      currentMode = TRACK_STOPPED; 
    }
  } else {
    delay(1);
  }
}

// =======================================================
// CORE 0: Sensors, Serial, and WiFi processing
// =======================================================
void commsTaskCode(void * pvParameters) {
  uint32_t lastBNO = 0;
  
  for(;;) {
    processLx200();
    handleCydCommands();
    checkButtons();
    
    if(millis() - lastBNO > 100) {
      lastBNO = millis();
      readAndSendBNO();
    }
    
    vTaskDelay(10 / portTICK_PERIOD_MS); 
  }
}

void handleCydCommands() {
  static String cmdBuffer = "";
  while (Serial2.available()) {
    char c = Serial2.read();
    if (c == '\n' || c == '\r') {
      cmdBuffer.trim();
      if(cmdBuffer.length() > 0) {
        String line = cmdBuffer;
        cmdBuffer = ""; // Reset buffer
        
        if(line == "C:HOME") {
          currentMode = TRACK_HOMING;
          digitalWrite(RA_EN, LOW); // Enable motors
          digitalWrite(DEC_EN, LOW);
          raMotor.setCurrentPosition(0);
          decMotor.setCurrentPosition(0);
          raMotor.setSpeed(SPEED_HOMING);
          decMotor.setSpeed(SPEED_HOMING);
          Serial.println("Homing calibration started at high speed...");
        }
    else if(line == "C:CALIMU") {
      Serial.println("IMU Calibration requested (System reset/recal)");
      // Can't easily force BNO055 to recalibrate without power cycle, 
      // but we can acknowledge or reset offsets if we implement that.
    }
    else if(line.startsWith("M:")) {
      String mode = line.substring(2);
      if(mode == "SIDEREAL") {
        currentMode = TRACK_SIDEREAL;
        targetSpeedRA = SPEED_SIDEREAL;
        digitalWrite(RA_EN, LOW); // Enable driver
        raMotor.setSpeed(targetSpeedRA);
        Serial.println("Tracking: SIDEREAL");
      }
      else if(mode == "LUNAR") {
        currentMode = TRACK_LUNAR;
        targetSpeedRA = SPEED_LUNAR;
        digitalWrite(RA_EN, LOW);
        raMotor.setSpeed(targetSpeedRA);
        Serial.println("Tracking: LUNAR");
      }
      else if(mode == "SOLAR") {
        currentMode = TRACK_SOLAR;
        targetSpeedRA = SPEED_SOLAR;
        digitalWrite(RA_EN, LOW);
        raMotor.setSpeed(targetSpeedRA);
        Serial.println("Tracking: SOLAR");
      }
      else if(mode == "STOP") {
        currentMode = TRACK_STOPPED;
        targetSpeedRA = 0;
        digitalWrite(RA_EN, HIGH); // Disable driver to save power
        digitalWrite(DEC_EN, HIGH);
        Serial.println("Tracking: STOP");
      }
    }
    else if (line.startsWith("T:")) { // Target RA,DEC (T:ra,dec)
      int comma = line.indexOf(',');
      if(comma > 0) {
        targetRA = line.substring(2, comma).toDouble();
        targetDEC = line.substring(comma+1).toDouble();
        
        double deltaRA_Hours = targetRA - currentRA;
        double deltaDEC_Deg = targetDEC - currentDEC;
        
        if (deltaRA_Hours > 12.0) deltaRA_Hours -= 24.0;
        if (deltaRA_Hours < -12.0) deltaRA_Hours += 24.0;
        
        long stepsToMoveRA = (long)((deltaRA_Hours * 15.0) * STEPS_PER_DEGREE_RA);
        long stepsToMoveDEC = (long)(deltaDEC_Deg * STEPS_PER_DEGREE_DEC);
        
        digitalWrite(RA_EN, LOW); 
        digitalWrite(DEC_EN, LOW);
        raMotor.move(stepsToMoveRA);
        decMotor.move(stepsToMoveDEC);
        currentMode = TRACK_GOTO;
        Serial.printf("CYD GOTO: Moving RA %ld steps, DEC %ld steps\n", stepsToMoveRA, stepsToMoveDEC);
      }
    }
      } // end if length > 0
    } // end if \n
    else {
      cmdBuffer += c;
      if(cmdBuffer.length() > 64) cmdBuffer = ""; // Safety overflow prevent
    }
  } // end while
}

void readAndSendBNO() {
  if(!bnoOK) return;
  
  sensors_event_t event;
  bool success = bno.getEvent(&event);
  
  delay(5); // Prevent I2C bus flooding
  
  uint8_t sys = 0, gyro = 0, accel = 0, mag = 0;
  bno.getCalibration(&sys, &gyro, &accel, &mag);
  
  if(!success) return; // Skip sending if read failed
  
  // Format: O:az,pitch,roll,sys,gyro,acc,mag
  String out = "O:" + String(event.orientation.x, 2) + "," +
               String(event.orientation.y, 2) + "," +
               String(event.orientation.z, 2) + "," +
               String(sys) + "," + String(gyro) + "," + 
               String(accel) + "," + String(mag);
               
  Serial2.println(out);
  
  // Debug: Print to PC Serial Monitor every 1 second so we can verify it's working
  static uint32_t lastPrint = 0;
  if(millis() - lastPrint > 1000) {
    lastPrint = millis();
    Serial.print("Telemetry Sent: ");
    Serial.println(out);
  }
}

void checkButtons() {
  uint32_t now = millis();
  for(int i=0; i<5; i++) {
    bool state = digitalRead(btns[i].pin);
    // Button is pulled UP, so LOW means pressed
    if(state != btns[i].lastState && (now - btns[i].lastChange > 50)) { // 50ms debounce
      btns[i].lastState = state;
      btns[i].lastChange = now;
      
      if(state == LOW) { // On press down
        // Send button event to CYD
        Serial2.println(String("B:") + btns[i].name);
        Serial.println(String("Pressed: ") + btns[i].name);
      }
    }
  }
}
