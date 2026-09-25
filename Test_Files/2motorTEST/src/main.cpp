#include <Arduino.h>
#include <AccelStepper.h>

// --- Pin Definitions สำหรับมอเตอร์ RA ---
#define RAEN_PIN 14    
#define RASTEP_PIN 4  
#define RADIR_PIN 13  
#define RAMS1_PIN 26  
#define RAMS2_PIN 27  

// --- Pin Definitions สำหรับมอเตอร์ DEC ---
#define DECEN_PIN 19   
#define DECSTEP_PIN 5 
#define DECDIR_PIN 18  
#define DECMS1_PIN 32  
#define DECMS2_PIN 33  

AccelStepper motorRA(AccelStepper::DRIVER, RASTEP_PIN, RADIR_PIN);
AccelStepper motorDEC(AccelStepper::DRIVER, DECSTEP_PIN, DECDIR_PIN);

void setup() {
  // ตั้งค่า Pin Mode
  pinMode(RAEN_PIN, OUTPUT);
  pinMode(RAMS1_PIN, OUTPUT);
  pinMode(RAMS2_PIN, OUTPUT);
  
  pinMode(DECEN_PIN, OUTPUT);
  pinMode(DECMS1_PIN, OUTPUT);
  pinMode(DECMS2_PIN, OUTPUT);

  // เปิดการทำงาน Driver
  digitalWrite(RAEN_PIN, LOW);
  digitalWrite(DECEN_PIN, LOW);

  // ⭐️ แก้ไข: ตั้งค่าเป็น Full Step (1/1) เพื่อให้ได้แรงบิด (Torque) 100% เต็ม
  // หรือถ้าหมุนแล้วสั่นเกินไป ให้เปลี่ยน MS1 เป็น HIGH, MS2 เป็น LOW (1/2 Step จะได้แรงบิด ~70%)
  digitalWrite(RAMS1_PIN, LOW);
  digitalWrite(RAMS2_PIN, LOW);
  
  digitalWrite(DECMS1_PIN, LOW);
  digitalWrite(DECMS2_PIN, LOW);

  // ===============================================
  // ⭐️ แก้ไข: จุดปรับจูนความเร็วและแรงบิดสำหรับโหลดหนัก
  // ===============================================
  // ธรรมชาติของ Stepper: ยิ่งหมุนเร็ว แรงบิดยิ่งตก 
  // ค่าเดิม 8000 เร็วเกินไปจนมอเตอร์ไม่มีแรงขับ Worm Gear
  
  motorRA.setMaxSpeed(1000.0);      // ลดความเร็วสูงสุดลงมาเพื่อให้มอเตอร์มีแรงบิด
  motorRA.setAcceleration(200.0);   // ลดความเร่งลง ค่อยๆ ออกตัว ป้องกันมอเตอร์หลุดสเตป (Stall)
  
  motorDEC.setMaxSpeed(1000.0);     
  motorDEC.setAcceleration(200.0); 

  // สั่งให้เริ่มหมุน
  motorRA.moveTo(300000);
  motorDEC.moveTo(-300000); 
}

void loop() {
  // สั่งให้มอเตอร์ทำงาน (ห้ามมีคำสั่ง delay() ในลูปนี้เด็ดขาด)
  motorRA.run();
  motorDEC.run();
}                                      