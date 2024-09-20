#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <avr/pgmspace.h>
#include <EEPROM.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32

#define REC_LENGTH 200

// Pins definition
#define ENCODER_PIN1 2
#define ENCODER_PIN2 4
#define PUSHBUTTON_PIN 5
#define OLED_RESET -1

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

const char vRangeName[1][5] PROGMEM = {" 5V"};  // Only 5V mode
const char *const vstring_table[] PROGMEM = {vRangeName[0]};
const char TimeName[8][6] PROGMEM = {" 10ms", "  7ms", "  5ms", "  2ms", "  1ms", "700us", "500us", "200us"};
const char *const tstring_table[] PROGMEM = {TimeName[0], TimeName[1], TimeName[2], TimeName[3], TimeName[4], TimeName[5], TimeName[6], TimeName[7]};

int waveBuff[REC_LENGTH];
char chrBuff[10];
String TimeScale = "xxxAs";
String vScale = "xxxx";

float lsb5V = 0.0055549;

volatile int vRange = 0;  // Default to 5V mode
volatile int Time;
volatile int trigD;
volatile int scopeP;
volatile boolean hold = false;
volatile boolean paraChanged = false;
volatile int saveTimer;
int timeExec;

int dataMin;
int dataMax;
int dataAve;
int rangeMax;
int rangeMin;
int rangeMaxDisp;
int rangeMinDisp;
int trigP;
boolean trigSync;
int att10x;

unsigned long buttonPressStartTime = 0;
const unsigned long holdActivationTime = 300;
unsigned long menuTimeout = 0;
bool menuVisible = true;

void setup() {
  pinMode(ENCODER_PIN1, INPUT_PULLUP);
  pinMode(ENCODER_PIN2, INPUT_PULLUP);
  pinMode(PUSHBUTTON_PIN, INPUT_PULLUP);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    for (;;);
  }
  display.clearDisplay();
  loadEEPROM();
  analogReference(INTERNAL);
  attachInterrupt(digitalPinToInterrupt(ENCODER_PIN1), pin2IRQ, FALLING);
  startScreen();
}

void loop() {
  digitalWrite(13, HIGH);
  if (!hold) {
    setConditions();
    readWave();
    dataAnalize();
    writeCommonImage();
    plotData();
    if (menuVisible) {
      dispInf();
    }
    display.display();
    saveEEPROM();
  } else {
    display.display();
  }
  digitalWrite(13, LOW);

  // Hold mode handling
  if (digitalRead(PUSHBUTTON_PIN) == LOW) {
    if (millis() - buttonPressStartTime >= holdActivationTime) {
      hold = true;
    }
  } else {
    if (hold) {
      hold = false;
    }
    buttonPressStartTime = millis(); // Reset the button press timer
  }

  // Menu timeout handling
  if (paraChanged) {
    menuTimeout = millis();
    menuVisible = true;
    paraChanged = false;
  } else if (millis() - menuTimeout > 2000) {
    menuVisible = false;
  }
}

void setConditions() {
  strcpy_P(chrBuff, (char *)pgm_read_word(&(tstring_table[Time])));
  TimeScale = chrBuff;

  strcpy_P(chrBuff, (char *)pgm_read_word(&(vstring_table[vRange])));
  vScale = chrBuff;

  // 5V mode settings
  rangeMax = 5 / lsb5V;
  rangeMaxDisp = 500;
  rangeMin = 0;
  rangeMinDisp = 0;
  att10x = 0;
}

void writeCommonImage() {
  display.clearDisplay();
  display.setTextColor(WHITE);
}

void readWave() {
  pinMode(12, INPUT); // 5V mode doesn't use attenuation

  switch (Time) {
    case 0: { // 10ms
        timeExec = 80 + 50;
        ADCSRA = (ADCSRA & 0xf8) | 0x07;
        for (int i = 0; i < REC_LENGTH; i++) {
          waveBuff[i] = analogRead(0);
          delayMicroseconds(288);
        }
        break;
      }
    case 1: { // 7ms
        timeExec = 56 + 50;
        ADCSRA = (ADCSRA & 0xf8) | 0x07;
        for (int i = 0; i < REC_LENGTH; i++) {
          waveBuff[i] = analogRead(0);
          delayMicroseconds(193);
        }
        break;
      }
    case 2: { // 5ms
        timeExec = 40 + 50;
        ADCSRA = (ADCSRA & 0xf8) | 0x07;
        for (int i = 0; i < REC_LENGTH; i++) {
          waveBuff[i] = analogRead(0);
          delayMicroseconds(138);
        }
        break;
      }
    case 3: { // 2ms
        timeExec = 16 + 50;
        ADCSRA = (ADCSRA & 0xf8) | 0x07;
        for (int i = 0; i < REC_LENGTH; i++) {
          waveBuff[i] = analogRead(0);
          delayMicroseconds(48);
        }
        break;
      }
    case 4: { // 1ms
        timeExec = 8 + 50;
        ADCSRA = (ADCSRA & 0xf8) | 0x07;
        for (int i = 0; i < REC_LENGTH; i++) {
          waveBuff[i] = analogRead(0);
          delayMicroseconds(18);
        }
        break;
      }
    case 5: { // 700us
        timeExec = 5.6 + 50;
        ADCSRA = (ADCSRA & 0xf8) | 0x07;
        for (int i = 0; i < REC_LENGTH; i++) {
          waveBuff[i] = analogRead(0);
          delayMicroseconds(11);
        }
        break;
      }
    case 6: { // 500us
        timeExec = 4 + 50;
        ADCSRA = (ADCSRA & 0xf8) | 0x07;
        for (int i = 0; i < REC_LENGTH; i++) {
          waveBuff[i] = analogRead(0);
          delayMicroseconds(8);
        }
        break;
      }
    case 7: { // 200us
        timeExec = 1 + 50;
        ADCSRA = (ADCSRA & 0xf8) | 0x07;
        for (int i = 0; i < REC_LENGTH; i++) {
          waveBuff[i] = analogRead(0);
          delayMicroseconds(4);
        }
        break;
      }
  }
}

void dataAnalize() {
  if (trigSync == true) {
    trigD = trigP;
  }

  if (trigSync == false) {
    int yMax = 0;
    int yMin = 1023;
    int ySum = 0;

    for (int i = 0; i < REC_LENGTH; i++) {
      ySum += waveBuff[i];
      if (waveBuff[i] > yMax) {
        yMax = waveBuff[i];
      }

      if (waveBuff[i] < yMin) {
        yMin = waveBuff[i];
      }
    }

    dataAve = ySum / REC_LENGTH;
    dataMax = yMax;
    dataMin = yMin;

    if (dataMax - dataAve > dataAve - dataMin) {
      trigP = REC_LENGTH - 1;
    }

    if (dataMax - dataAve < dataAve - dataMin) {
      trigP = 0;
    }

    trigSync = true;
  }
}

void plotData() {
  int tempY1;
  int tempY2;
  int tempX;

  if (trigD + 61 > REC_LENGTH) {
    trigD = REC_LENGTH - 61;
  }

  if (trigD < 61) {
    trigD = 61;
  }

  tempX = 0;

  // Draw waveform from left edge to right edge of the screen
  for (int i = trigD - 61; i < trigD + 61 - 1; i++) {
    if (i >= REC_LENGTH - 1) break; // Ensure we do not exceed the buffer length
    tempY1 = map(waveBuff[i], rangeMin, rangeMax, 31, 0);
    tempY2 = map(waveBuff[i + 1], rangeMin, rangeMax, 31, 0);
    display.drawLine(tempX, tempY1, tempX + 1, tempY2, WHITE);
    tempX++;
    if (tempX >= SCREEN_WIDTH) break; // Ensure we do not draw beyond the screen width
  }
}

void dispInf() {
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.print(TimeScale);
  //display.print(F(" | "));
  //display.print(vScale);
}

void loadEEPROM() {
  vRange = EEPROM.read(0);

  if (vRange != 0) {
    vRange = 0;  // Force to 5V mode
  }

  Time = EEPROM.read(1);

  if (Time > 7) {
    Time = 0;
  }

  trigD = EEPROM.read(2);
  scopeP = EEPROM.read(3);
}

void saveEEPROM() {
  if (saveTimer >= 5000) {
    saveTimer = 0;
    EEPROM.write(0, vRange);
    EEPROM.write(1, Time);
    EEPROM.write(2, trigD);
    EEPROM.write(3, scopeP);
  }
}

void startScreen() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println(F("SPCTRL &  WGDMODULAR"));
  display.display();
  delay(2000);
  display.clearDisplay();
  display.display();
  //}

  //--------------------------------------------------------------------------------------------------------------
  //BITMAP ANIMATION SPECTRAL & WGD
  animateBitmap();
}

const unsigned char NaN [] PROGMEM = {
  // 'Design ohne Titel (6), 128x32px
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x80, 0x80, 0x60, 0x00, 
  0x00, 0x00, 0x7b, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0xc0, 0xc0, 0xe0, 0x00, 
  0x00, 0x01, 0x80, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0xc0, 0xc0, 0xe0, 0x00, 
  0x00, 0x03, 0x00, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0xc1, 0xe0, 0xe0, 0x00, 
  0x00, 0x02, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe1, 0xe1, 0xc0, 0x00, 
  0x00, 0x04, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe1, 0xe1, 0xc0, 0x00, 
  0x00, 0x0c, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x73, 0xf3, 0x80, 0x00, 
  0x00, 0x08, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x73, 0xf3, 0x80, 0x00, 
  0x00, 0x08, 0x00, 0x42, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x73, 0x33, 0x80, 0x00, 
  0x00, 0x08, 0xf0, 0xe2, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0x3f, 0x00, 0x00, 
  0x00, 0x08, 0xf1, 0xe2, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0x3f, 0x00, 0x00, 
  0x00, 0x08, 0x60, 0xc2, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1e, 0x1e, 0x00, 0x00, 
  0x00, 0x08, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1e, 0x1e, 0x00, 0x00, 
  0x00, 0x08, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1e, 0x1e, 0x00, 0x00, 
  0x00, 0x08, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1e, 0x1e, 0x00, 0x00, 
  0x00, 0x08, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1e, 0x1e, 0x00, 0x00, 
  0x00, 0x08, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 0x1f, 0x00, 0x00, 
  0x00, 0x08, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0x3f, 0x00, 0x00, 
  0x00, 0x08, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0x3b, 0x80, 0x00, 
  0x00, 0x08, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x73, 0xb3, 0x80, 0x00, 
  0x00, 0x08, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x73, 0xf3, 0x80, 0x00, 
  0x00, 0x08, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x71, 0xf1, 0xc0, 0x00, 
  0x00, 0x08, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe1, 0xe1, 0xc0, 0x00, 
  0x00, 0x08, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe1, 0xe0, 0xe0, 0x00, 
  0x00, 0x0f, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0xc0, 0xe0, 0xe0, 0x00, 
  0x00, 0x00, 0x80, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0xc0, 0xc0, 0xe0, 0x00, 
  0x00, 0x00, 0xdf, 0x7c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0xc0, 0xc0, 0x70, 0x00, 
  0x00, 0x00, 0x70, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

//-------------------------------------------------------------------------------------------------------


void pin2IRQ() {
  if (digitalRead(ENCODER_PIN2) == LOW) {
    Time--;
    if (Time < 0) {
      Time = 7; // Wrap around to the last item
    }
  } else {
    Time++;
    if (Time > 7) {
      Time = 0; // Wrap around to the first item
    }
  }

  paraChanged = true;
}

//------------------------------------------------------------------------------------------------------
//BITMAP ANIMATION
void animateBitmap() {
  for (int x = SCREEN_WIDTH; x >= -128; x--) { // Move from right to left
    display.clearDisplay();
    display.drawBitmap(x, 0, NaN, 128, 32, SSD1306_WHITE);
    display.display();
    delay(0.1); // Adjust the delay for animation speed
  }
}