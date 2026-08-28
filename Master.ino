// ============================================================
// ARC INDUCTION - MASTER (SENSOR) ARDUINO
// Reads LDR, gas sensor, temperature sensor.
// Drives servo + buzzer.
// Talks to the SLAVE (display/control) Arduino over I2C.
// Tinkercad link: https://www.tinkercad.com/things/54ooOXVzq9j-arc-electrical?sharecode=k2j0r8xqKekT9HRo8Ulcu10jjcbj_MqFu-DitpWaPls
// ============================================================

#include <Wire.h>
#include <Servo.h>


const int LDR_PIN    = A0;   
const int GAS_PIN     = A1;  
const int TEMP_PIN    = A2;  
const int SERVO_PIN   = 9;
const int BUZZER_PIN  = 8;


const uint8_t SLAVE_ADDR = 0x08;

const int GAS_ALERT_ON   = 180;   
const int GAS_ALERT_OFF  = 130;   
const float TEMP_EMERGENCY_C = 45.0;


const int BLACKOUT_DROP_THRESHOLD = 400;
const int BLACKOUT_RECOVER_MARGIN = 100;
float lightBaseline = 512;              
const float BASELINE_ALPHA = 0.02;      

enum State : uint8_t {
  STANDBY = 0,
  ACTIVE = 1,
  GAS_ALERT = 2,
  BLACKOUT = 3,
  TEMP_EMERGENCY = 4,
  MULTI_FAULT = 5
};

uint8_t currentState = STANDBY;
bool systemActivated = false;   
bool gasAlertLatched = false;   
bool blackoutLatched = false;   
uint8_t displayMode = 0;        

Servo ventServo;


enum IrCmd : uint8_t {
  CMD_NONE = 0,
  CMD_ACTIVATE = 1,
  CMD_TOGGLE = 2,
  CMD_RESET = 3
};

unsigned long lastLoopMs = 0;
const unsigned long LOOP_PERIOD_MS = 150; 

void setup() {
  Serial.begin(9600);
  Wire.begin(); 
  ventServo.attach(SERVO_PIN);
  ventServo.write(0);
  pinMode(BUZZER_PIN, OUTPUT);
  noTone(BUZZER_PIN);

 
  lightBaseline = analogRead(LDR_PIN);
}

void loop() {
  unsigned long now = millis();
  if (now - lastLoopMs < LOOP_PERIOD_MS) return;
  lastLoopMs = now;

  int lightRaw = analogRead(LDR_PIN);
  int gasRaw   = analogRead(GAS_PIN);      
  int tempRaw  = analogRead(TEMP_PIN);
  float tempC  = tmp36ToCelsius(tempRaw);

 
  uint8_t irCmd = requestIrCommand();

 
  if (gasRaw > GAS_ALERT_ON) gasAlertLatched = true;
  else if (gasRaw < GAS_ALERT_OFF) gasAlertLatched = false;

  bool suddenDrop = (lightBaseline - lightRaw) > BLACKOUT_DROP_THRESHOLD;
  if (suddenDrop) {
    blackoutLatched = true;
  } else if (blackoutLatched && lightRaw > (lightBaseline - BLACKOUT_RECOVER_MARGIN)) {
    blackoutLatched = false;
  }
 
  if (!blackoutLatched) {
    lightBaseline = lightBaseline + BASELINE_ALPHA * (lightRaw - lightBaseline);
  }


  bool inBlackout = blackoutLatched && currentState != TEMP_EMERGENCY;
  if (irCmd == CMD_ACTIVATE && !inBlackout) {
    systemActivated = true;
  }
  if (irCmd == CMD_TOGGLE && !inBlackout && currentState == ACTIVE) {
    displayMode = 1 - displayMode;
  }

  
  if (tempC > TEMP_EMERGENCY_C) {
    currentState = TEMP_EMERGENCY;
  } else if (currentState == TEMP_EMERGENCY) {
    
    if (irCmd == CMD_RESET) {
      currentState = resolveNonEmergencyState();
    }
    
  } else {
    currentState = resolveNonEmergencyState();
  }


  ventServo.write(currentState == TEMP_EMERGENCY ? 180 : 0);
  if (currentState == MULTI_FAULT) {
    tone(BUZZER_PIN, 2000); 
  } else {
    noTone(BUZZER_PIN);
  }

  
  int gasPurityPct = map(constrain(gasRaw, 0, 1023), 0, 1023, 100, 0);
  sendTelemetry(currentState, displayMode, lightRaw, gasPurityPct, (int)tempC);

  // ---- Debug ----
  Serial.print("state="); Serial.print(currentState);
  Serial.print(" light="); Serial.print(lightRaw);
  Serial.print(" gas="); Serial.print(gasRaw);
  Serial.print(" tempC="); Serial.print(tempC);
  Serial.print(" irCmd="); Serial.println(irCmd);
}

uint8_t resolveNonEmergencyState() {
  if (gasAlertLatched && blackoutLatched) return MULTI_FAULT;
  if (gasAlertLatched) return GAS_ALERT;
  if (blackoutLatched) return BLACKOUT;
  return systemActivated ? ACTIVE : STANDBY;
}

float tmp36ToCelsius(int raw) {
  float voltage = raw * 5.0 / 1024.0;
  return (voltage - 0.5) * 100.0;
}


uint8_t requestIrCommand() {
  uint8_t received = CMD_NONE;
  Wire.requestFrom((int)SLAVE_ADDR, 1);
  if (Wire.available()) {
    received = Wire.read();
  }
  return received;
}


void sendTelemetry(uint8_t state, uint8_t mode, int lightVal, int gasPct, int tempVal) {
  Wire.beginTransmission(SLAVE_ADDR);
  Wire.write(state);
  Wire.write(mode);
  Wire.write(highByte(lightVal));
  Wire.write(lowByte(lightVal));
  Wire.write(highByte(gasPct));
  Wire.write(lowByte(gasPct));
  Wire.write(highByte(tempVal));
  Wire.write(lowByte(tempVal));
  Wire.endTransmission();
}
