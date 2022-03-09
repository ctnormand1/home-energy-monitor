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
#include <Adafruit_SleepyDog.h>     // Watchdog timer
#include <MemoryFree.h>             // Memory tracking

// Comment
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

constexpr int jsonCapacity = 768;

// State variables
int wifiState, mqttState, displayState;
float current1 = -1, current2 = -1;
int timeOffset = -6;
unsigned long lastSampleMillis, lastDispUpdateMillis, backlightOnMillis, commWaitingMillis;
unsigned long deviceResetMillis, wifiReconnectMillis, mqttReconnectMillis;
short sampleIx = 0, eventIx = 0, sampleNum = 6;
bool commEnabled = true, commWaiting = false, backlightOn;
bool deviceReset = false, wifiReconnect = false, mqttReconnect = false;
char dispBuffer[17], deviceIDString[17];
unsigned long measurementTimes[6];
float readings1[6], readings2[6];
int freeMem[6];

void setup() {  
  deviceReset = true;
  
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

  Watchdog.enable(16000);  // 16 second watchdog timer
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
  
  // Kick the sensors so they take a reading
  sensor1.getReading();
  sensor2.getReading();

  // Record measurements every 5 seconds
  if (millis() - lastSampleMillis > 5000) {    
    current1 = sensor1.returnReading();
    current2 = sensor2.returnReading();
    measurementTimes[sampleIx] = getTime();
    freeMem[sampleIx] = freeMemory();
    if (current1 > 0) {
      readings1[sampleIx] = current1 * 120;
    } 
    else {
      readings1[sampleIx] = current1;
    }
    if (current2 > 0) {
      readings2[sampleIx] = current2 * 120;
    } 
    else {
      readings2[sampleIx] = current2;
    }
    sampleIx ++;
    lastSampleMillis = millis();
  }

  // Send a message as soon as we have 6 measurements.
  if (sampleIx >= sampleNum) {
    sendMessage();
    sampleIx = 0;
    deviceReset = false;
    wifiReconnect =false;
    mqttReconnect = false;
  }

  // Change display or turn on backlight if button was clicked
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

  Watchdog.reset();  // Kick watchdog
}

void sendMessage() {
  StaticJsonDocument<jsonCapacity> jsonDoc;
  jsonDoc["device_id"] = deviceIDString;
  jsonDoc["msg_timestamp"] = getTime();
  for (int i = 0; i < sampleNum; i++) {
    jsonDoc["measurements"]["timestamps"].add(measurementTimes[i]);
    jsonDoc["measurements"]["sensor_1"].add(readings1[i]);
    jsonDoc["measurements"]["sensor_2"].add(readings2[i]);
    jsonDoc["measurements"]["free_mem"].add(freeMem[i]);
  }
  if (deviceReset) {
    jsonDoc["events"]["device_reset"] = getTime() - millis() / 1000;
  }
  if (wifiReconnect) {
    jsonDoc["events"]["wifi_reconnect"] = getTime() - (millis() - wifiReconnectMillis) / 1000;
  }
  if (mqttReconnect) {
    jsonDoc["events"]["mqtt_reconnect"] = getTime() - (millis() - mqttReconnectMillis) / 1000;
  }
  if (mqttClient.connected()) {
    mqttClient.beginMessage("smartwatt/outgoing", (unsigned long)measureJson(jsonDoc));
    serializeJson(jsonDoc, mqttClient);
    mqttClient.endMessage();
  }
}

boolean connectWiFi() {
  wifiState = 1;
  updateDisplay();
  byte attempts = 0;
  while ((WiFi.begin(ssid, pass) != WL_CONNECTED) && (attempts < 5)) {
    attempts ++;
    Watchdog.reset();
    delay(5000);  // On failure, wait 5s and try again
    Watchdog.reset();
  }
  if (WiFi.status() != WL_CONNECTED) {
    wifiState = 0;
    updateDisplay();
    return false;
  }
  wifiState = 2;
  updateDisplay();
  wifiReconnect = true;
  wifiReconnectMillis = millis();
  return true;
}

boolean connectMQTT() {
  if (wifiState == 0) {
    return false;
  }
  mqttState = 1;
  updateDisplay();
  byte attempts = 0;
  while ((!mqttClient.connect(broker, 8883)) && (attempts < 5)) {
    attempts ++;
    Watchdog.reset();
    delay(5000); // On failure, wait 5s and try again
    Watchdog.reset();
  }
  Wire.setClock(100000);
//  connectI2CDevices();
  if (!mqttClient.connected()) {
    mqttState = 0;
    updateDisplay();
    return false;
  }
  mqttState = 2;
  updateDisplay();
  mqttReconnect = true;
  mqttReconnectMillis = millis();
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
  mqttState = 0;
  updateDisplay();
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
