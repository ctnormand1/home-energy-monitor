/*
 * Main sketch to run on the Arduino home energy monitor. 
 */
#include <WiFiNINA.h>               // Networking
#include <ArduinoMqttClient.h>      // Networking
#include <ArduinoBearSSL.h>         // Networking
#include <ArduinoECCX08.h>          // Networking
#include <TimeLib.h>                // Timekeeping
#include <Wire.h>                   // I2C
#include <SerLCD.h>                 // Display
#include <SparkFun_Qwiic_Button.h>  // Button
#include <CurrentSensor.h>          // Current sensors
#include <ArduinoJson.h>            // Message formatting
#include <ArduinoUniqueID.h>        // Device serial number
//#include <Adafruit_SleepyDog.h>     // Watchdog timer
#include <MemoryFree.h>             // Memory tracking

// Store sensitive info in secrets.h
#include "secrets.h"
const char ssid[] = SECRET_SSID;
const char pass[] = SECRET_PASS;
const char broker[] = SECRET_BROKER;
const char* certificate = SECRET_CERTIFICATE;

// Initialize networking resources
WiFiClient wifiClient;  // Used for TCP connection
BearSSLClient sslClient(wifiClient);  // Used for SSL/TLS connection
MqttClient mqttClient(sslClient);

// Initialize hardware
SerLCD lcd;
QwiicButton button;
CurrentSensor sensor1(A1, 3, 300);
CurrentSensor sensor2(A3, 4, 300);

// Initialize JSON resources
constexpr int jsonCapacity = 750;
StaticJsonDocument<jsonCapacity> jsonDoc;
StaticJsonDocument<300> eventDoc;

// State variables
int wifiState, mqttState, displayState;
float current1 = -1, current2 = -1;
int timeOffset = -6;
unsigned int lastSampleMillis, lastDispUpdateMillis, backlightOnMillis, commWaitingMillis;
short sampleIx = 0, eventIx = 0, sampleNum = 6;
bool commEnabled = true, commWaiting = false, backlightOn;
char dispBuffer[17], deviceIDString[17];

void setup() {  
  // Begin I2C devices
  Wire.begin();
  lcd.begin(Wire);
  button.begin();

  // Allow WiFi and MQTT to be disabled by holding the button on startup
  if (button.isPressed()) {
    commEnabled = false;
    wifiState = 3;
    mqttState = 3;
  }

  // LCD config and splash screen
  lcd.clear();
  turnBacklightOff();
  lcd.setContrast(5);
  delay(1000);
  turnBacklightOn();
  lcd.println("SmartWatt Home");
  lcd.println("Energy Monitor");

  // ADC configuration
  analogReadResolution(12);
  analogReference(AR_INTERNAL1V65);

  // Set callback for certificate validation
  ArduinoBearSSL.onGetTime(getTime);

  // Set ECCX08 slot to use for private key
  // and the corresponding public certificate
  sslClient.setEccSlot(0, certificate);

  // Get the serial number from SAMD21 chip
  snprintf(deviceIDString, 17, "%02X%02X%02X%02X%02X%02X%02X%02X", \
    UniqueID8[0], UniqueID8[1], UniqueID8[2], UniqueID8[3], \
    UniqueID8[4], UniqueID8[5], UniqueID8[6], UniqueID8[7]);

  while (millis() < 6000);  // Make sure splash screen displays for a few seconds

  displayState = 0;
  lastSampleMillis = 0;
  backlightOnMillis = millis();

//  Watchdog.enable(10000);  // 10 second watchdog timer

  eventDoc["data"][eventIx]["event_type"] = "DEVICE_RESET";
  eventDoc["data"][eventIx]["timestamp"] = getTime();
  eventIx++;
}

void loop() {
  // If the device can't connect to networking resources, it'll wait 5 minutes before trying again.
  // This checks to see if 5 minutes has passed.
  if (commWaiting && millis() - commWaitingMillis > 300000) {
    commWaiting = false;
  }
  
  // Check WiFi status and connect if disconnected
  if (commEnabled && !commWaiting && WiFi.status() != WL_CONNECTED){
    wifiState = 0;
    mqttState = 0;
    updateDisplay();
    if (!connectWiFi()) {
      disconnectComms();
      commWaiting = true;
      commWaitingMillis = millis();
    }
  }

  // Check MQTT status and connect if disconnected
  if (commEnabled && !commWaiting && !mqttClient.connected()) {
    mqttState = 0;
    updateDisplay();
    if (!connectMQTT()) {
      disconnectComms();
      commWaiting = true;
      commWaitingMillis = millis();
    }
  }

  if (!eventDoc.isNull()) {
    sendMessage("event", eventDoc);
    eventIx = 0;
  }

  // Kick the sensors so they take a reading
  sensor1.getReading();
  sensor2.getReading();

  if (millis() - lastSampleMillis > 5000) {    
    current1 = sensor1.returnReading();
    current2 = sensor2.returnReading();
    jsonDoc["data"][sampleIx]["time"] = getTime();
    if (current1 > 0) {
      jsonDoc["data"][sampleIx]["sensor_1"] = current1 * 120;
    }
    if (current2 > 0) {
      jsonDoc["data"][sampleIx]["sensor_2"] = current2 * 120;
    }
    jsonDoc["data"][sampleIx]["free_memory"] = freeMemory();   
    sampleIx ++;
    lastSampleMillis = millis();
  }

  if (sampleIx >= sampleNum) {
//    jsonDoc["deviceId"] = deviceIDString;
//    if (mqttClient.connected()) {
//      mqttClient.beginMessage("smartwatt/outgoing", (unsigned long)measureJson(jsonDoc));
//      serializeJson(jsonDoc, mqttClient);
//      mqttClient.endMessage();
//      Wire.setClock(100000);
//    }
//    jsonDoc.clear();
    sendMessage("sensor_data", jsonDoc);
    sampleIx = 0;
  }
  
  if (button.hasBeenClicked()) {
    button.clearEventBits();
    if (backlightOn) {
      displayState = (displayState + 1) % 3;
      updateDisplay();
    }
    turnBacklightOn();
  }
  if (millis() - lastDispUpdateMillis > 5000) {
    updateDisplay();
  }
  if (backlightOn && millis() - backlightOnMillis > 60000) {
    turnBacklightOff();
  }

//  Watchdog.reset();
}

boolean connectWiFi() {
  wifiState = 1;
  updateDisplay();
  byte attempts = 0;
//  Watchdog.disable();
  while ((WiFi.begin(ssid, pass) != WL_CONNECTED) && (attempts < 5)) {
    attempts ++;
    delay(5000);  // On failure, wait 5s and try again
  }
//  Watchdog.enable(10000);
  if (WiFi.status() != WL_CONNECTED) {
    wifiState = 0;
    updateDisplay();
    return false;
  }
  wifiState = 2;
  updateDisplay();
  eventDoc["data"][eventIx]["event_type"] = "WIFI_CONNECT";
  eventDoc["data"][eventIx]["timestamp"] = getTime();
  eventIx ++;
  return true;
}


boolean connectMQTT() {
  Serial.println("Connecting MQTT");
  if (wifiState == 0) {
    return false;
  }
  mqttState = 1;
  updateDisplay();
  byte attempts = 0;
//  Watchdog.disable();
  while ((!mqttClient.connect(broker, 8883)) && (attempts < 5)) {
    Serial.print("Attempt ");
    Serial.println(attempts);
    attempts ++;
    delay(5000); // On failure, wait 5s and try again
  }
  Wire.setClock(100000);
//  Watchdog.enable(10000);
//  connectI2CDevices();
  if (!mqttClient.connected()) {
    mqttState = 0;
    updateDisplay();
    return false;
  }
  mqttState = 2;
  updateDisplay();
  eventDoc["data"][eventIx]["event_type"] = "MQTT_CONNECT";
  eventDoc["data"][eventIx]["timestamp"] = getTime();
  eventIx ++;
  return true;
}

unsigned long getTime() {
  return WiFi.getTime();
}

void turnBacklightOn() {
  lcd.setBacklight(255, 255, 255);
  backlightOnMillis = millis();
  backlightOn = true;
}

void turnBacklightOff() {
  lcd.setBacklight(0, 0, 0);
  backlightOn = false;
}

void disconnectComms() {
  WiFi.end();
  wifiState = 0;
//  mqttClient.disconnect();
  mqttState = 0;
  updateDisplay();
}

void sendMessage(char *msgType, JsonDocument& payload) {
  payload["device_id"] = deviceIDString;
  payload["timestamp"] = getTime();
  payload["msg_type"] = msgType;
  if (mqttClient.connected()) {
    mqttClient.beginMessage("smartwatt/outgoing", (unsigned long)measureJson(payload));
    serializeJson(jsonDoc, mqttClient);
    mqttClient.endMessage();
    Wire.setClock(100000);
  }
  payload.clear();
}

void updateDisplay() {
  float dispAmps = 0;
  int dispWatts = 0;
  lastDispUpdateMillis = millis();
  lcd.clear();
  switch (displayState) {
    case 0: {
      lcd.print("WiFi: ");
      switch (wifiState) {
        case 0:
          lcd.print("No Conn");
          break;
        case 1:
          lcd.print("Connecting");
          break;
        case 2:
          lcd.print("Connected");
          break;
        case 3:
          lcd.print("Disabled");
          break;
      }
    }
      lcd.setCursor(0, 1);
      lcd.print("MQTT: ");
      switch (mqttState) {
        case 0:
          lcd.print("No Conn");
          break;
        case 1:
          lcd.print("Connecting");
          break;
        case 2:
          lcd.print("Connected");
          break;
        case 3:
          lcd.print("Disabled");
          break;
      }
      break;
    case 1: {
      time_t t = getTime() + timeOffset * 3600;
      snprintf(dispBuffer, 17, "%02d/%02d/%4d %02d:%02d", month(t), day(t), year(t), hour(t), minute(t));
      lcd.print(dispBuffer);    
      if ((current1 > 0) || (current2 > 0)) {
        dispAmps = max(current1, 0) + max(current2, 0);
        dispWatts = dispAmps * 120;
      }        
      snprintf(dispBuffer, 17, "%5.1fA   %5dW", dispAmps, dispWatts); 
      lcd.print(dispBuffer);
      break;
    }
    case 2: {
      snprintf(dispBuffer, 17, "1: %5.1fA %5dW", max(current1, 0), max((int)(current1 * 120), 0));
      lcd.print(dispBuffer);
      snprintf(dispBuffer, 17, "2: %5.1fA %5dW", max(current2, 0), max((int)(current2 * 120), 0));
      lcd.print(dispBuffer);
      break;
    }
  }
}
