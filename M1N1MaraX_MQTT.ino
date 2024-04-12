
/* Copyright (C) 2024 Ralf Grafe
This file is partly based on MaraX-Shot-Monitor <https://github.com/Anlieger/MaraX-Shot-Monitor>.

M1N1MaraX_Web is a free software you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

M1N1MaraX_Web is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

//Includes
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SoftwareSerial.h>
#include <Timer.h>
#include <Event.h>
#include "bitmaps.h"
#include <ESP8266WiFi.h>
#include <ArduinoHttpClient.h>
#include "secrets.h"
#include <PubSubClient.h>

//Definessss
#define SCREEN_WIDTH 128  //Width in px
#define SCREEN_HEIGHT 64  // Height in px
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C  // or 0x3D Check datasheet or Oled Display
#define BUFFER_SIZE 32

#define D5 (14)             // D5 is Rx Pin
#define D6 (12)             // D6 is Tx Pin
#define INVERSE_LOGIC true  // Use inverse logic for MaraX V2


#define DEBUG false

//Internals
int state = LOW;
char on = LOW;
char off = HIGH;

long timerStartMillis = 0;
long timerStopMillis = 0;
long timerDisplayOffMillis = 0;
int timerCount = 0;
bool timerStarted = false;
bool displayOn = false;

int prevTimerCount = 0;
long serialTimeout = 0;
char buffer[BUFFER_SIZE];
int bufferIndex = 0;
int isMaraOff = 0;
long lastToggleTime = 0;
int HeatDisplayToggle = 0;
int pumpInValue = 0;
const int Sim = 0;
int tt = 8;

//Secrets from secrets.h
//Will not be pushed into the repo you have to create the file by yourself
String ssid = WLAN_SSID;
String pass = WLAN_PASS;
const String mqttserver = MQTT_SERVER;
int mqttport = MQTT_PORT;
int mqttupint = MQTT_UPDATE_INTERVAL * 1000;  // MQTT Update Interval from secrets.h


WiFiClient WIFI_CLIENT;
PubSubClient MQTT_CLIENT;
//Names for the MQTT Server
char softwareVersion[32];
char targetSteamTemp[32];
char steamTemp[32];
char heatExchangeTemp[32];
char heatingMode[32];
char heatingElement[32];
char pump[32];

//Space for the serial Port 
const byte numChars = 32;
char receivedChars[numChars];
static byte ndx = 0;
char rc;
char mqttservbuf[15];
long lastMsg = 0;
char msg[50];


//Mara Data
String maraData[7];

//Instances

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
SoftwareSerial mySerial(D5, D6, INVERSE_LOGIC);  // Rx, Tx, Inverse_Logic
Timer t;

void setup() {
  display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS);
  display.clearDisplay();
  display.display();
  Serial.begin(9600);
  mySerial.begin(9600);
  //mySerial.write(0x11);  // this is XON Flow Control Chr ... do not use. 
  WiFi.begin(ssid, pass);
  WiFi.hostname("MaraX");
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
    }

  pinMode(LED_BUILTIN, OUTPUT);
  delay(1000);
  //
  t.every(1000, updateView);
}

// -----------------------------------------------------------
//
void getMaraData() {
  /*
    Example Data: C1.12,116,124,093,0840,1,05\n every ~400-500ms
    Length: 27
    [Pos] [Data] [Describtion]
    0)      C     Coffee Mode (C) or SteamMode (V) // "+" in case of Mara X V2 Steam Mode
    -       1.22  Software Version
    1)      116   current steam temperature (Celsisus)
    2)      124   target steam temperature (Celsisus)
    3)      093   current hx temperature (Celsisus)
    4)      0840  countdown for 'boost-mode'
    5)      1     heating element on or off
    6)      0     pump on or off
  */
  //
  while (mySerial.available()) {    // true, as long there are chrs in the Rx Buffer
    isMaraOff = 0;                  // Mara is not off
    serialTimeout = millis();       // save current time
    char rcv = mySerial.read();     // read next chr
    if (rcv != '\n')                // test if not CR
      buffer[bufferIndex++] = rcv;  // add to buffer and increase counter
    else {                          // CR received = EOM
      bufferIndex = 0;              // set buffer index to 0
      Serial.println(buffer);       // print buffer on serial monitor
      char* ptr = strtok(buffer, ",");    // Split String into Tokens with ',' as delimiter
      int idx = 0;
      while (ptr != NULL) {
        maraData[idx++] = String(ptr);
        ptr = strtok(NULL, ",");
      }
    }
  }
  //
  //
  if (millis() - serialTimeout > 999) {  // are there 1000ms passed after last chr rexeived?
    isMaraOff = 1;                       // Mara is off 
    if (DEBUG == true) {
      Serial.println("No Rx");  // Inserted for debugging
    }
    serialTimeout = millis();
    mySerial.write(0x11);
  }
}
// -----------------------------------------------------------
//Get the Machine Input
void getMachineInput() {
  while (mySerial.available() ) {
    rc = mySerial.read();

    if (rc != '\n') {                         // look for CR
      receivedChars[ndx] = rc;
      ndx++;
      if (ndx >= numChars) {                  // ndx counter > numChars = 32
        ndx = numChars - 1;
      }
    } else {
      receivedChars[ndx] = '\0';              // add zero as end of received Chars
      ndx = 0;
    }
  }
  // Split the Machine Input for the MQTT
strcpy(softwareVersion,receivedChars);
softwareVersion[5] = '\0';                      // first 5 chr & zero
strcpy(steamTemp,receivedChars+6);
steamTemp[3] = '\0';                            // 3 Chr & zero starting at position 6
strcpy(targetSteamTemp,receivedChars+10);
targetSteamTemp[3] = '\0';                      // 3 Chr & zero starting at position 10
strcpy(heatExchangeTemp,receivedChars+14);
heatExchangeTemp[3] = '\0';
strcpy(heatingMode,receivedChars+18);
heatingMode[4] = '\0';
strcpy(heatingElement,receivedChars+23);
heatingElement[1] = '\0';
strcpy(pump,receivedChars+25);
pump[1] = '\0';

}
// -----------------------------------------------------------
// This function connects to the MQTT broker
void reconnect() {
  // Set our MQTT broker address and port
  mqttserver.toCharArray(mqttservbuf,15);
  MQTT_CLIENT.setServer(mqttservbuf, mqttport);
  MQTT_CLIENT.setClient(WIFI_CLIENT);

  while (!MQTT_CLIENT.connected()) {
    Serial.println("Connect to MQTT broker");
    MQTT_CLIENT.connect("Kaffeemaschine_MQTT");

    // Wait some time to space out connection requests
    delay(500);
  }

  Serial.println("MQTT connected");
}
// -----------------------------------------------------------
void publishMQTT(){
  if (!MQTT_CLIENT.connected()) {
 //    If we're not, attempt to reconnect
    reconnect();
  }
  long now = millis();
  if (now - lastMsg > mqttupint) {            // Check MQTT Timer Interval
     lastMsg = now;
       //publish sensor data to MQTT broker
       //
       // Mode & Software Version (5+2 Chr)
      maraData[0].toCharArray(softwareVersion,7);
      //strncat(softwareVersion, "\0",2);                                         // append 2 chr \n to CharArray
      MQTT_CLIENT.publish("fhem/MaraX/softwareVersion", softwareVersion);
      //
      // Steam Temp (3+2 Chr)
      maraData[1].toCharArray(steamTemp,5);
      //strncat(steamTemp, "\0",2);
      MQTT_CLIENT.publish("fhem/MaraX/steamTemp", steamTemp);
      //
      // Target Steam Temp (3+2 Chr)
      maraData[2].toCharArray(targetSteamTemp,5);
      //strncat(targetSteamTemp, "\0",2);
      MQTT_CLIENT.publish("fhem/MaraX/targetSteamTemp", targetSteamTemp);
      //
      // Water Temperature (3+2 Chr)
      maraData[3].toCharArray(heatExchangeTemp,5);
      //strncat(heatExchangeTemp, "\0",2);
      MQTT_CLIENT.publish("fhem/MaraX/heatExchangeTemp", heatExchangeTemp); 
      //
      // Boost Mode Counter (4+2 Chr)
      maraData[4].toCharArray(heatingMode,6);
      MQTT_CLIENT.publish("fhem/MaraX/heatingMode", heatingMode);
      //
      // Heater Status (1+2 Chr)
      maraData[5].toCharArray(heatingElement,3);
      MQTT_CLIENT.publish("fhem/MaraX/heatingElement", heatingElement);
      //
      // Pump Status (1+2 Chr)
      maraData[6].toCharArray(pump,3);
      MQTT_CLIENT.publish("fhem/MaraX/pump", pump);
      //
      //
      Serial.println("MQTT Data published");
     }
   }
// -----------------------------------------------------------
void detectChanges() {
//
  if (maraData[6].toInt() == 1) {       // [6] == 1 is Flag for Pump ON
    if (!timerStarted) {                // Timer is not started
      timerStartMillis = millis();      // Save current time
      timerStarted = true;              // Save that Timer was started
      displayOn = true;                 //
      Serial.println("Start pump");     // Message for Serial Monitor
    }
  }
  //
  if (maraData[6].toInt() == 0) {       // [6] == 0 is Flag for Pump OFF
   if (timerStarted) {                  // Check if timer was started
      if (timerStopMillis == 0) {       //
        timerStopMillis = millis();     //
      }
      if (millis() - timerStopMillis > 500) {
        timerStarted = false;
        timerStopMillis = 0;
        timerDisplayOffMillis = millis();
        display.invertDisplay(false);
        Serial.println("Stop pump");
        tt = 8;

        delay(4000);
      }
    }
  } else {
    timerStopMillis = 0;
  }
}

String getTimer() {
  char outMin[2];
  if (timerStarted) {
    timerCount = (millis() - timerStartMillis) / 1000;
    if (timerCount > 4) {
      prevTimerCount = timerCount;
    }
  } else {
    timerCount = prevTimerCount;
  }
  if (timerCount > 99) {
    return "99";
  }
  sprintf(outMin, "%02u", timerCount);
  return outMin;
}
// -----------------------------------------------------------
void updateView() {
  //
  publishMQTT();
  //
  if (DEBUG == true) {
    Serial.println(serialTimeout);  // Inserted for debugging
  }
  display.clearDisplay();
  display.setTextColor(WHITE);

  if (isMaraOff == 1) {               // Mara OFF Message
    if (DEBUG == true) {
      Serial.println("Mara is off");  // Inserted for debugging
    }
    display.setCursor(30, 6);
    display.setTextSize(2);
    display.print("MARA X");
    display.setCursor(30, 28);
    display.setTextSize(4);
    display.print("OFF");
  } else {                                // Mara is NOT off
    if (timerStarted) {                   // If timer was started
      // draw the timer on the right
      display.fillRect(60, 9, 63, 55, BLACK);
      display.setTextSize(5);
      display.setCursor(68, 20);
      display.print(getTimer());

      if (timerCount >= 20 && timerCount <= 24) { // Message for 20 - 24 sec
        display.setTextSize(5);
        display.setCursor(68, 20);
        display.print(getTimer());

        //display.setTextSize(1);
        //display.setCursor(38, 2);
        // display.print("Get ready");
      }
      if (timerCount > 24) {
        display.setTextSize(5);
        display.setCursor(68, 20);
        display.print(getTimer());

        // display.setTextSize(1);
        // display.setCursor(35, 2);
        // display.print("You missed");
      }

      if (tt >= 1 && timerCount <= 23) {
        if (tt == 8) {
          display.drawBitmap(17, 14, coffeeCup30_01, 30, 30, WHITE);
          Serial.println(tt);
        } else if (tt == 7) {
          display.drawBitmap(17, 14, coffeeCup30_02, 30, 30, WHITE);
          Serial.println(tt);
        } else if (tt == 6) {
          display.drawBitmap(17, 14, coffeeCup30_03, 30, 30, WHITE);
          Serial.println(tt);
        } else if (tt == 5) {
          display.drawBitmap(17, 14, coffeeCup30_04, 30, 30, WHITE);
          Serial.println(tt);
        } else if (tt == 4) {
          display.drawBitmap(17, 14, coffeeCup30_05, 30, 30, WHITE);
          Serial.println(tt);
        } else if (tt == 3) {
          display.drawBitmap(17, 14, coffeeCup30_06, 30, 30, WHITE);
          Serial.println(tt);
        } else if (tt == 2) {
          display.drawBitmap(17, 14, coffeeCup30_07, 30, 30, WHITE);
          Serial.println(tt);
        } else if (tt == 1) {
          display.drawBitmap(17, 14, coffeeCup30_08, 30, 30, WHITE);
          Serial.println(tt);
        }
        if (tt == 1 && timerCount <= 24) {
          tt = 8;
        } else {
          tt--;
        }
      } else {
        if (tt == 8) {
          display.drawBitmap(17, 14, coffeeCup30_09, 30, 30, WHITE);
          Serial.println(tt);
        } else if (tt == 7) {
          display.drawBitmap(17, 14, coffeeCup30_10, 30, 30, WHITE);
          Serial.println(tt);
        } else if (tt == 6) {
          display.drawBitmap(17, 14, coffeeCup30_11, 30, 30, WHITE);
          Serial.println(tt);
        } else if (tt == 5) {
          display.drawBitmap(17, 14, coffeeCup30_12, 30, 30, WHITE);
          Serial.println(tt);
        } else if (tt == 4) {
          display.drawBitmap(17, 14, coffeeCup30_13, 30, 30, WHITE);
          Serial.println(tt);
        } else if (tt == 3) {
          display.drawBitmap(17, 14, coffeeCup30_14, 30, 30, WHITE);
          Serial.println(tt);
        } else if (tt == 2) {
          display.drawBitmap(17, 14, coffeeCup30_15, 30, 30, WHITE);
          Serial.println(tt);
        } else if (tt == 1) {
          display.drawBitmap(17, 14, coffeeCup30_16, 30, 30, WHITE);
          Serial.println(tt);
        }
        if (tt == 1) {    // tt gets count down from 8 to 1
          tt = 8;
        } else {
          tt--;
        }
      }

      if (maraData[3].toInt() < 100) {      // [3] = Mara Hx temperature ... take care of 2 or 3 digit value display
        display.setCursor(19, 50);
      } else {
        display.setCursor(9, 50);
      }
      display.setTextSize(2);
      display.print(maraData[3].toInt());
      display.setTextSize(1);
      display.print((char)247);
      display.setTextSize(1);
      display.print("C");
    } else {
      //Coffee temperature and bitmap
      display.drawBitmap(17, 14, coffeeCup30_00, 30, 30, WHITE);
      if (maraData[3].toInt() < 100) {
        display.setCursor(19, 50);
      } else {
        display.setCursor(9, 50);
      }
      display.setTextSize(2);
      display.print(maraData[3].toInt());
      display.setTextSize(1);
      display.print((char)247);
      display.setTextSize(1);
      display.print("C");

      //Steam temperature and bitmap
      display.drawBitmap(83, 14, steam30, 30, 30, WHITE);
      if (maraData[1].toInt() < 100) {
        display.setCursor(88, 50);
      } else {
        display.setCursor(78, 50);
      }
      display.setTextSize(2);
      display.print(maraData[1].toInt());
      display.setTextSize(1);
      display.print((char)247);
      display.setTextSize(1);
      display.print("C");

      //Draw Line
      display.drawLine(66, 14, 66, 64, WHITE);

      //Boiler
      if (maraData[5].toInt() == 1) {
        display.setCursor(13, 0);
        display.setTextSize(1);
        display.print("Heating up");

        if ((millis() - lastToggleTime) > 1000) {
          lastToggleTime = millis();
          if (HeatDisplayToggle == 1) {
            HeatDisplayToggle = 0;
          } else {
            HeatDisplayToggle = 1;
          }
        }
        if (HeatDisplayToggle == 1) {
          display.fillRect(0, 0, 12, 12, BLACK);
          display.drawCircle(3, 3, 3, WHITE);
          display.fillCircle(3, 3, 2, WHITE);

        } else {
          display.fillRect(0, 0, 12, 12, BLACK);
          display.drawCircle(3, 3, 3, WHITE);
          // display.print("");
        }
      } else {
        display.print("");
        display.fillCircle(3, 3, 3, BLACK);

        //Draw machine mode
        if (maraData[0].substring(0, 1) == "C") {     // [0] = Mode & Version number
          // Coffee mode
          display.drawBitmap(115, 0, coffeeCup12, 12, 12, WHITE);   // Draw Coffe Cup upper right corner
          display.drawBitmap(58, 0, wifiicon, 12, 12, WHITE);       // Draw WiFi Icon upper center
        } else {
          // Steam mode
          display.drawBitmap(115, 0, steam12, 12, 12, WHITE);       // Draw Steam upper right corner
          display.drawBitmap(58, 0, wifiicon, 12, 12, WHITE);       // Draw WiFi Icon upper center
        }
      }
    }
  }

  display.display();
}
// -----------------------------------------------------------
// -----------------------------------------------------------
// -----------------------------------------------------------
// Main Loop
void loop() {
  t.update();
  detectChanges();
  getMaraData();
}
