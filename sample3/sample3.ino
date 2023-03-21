#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>
#include <ESP8266WiFi.h>
#include <Wire.h>
#include <SoftwareSerial.h>


const byte numChars = 32;
bool displayOn = true;
int prevTimerCount = 0;
bool timerStarted = false;
unsigned long timerStartMillis = 0;
char receivedChars[numChars];

Adafruit_SSD1306 display(128, 64, &Wire, -1);
SoftwareSerial mySerial(D3, D4);

void setup() {
    WiFi.mode(WIFI_OFF);

    Serial.begin(9600);
    mySerial.begin(9600);

    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);

    memset(receivedChars, 0, numChars );

    display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
    display.clearDisplay();
    display.setTextColor(WHITE);
    display.display();
    mySerial.write(0x11);
}

void loop() {
    getMachineInput();
    detectChanges();
    updateDisplay();
}

void getMachineInput() {
    static unsigned long serialUpdateMillis = 0;
    static byte ndx = 0;
    char endMarker = '\n';
    char rc;
    while (mySerial.available() ) {
        serialUpdateMillis = millis();
        rc = mySerial.read();

        if (rc != endMarker) {
            receivedChars[ndx] = rc;
            ndx++;
            if (ndx >= numChars) {
                ndx = numChars - 1;
            }
        } else {
            receivedChars[ndx] = '\0';
            ndx = 0;
            Serial.println(receivedChars);
        }
    }
    if (millis() - serialUpdateMillis > 5000) {
        serialUpdateMillis = millis();
        memset(receivedChars, 0, numChars);
        Serial.println("Request serial update");
        mySerial.write(0x11);
    }
}


void detectChanges() {
    static unsigned long timerDisplayOffMillis = millis();
    static unsigned long timerStopMillis = 0;

    if (!timerStarted && receivedChars[25] == '1') {
        timerStartMillis = millis();
        timerStarted = true;
        displayOn = true;
        digitalWrite(LED_BUILTIN, LOW);
        timerDisplayOffMillis = millis();
        Serial.println("Start pump");
    }
    if (timerStarted && receivedChars[25] == '0') {
        digitalWrite(LED_BUILTIN, HIGH);
        if (timerStopMillis == 0) {
            timerStopMillis = millis();
        }
        if (millis() - timerStopMillis > 500) {
            timerStarted = false;
            timerStopMillis = 0;
            timerDisplayOffMillis = millis();
            Serial.println("Stop pump");
            prevTimerCount = (millis() - timerStartMillis) / 1000;
        }
    } else {
        timerStopMillis = 0;
    }
    if (!timerStarted && displayOn && timerDisplayOffMillis >= 0 && (millis() - timerDisplayOffMillis > 1000 * 60 * 60)) {
        timerDisplayOffMillis = 0;
        prevTimerCount = 0;
        displayOn = false;
        Serial.println("Sleep");
    }
}

String getTimer() {
    char outMin[3];
    int timerCount;

    if (timerStarted) {
        timerCount = (millis() - timerStartMillis ) / 1000;
    } else {
        timerCount = prevTimerCount;
    }

    if (timerCount > 99) {
        return "99";
    }
    sprintf(outMin, "%02u", timerCount);
    return outMin;
}

void updateDisplay() {
    display.clearDisplay();
    if (displayOn) {
        // draw line
        display.drawLine(74, 0, 74, 63, SSD1306_WHITE);
        // draw time seconds
        display.setTextSize(4);
        display.setCursor(display.width() / 2 - 1 + 17, 20);
        display.print(getTimer());
        display.setTextColor(WHITE, BLACK);
        // draw machine state C/S
        if (receivedChars[0]) {
            display.setTextSize(2);
            display.setCursor(1, 1);
            if (String(receivedChars[0]) == "C") {
                display.print("C");
            } else if (String(receivedChars[0]) == "V") {
                display.print("S");
            } else {
                display.print("X");
            }
        }
        if (String(receivedChars).substring(18, 22) == "0000") {
            // not in boost heating mode
            // draw fill circle if heating on
            if (String(receivedChars[23]) == "1") {
                display.fillCircle(45, 7, 6, SSD1306_WHITE);
            }
            // draw empty circle if heating off
            if (String(receivedChars[23]) == "0") {
                display.drawCircle(45, 7, 6, SSD1306_WHITE);
            }
        } else {
            // in boost heating mode
            // draw fill rectangle if heating on
            if (String(receivedChars[23]) == "1") {
                display.fillRect(39, 1, 12, 12, SSD1306_WHITE);
            }
            // draw empty rectangle if heating off
            if (String(receivedChars[23]) == "0") {
                display.drawRect(39, 1, 12, 12, SSD1306_WHITE);
            }
        }
        // draw temperature
        if (receivedChars[14] && receivedChars[15] && receivedChars[16]) {
            display.setTextSize(3);
            display.setCursor(1, 20);
            if (String(receivedChars[14]) != "0") {
                display.print(String(receivedChars[14]));
            }
            display.print(String(receivedChars[15]));
            display.print(String(receivedChars[16]));
            display.setTextSize(2);
            display.print((char)247);
            if (String(receivedChars[14]) == "0") {
                display.print("C");
            }
        }
        // draw steam temperature
        if (receivedChars[6] && receivedChars[7] && receivedChars[8]) {
            display.setTextSize(2);
            display.setCursor(1, 48);
            if (String(receivedChars[6]) != "0") {
                display.print(String(receivedChars[6]));
            }
            display.print(String(receivedChars[7]));
            display.print(String(receivedChars[8]));
            display.setTextSize(1);
            display.print((char)247);
            display.print("C");
        }
        display.invertDisplay(timerStarted);
    }
    display.display();
}
