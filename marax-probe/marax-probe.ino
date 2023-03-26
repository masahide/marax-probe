#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>
#include <ESP8266WiFi.h>
#include <Wire.h>
#include <SoftwareSerial.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "values.h"


#define pumpState (status.csv[25] == '1')
#define backoffStart 500

//
const byte numChars = 64;

struct Status {
  char csv[numChars];
  unsigned long timer;
  unsigned long timeoutCnt;
  bool displayOn;
  bool changed;
};

unsigned long timerDisplayOffMillis = 0;

bool status_equal(struct Status *a, struct Status *b) {
  return strcmp(a->csv, b->csv) == 0
         && a->timer == b->timer
         && a->displayOn == b->displayOn
         && a->timeoutCnt == b->timeoutCnt;
  // return a->machineState == b->machineState && a->heatingState == b->heatingState && a->pumpState == b->pumpState && a->heatingMode == b->heatingMode && a->currentTemp == b->currentTemp && a->targetTemp  == b->targetTemp && a->steamTemp == b->steamTemp && a->timer == b->timer;
}

Adafruit_SSD1306 display(128, 64, &Wire, -1);
SoftwareSerial mySerial(D3, D4);

WiFiClient espClient;
PubSubClient client(espClient);

Status status;
Status prevStatus;


void publish(Status status) {
  char buf[60];
  sprintf(buf, "%s,%d,%d", status.csv, status.timer, status.timeoutCnt);
  client.publish(topic, buf);
  //Serial.println("published: " + String(buf));
}

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
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

  status.displayOn = true;
  status.timeoutCnt = 0;
  status.changed = false;
  strcpy(status.csv, "_0.00,000,000,000,0000,0,0");
  prevStatus = status;
  timerDisplayOffMillis = millis();

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.display();
  mySerial.write(0x11);

  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}


void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  getMachineInput(status);
  if (status.changed) {
    publish(status);
    status.changed = false;
    updateDisplay(status);
  }
}


void getMachineInput(Status &status) {
  static bool prevPumpState = false;
  static unsigned long serialUpdateMillis = 0;
  static unsigned long timerStartMillis = 0;
  static unsigned long backoff = backoffStart;
  static int prevTimerCount = 0;
  static byte ndx = 0;
  char endMarker1 = '\n';
  char endMarker2 = '\r';
  char rc;
  while (mySerial.available()) {
    serialUpdateMillis = millis();
    rc = mySerial.read();

    if (rc != endMarker1) {
      if (rc == endMarker2) {
        continue;
      }
      status.csv[ndx] = rc;
      ndx = (ndx < numChars - 1) ? ndx + 1 : numChars - 1;
    } else {
      status.csv[ndx] = 0;
      ndx = 0;

      //Serial.println("csv: " + String(status.csv));
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
    }
  }
  int timerCount = (pumpState) ? ((millis() - timerStartMillis) / 1000) : prevTimerCount;
  status.timer = (timerCount > 99) ? 99 : timerCount;
  if (millis() - serialUpdateMillis > backoff) {
    serialUpdateMillis = millis();
    if (backoff < 6000) {
      backoff = backoff << 1
    }
    if (status.timeoutCnt < 99) {
      status.timeoutCnt++;
    }
    if (status.timeoutCnt > 10) {
      prevTimerCount = 0;
      status.displayOn = false;
      //Serial.println("Sleep");
    }
    memcpy(status.csv, prevStatus.csv, numChars);
    if (status.timeoutCnt > 2) {
      status.csv[0] = 'E';
    }
    //Serial.println("Serial.read() timeoutCnt: " + String(status.timeoutCnt));
    mySerial.write(0x11);
  }
  // 現在のStatusと前回のStatusを比較し、変更がある場合のみchangedフラグをtrueに設定
  if (!status_equal(&status, &prevStatus)) {
    status.changed = true;
    prevStatus = status;
    prevStatus.changed = false;
  } else {
    status.changed = false;
  }
}



void updateDisplay(Status &status) {
  display.clearDisplay();
  if (status.displayOn) {
    char buf[64];
    // draw line
    display.drawLine(74, 0, 74, 63, SSD1306_WHITE);
    // draw time seconds
    display.setTextSize(4);
    display.setCursor(display.width() / 2 - 1 + 17, 20);
    sprintf(buf, "%02u", status.timer);
    display.print(buf);
    display.setTextColor(WHITE, BLACK);
    // draw machine state C/S
    char machineState = status.csv[0];
    if (machineState) {
      display.setTextSize(2);
      display.setCursor(1, 1);
      if (machineState == 'C') {
        display.print("C");
      } else if (machineState == 'V') {
        display.print("S");
      } else {
        display.print("X");
      }
    }

    int heatingMode = String(status.csv).substring(18, 22).toInt();
    bool heatingState = status.csv[23] == '1';
    if (heatingMode = 0) {
      // not in boost heating mode
      // draw fill circle if heating on
      if (heatingState) {
        display.fillCircle(45, 7, 6, SSD1306_WHITE);
      } else {
        display.drawCircle(45, 7, 6, SSD1306_WHITE);
      }
    } else {
      // in boost heating mode
      // draw fill rectangle if heating on
      if (heatingState) {
        display.fillRect(39, 1, 12, 12, SSD1306_WHITE);
      } else {
        display.drawRect(39, 1, 12, 12, SSD1306_WHITE);
      }
    }
    int currentTemp = String(status.csv).substring(14, 17).toInt();
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
    int steamTemp = String(status.csv).substring(6, 9).toInt();
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
