// ============================================================
// ARC INDUCTION - SLAVE (DISPLAY/CONTROL) ARDUINO
// Drives the I2C LCD, reads the IR remote.
// Acts as an I2C SLAVE to the master (receives telemetry),
// and as an I2C MASTER to the LCD (address 0x27).
// Tinkercad link: https://www.tinkercad.com/things/54ooOXVzq9j-arc-electrical?sharecode=k2j0r8xqKekT9HRo8Ulcu10jjcbj_MqFu-DitpWaPls
// ============================================================

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <IRremote.h>


const int IR_RECV_PIN = 12;


const uint8_t MY_ADDR = 0x08;
LiquidCrystal_I2C lcd(0x27, 16, 2);


const unsigned long IR_ACTIVATE = 0xEF10BF00UL; // button "1"  -> enter Active Monitoring
const unsigned long IR_TOGGLE   = 0xEE11BF00UL; // button "2"  -> toggle LCD display mode
const unsigned long IR_RESET    = 0xED12BF00UL; // button "3"  -> manual reset out of Temp Emergency


volatile uint8_t pendingCmd = 0; // 0=none, 1=activate, 2=toggle, 3=reset

volatile bool newTelemetry = false;
volatile uint8_t rxState = 0, rxMode = 0;
volatile int rxLight = 0, rxGasPct = 0, rxTemp = 0;


uint8_t shownState = 255; 
uint8_t shownMode = 255;
int shownLight = -1, shownGasPct = -1, shownTemp = -1000;

void setup() {
  Serial.begin(9600);

  Wire.begin(MY_ADDR);
  Wire.onReceive(onReceiveEvent);
  Wire.onRequest(onRequestEvent);

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("AWAITING RITUAL");

  IrReceiver.begin(IR_RECV_PIN, ENABLE_LED_FEEDBACK);
}

void loop() {
  readIr();

  if (newTelemetry) {
   
    noInterrupts();
    uint8_t state = rxState, mode = rxMode;
    int light = rxLight, gasPct = rxGasPct, temp = rxTemp;
    newTelemetry = false;
    interrupts();

    updateLcd(state, mode, light, gasPct, temp);
  }
}

void readIr() {
  if (IrReceiver.decode()) {
    unsigned long code = IrReceiver.decodedIRData.decodedRawData;
    Serial.print("IR code: 0x");
    Serial.println(code, HEX);

    if (code == IR_ACTIVATE) pendingCmd = 1;
    else if (code == IR_TOGGLE) pendingCmd = 2;
    else if (code == IR_RESET) pendingCmd = 3;
   

    IrReceiver.resume();
  }
}


void updateLcd(uint8_t state, uint8_t mode, int light, int gasPct, int temp) {
  bool stateChanged = (state != shownState);
  bool contentChanged = (mode != shownMode) || (light != shownLight) ||
                         (gasPct != shownGasPct) || (temp != shownTemp);
  if (!stateChanged && !contentChanged) return;

  shownState = state; shownMode = mode;
  shownLight = light; shownGasPct = gasPct; shownTemp = temp;

  lcd.clear();
  lcd.setCursor(0, 0);

  switch (state) {
    case 0: 
      lcd.print("AWAITING RITUAL");
      break;
    case 1: 
      if (mode == 0) {
        lcd.print("LIGHT LEVEL");
        lcd.setCursor(0, 1);
        lcd.print(light);
      } else {
        lcd.print("AIR PURITY");
        lcd.setCursor(0, 1);
        lcd.print(gasPct);
        lcd.print("%");
      }
      break;
    case 2: 
      lcd.print("TOXIC PURGE");
      break;
    case 3:
      lcd.print("NOCTIS PROTOCOL");
      break;
    case 4: 
      lcd.print("COOKED");
      break;
    case 5: 
      lcd.print("MULTIPLE PROBLEMS");
      lcd.setCursor(0, 1);
      lcd.print("DETECTED");
      break;
  }
}



void onReceiveEvent(int numBytes) {
  if (numBytes < 8) {
    while (Wire.available()) Wire.read(); 
    return;
  }
  rxState  = Wire.read();
  rxMode   = Wire.read();
  int lHi = Wire.read(), lLo = Wire.read();
  int gHi = Wire.read(), gLo = Wire.read();
  int tHi = Wire.read(), tLo = Wire.read();
  rxLight  = (lHi << 8) | lLo;
  rxGasPct = (gHi << 8) | gLo;
  rxTemp   = (tHi << 8) | tLo;
  newTelemetry = true;
}

void onRequestEvent() {
  Wire.write(pendingCmd);
  pendingCmd = 0; 
}