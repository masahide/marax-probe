#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SoftwareSerial.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET     -1
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// #define SERIAL_BAUDRATE 115200
#define SERIAL_BAUDRATE 9600
SoftwareSerial maraSerial(D3, D4); // RX (D3), TX (D4)

#define BUFFER_SIZE     64

//Mara Data
String maraData[7];
String* lastMaraData;

char buffer[BUFFER_SIZE];
int bufindex = 0;

void setup() {
  Serial.begin(SERIAL_BAUDRATE);
  maraSerial.begin(SERIAL_BAUDRATE); // ソフトウェアシリアルの開始

  /*if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
     Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }*/
  display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS);

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println(F("Startup."));
  display.setCursor(0, 30);
  display.println(F("read mara serial... test test1 test2"));
  display.display();

  memset(buffer, 0, BUFFER_SIZE);
}

static int i = 0;

void loop() {
  char endMarker = '\n';
  if (maraSerial.available() > 0) {
    char rcv = maraSerial.read();
    if (rcv != '\n')
      buffer[bufindex++] = rcv;
    else {
      buffer[bufindex] = 0;
      bufindex = 0;
      Serial.println(buffer);
        
      display.clearDisplay();
      display.setCursor(0, 0);

      String actual = String(i);
      if (actual.length()< 2)
        actual = "0" + actual;
      display.println(actual);

      display.println(buffer);
      display.display();
    }
    if (i == 99) {
      i = 0;
    } else {
      i++;
    }
  }
}