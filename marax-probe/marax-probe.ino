#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <Wire.h>
#include <SoftwareSerial.h>
#include <PubSubClient.h>
#include <TimeLib.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include "values.h"

#define pumpState (status.csv[25] == '1')
#define backoffStart 1000
#define csvInitData "Z0.00,000,000,000,0000,0,0"
#define endMarker '\n'
#define numChars 64

const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 9 * 3600; // JST (UTC+9)の場合
const int daylightOffset_sec = 0; // 夏時間を使用しない場合

struct Status {
  char csv[numChars];
  unsigned long timer;
  unsigned long timeoutCnt;
  bool displayOn;
  bool changed;
};

struct weather {
  int temp;
  uint8_t* icon;
};

/*
const uint8_t sunny[]   ={ 0x91,0x42,0x18,0x3d,0xbc,0x18,0x42,0x89};
const uint8_t rain[]    ={ 0x55,0xaa,0x55,0xaa,0x55,0xaa,0x55,0xaa};
const uint8_t cloud[]   ={ 0x00,0x00,0x7e,0x81,0x89,0x99,0x71,0x0e};
const uint8_t showers[] ={ 0x15,0x2a,0x7e,0x81,0x81,0x91,0x71,0x0e};
const uint8_t snow[]    ={ 0xa5,0x42,0xa5,0x18,0x18,0xa5,0x42,0xa5};
const uint8_t storm[]   ={ 0x09,0x04,0x08,0x7e,0x81,0x91,0x71,0x0e};
*/

uint8_t icon00[] = {
        0b00000000,
        0b01000010,
        0b00100100,
        0b00011000,
        0b00011000,
        0b00100100,
        0b01000010,
        0b00000000,
};
uint8_t icon01[] = {
        0b00010000,
        0b01010010,
        0b00111100,
        0b00111111,
        0b11111100,
        0b00111100,
        0b01001010,
        0b00001000,
};
uint8_t icon02[] = {
        0b00010000,
        0b01010100,
        0b00111000,
        0b11111110,
        0b01000100,
        0b10000010,
        0b10000001,
        0b01111110,
};
uint8_t icon03[] = {
        0b00010000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00111000,
        0b01000100,
        0b10000010,
        0b10000001,
        0b01111110,
};
uint8_t icon04[] = {
        0b01100000,
        0b10011000,
        0b10000100,
        0b01111000,
        0b01000100,
        0b10000010,
        0b10000001,
        0b01111110,
};
uint8_t icon09[] = {
        0b00111100,
        0b01111110,
        0b11111111,
        0b11111111,
        0b10001001,
        0b00001000,
        0b00101000,
        0b00111000,
};
uint8_t icon10[] = {
        0b00101010,
        0b00011100,
        0b01111111,
        0b00011100,
        0b01111010,
        0b11111100,
        0b00010000,
        0b00110000,
};
uint8_t icon11[] = {
        0b00001111,
        0b00011110,
        0b00111100,
        0b01111111,
        0b11111110,
        0b00000100,
        0b00001000,
        0b00010000,
};
uint8_t icon13[] = {
        0b01010010,
        0b11010011,
        0b00111100,
        0b00100111,
        0b11100100,
        0b00111100,
        0b11001011,
        0b01001010,
};
uint8_t icon50[] = {
        0b11111111,
        0b00000000,
        0b11111111,
        0b00000000,
        0b11111111,
        0b00000000,
        0b11111111,
        0b00000000,
};

Adafruit_SSD1306 display(128, 64, &Wire, -1);
SoftwareSerial mySerial(D3, D4);
WiFiClient espClient;
PubSubClient client(espClient);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, ntpServer, gmtOffset_sec, daylightOffset_sec);


void publish(Status status) {
  char buf[60];
  sprintf(buf, "%s,%d,%d", status.csv, status.timer, status.timeoutCnt);
  client.publish(topic, buf);
  //Serial.println("published: " + String(buf));
}

void setup_wifi() {
  delay(10);
  String str;
  str = "\nconnect:" + String(ssid) + "\n";
  Serial.print(str);

  initDisplay();
  outputDisplay(str);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    outputDisplay(".");
  }
  str = "\nConnected!\nIP:" + String(WiFi.localIP().toString()) + "\n";
  Serial.print(str);
  initDisplay();
  outputDisplay(str);
  delay(3000);
}

void callback(char *topic, byte *payload, unsigned int length) {
  // 何もしない
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);

    if (client.connect(clientId.c_str(), mqtt_user, mqtt_pass)) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(9600);
  mySerial.begin(9600);

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.display();
  mySerial.write(0x11);

  setup_wifi();
  timeClient.begin();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  getWeather();
}


void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  Status status = getMachineInput();
  if (status.changed) {
    publish(status);
    updateDisplay(status);
  }
}


bool isNum(char *str, int size) {
  for (int i = 0; i < size; i++) {
    if (str[i] < '0' || str[i] > '9') {
      return false;
    }
  }
  return true;
}

bool validate(char *csv) {
  return strlen(csv) == 26 && csv[0] >= 'A' && csv[0] <= 'Z'
         && csv[5] == ',' && csv[9] == ',' && csv[13] == ',' && csv[17] == ','
         && csv[22] == ',' && csv[24] == ','
         && isNum(csv + 6, 3) && isNum(csv + 10, 3) && isNum(csv + 14, 3) && isNum(csv + 18, 4);
}

bool statusEq(struct Status *a, struct Status *b) {
  return strcmp(a->csv, b->csv) == 0
         && a->timer == b->timer
         && a->displayOn == b->displayOn
         && a->timeoutCnt == b->timeoutCnt;
}

Status getMachineInput() {
  static bool prevPumpState = false;
  static unsigned long serialUpdateMillis = 0;
  static unsigned long timerStartMillis = 0;
  static unsigned long backoff = backoffStart;
  static int prevTimerCount = 0;
  static Status status = { csvInitData, 0, 0, true, false };
  static Status prevStatus = { csvInitData, 0, 0, true, false };
  int rlen = 0;
  if (mySerial.available() > 0) {
    rlen = mySerial.readBytesUntil(endMarker, status.csv, numChars);
    if (rlen > 0) {
      status.csv[rlen - 1] = 0;
    }
  }
  if (rlen > 0 && validate(status.csv)) {
    // ok
    //Serial.println("ok csv: " + String(status.csv));
    serialUpdateMillis = millis();
    if (!prevPumpState && pumpState) {
      timerStartMillis = millis();
      digitalWrite(LED_BUILTIN, LOW);
      //Serial.println("Start pump");
    }
    if (prevPumpState && !pumpState) {
      digitalWrite(LED_BUILTIN, HIGH);
      prevPumpState = false;
      //Serial.println("Stop pump");
      prevTimerCount = (millis() - timerStartMillis) / 1000;
    }
    prevPumpState = pumpState;
    status.timeoutCnt = 0;
    backoff = backoffStart;
    status.displayOn = true;
  } else {
    // ng
    if (rlen) {
      Serial.println("ng csv: " + String(status.csv) + "; len csv: " + String(strlen(status.csv)));
    }
    memcpy(status.csv, prevStatus.csv, numChars);
  }
  int timerCount = (pumpState) ? ((millis() - timerStartMillis) / 1000) : prevTimerCount;
  status.timer = (timerCount > 99) ? 99 : timerCount;

  if (millis() - serialUpdateMillis > backoff) {
    serialUpdateMillis = millis();
    if (backoff < backoffStart * 20) {
      backoff = backoff << 1;
    }
    if (status.timeoutCnt < 99) {
      status.timeoutCnt++;
    }
    if (status.timeoutCnt > 10) {
      prevTimerCount = 0;
      status.displayOn = false;
      //Serial.println("Sleep");
    }
    if (status.timeoutCnt > 2) {
      status.csv[0] = 'E';
    }
    Serial.println("Serial.read() timeoutCnt: " + String(status.timeoutCnt));
    mySerial.write(0x11);
  }
  if (!statusEq(&status, &prevStatus)) {
    status.changed = true;
    prevStatus = status;
    prevStatus.changed = false;
  } else {
    status.changed = false;
  }
  return status;
}

String getFormattedDateTime(unsigned long unixTime) {
  char buffer[16];
  time_t time = (time_t)unixTime;
  strftime(buffer, sizeof(buffer), "%m/%d %H:%M", localtime(&time));
  return String(buffer);
}


void updateDisplay(Status status) {
  timeClient.update();
  String formattedTime = getFormattedDateTime(timeClient.getEpochTime());
  display.clearDisplay();
  if (status.displayOn) {
    char buf[64];
    // draw line
    display.drawLine(74, 8, 74, 63-8, SSD1306_WHITE);
    // draw time seconds
    display.setTextSize(4);
    display.setCursor(display.width() / 2 - 1 + 17, 20);
    sprintf(buf, "%02u", status.timer);
    display.print(buf);
    display.setTextColor(WHITE, BLACK);
    // draw machine state C/S
    char state = status.csv[0];
    if (state) {
      display.setTextSize(2);
      display.setCursor(1, 1);
      display.print((state == 'C') ? "C" : (state == 'V') ? "S" : "X");
    }

    int heatingMode = atoi(status.csv + 18);
    bool heatingState = status.csv[23] == '1';
    if (heatingMode == 0) {
      // not in boost heating mode
      // draw fill circle if heating on
      if (heatingState) {
        display.fillCircle(45-8, 7, 6, SSD1306_WHITE);
      } else {
        display.drawCircle(45-8, 7, 6, SSD1306_WHITE);
      }
    } else {
      // in boost heating mode
      // draw fill rectangle if heating on
      if (heatingState) {
        display.fillRect(39-8, 1, 12, 12, SSD1306_WHITE);
      } else {
        display.drawRect(39-8, 1, 12, 12, SSD1306_WHITE);
      }
    }
    // datetime
    display.setCursor(display.width() / 2-3 , 0);
    display.setTextSize(1);
    display.print(formattedTime);
    // weather
    weather w = getWeather();
    display.setCursor(display.width()-8*5, display.height() - 8);
    display.print(String(w.temp) + (char)247 +"C");
    display.drawBitmap(display.width() - 8, display.height() - 8, w.icon, 8, 8, WHITE);

    int currentTemp = atoi(status.csv + 14);
    if (currentTemp) {
      display.setTextSize(3);
      display.setCursor(1, 20);
      display.print(String(currentTemp));
      display.setTextSize(2);
      display.print((char)247);
      if (currentTemp < 100) {
        display.print("C");
      }
    }
    int steamTemp = atoi(status.csv + 6);
    if (steamTemp) {
      display.setTextSize(2);
      display.setCursor(1, 48);
      display.print(String(steamTemp));
      display.setTextSize(1);
      display.print((char)247);
      display.print("C");
    }
    display.invertDisplay(pumpState);
  } else {
    display.invertDisplay(false);
  }
  display.display();
}

void initDisplay() {
  display.clearDisplay();
  display.setCursor(0,0);
  display.setTextSize(1);
}
void outputDisplay(String str) {
  display.print(str);
  display.display();
}


weather getWeather() {
  static weather prevWeather = { 0, icon00  };
  static unsigned long updateMillis = 0;
  weather res = { 0, icon01 };
  if (updateMillis != 0 && millis() - updateMillis < 1000*60*60) {
    return prevWeather;
  }
  updateMillis = millis();
  HTTPClient http;
  http.begin(espClient, weather_url);
  int httpCode = http.GET();
  if (httpCode > 0) {
    String payload = http.getString();
    Serial.println("HTTP Response code: " + String(httpCode));
    Serial.println("Weather data:");
    Serial.println(payload);

    DynamicJsonDocument doc(1024);
    deserializeJson(doc, payload);
    const char* icon = doc["weather"][0]["icon"];
    float temperature = doc["main"]["temp"];
    res.temp = (int)temperature;
    Serial.println("icon: " + String(icon));
    Serial.println("temp: " + String(res.temp) + " °C");
    if (strstr(icon,"01")) {
      res.icon = icon01;
    }
    else if (strstr(icon,"02")){
      res.icon = icon02;
    }
    else if (strstr(icon,"03")){
      res.icon = icon03;
    }
    else if (strstr(icon,"04")){
      res.icon = icon04;
    }
    else if (strstr(icon,"09")){
      res.icon = icon09;
    }
    else if (strstr(icon,"10")){
      res.icon = icon10;
    }
    else if (strstr(icon,"11")){
      res.icon = icon11;
    }
    else if (strstr(icon,"13")){
      res.icon = icon13;
    }
    else if (strstr(icon,"50")){
      res.icon = icon50;
    }
    else {
      res.icon = icon00;
    }
  }
  else {
    Serial.println("Error on HTTP request");
  }
  http.end();
  prevWeather = res;
  return res;
}
