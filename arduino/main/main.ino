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
constexpr int jsonCapacity = JSON_ARRAY_SIZE(6) + 13 * JSON_OBJECT_SIZE(2);
StaticJsonDocument<jsonCapacity> jsonDoc;

// State variables
int wifiState, mqttState, displayState;
float current1 = -1, current2 = -1;
int timeOffset = -6;
unsigned int lastSampleMillis, lastDispUpdateMillis;
short sampleIx = 0, sampleNum = 6;
bool commEnabled = true;

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

  // Display splash screen
  lcd.clear();
  lcd.setBacklight(0, 0, 0);
  lcd.setContrast(5);
  delay(1000);
  lcd.setBacklight(255, 255, 255);
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

  while (millis() < 6000);  // Make the splashscreen display for a few seconds

  displayState = 0;
  lastSampleMillis = 0;
}

void loop() {
  // Check WiFi status and connect if disconnected
  if (commEnabled && WiFi.status() != WL_CONNECTED){
    wifiState = 0;
    mqttState = 0;
    updateDisplay();
    connectWiFi();
  }

  // Check MQTT status and connect if disconnected
  if (commEnabled && !mqttClient.connected()) {
    mqttState = 0;
    updateDisplay();
    connectMQTT();
  }

  // Kick the sensors so they take a reading
  sensor1.getReading();
  sensor2.getReading();

  
  if (millis() - lastSampleMillis > 5000) {
    current1 = sensor1.returnReading();
    current2 = sensor2.returnReading();
    jsonDoc[sampleIx]["time"] = getTime();
    if (current1 > 0) {
      jsonDoc[sampleIx]["sensor1"] = current1 * 120;
    }
    if (current2 > 0) {
      jsonDoc[sampleIx]["sensor2"] = current2 * 120;
    }
    sampleIx ++;
    lastSampleMillis = millis();
  }

  if (sampleIx >= sampleNum) {
    if (mqttClient.connected()) {
      mqttClient.beginMessage("smartwatt/outgoing", (unsigned long)measureJson(jsonDoc));
      serializeJson(jsonDoc, mqttClient);
      mqttClient.endMessage();
    }
    jsonDoc.clear();
    sampleIx = 0;
  }
  
  if (button.hasBeenClicked()) {
    button.clearEventBits();
    displayState = (displayState + 1) % 3;
    updateDisplay();
  }

  if (millis() - lastDispUpdateMillis > 5000) {
    updateDisplay();
  }
}

boolean connectWiFi() {
  wifiState = 1;
  updateDisplay();

  byte attempts = 0;
  while ((WiFi.begin(ssid, pass) != WL_CONNECTED) && (attempts < 5)) {
    attempts ++;
    delay(5000);  // On failure, wait 5s and try again
  }
  
  if (WiFi.status() != WL_CONNECTED) {
    wifiState = 0;
    updateDisplay();
    return false;
  }

  wifiState = 2;
  updateDisplay();
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
    delay(5000); // On failure, wait 5s and try again
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
  return true;
}

unsigned long getTime() {
  return WiFi.getTime();
}

void connectI2CDevices() {
  Wire.begin();
  lcd.begin(Wire);
}


void updateDisplay() {
  char dispBuffer[16];
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
      sprintf(dispBuffer, "%02d/%02d/%4d %02d:%02d", month(t), day(t), year(t), hour(t), minute(t));
      lcd.print(dispBuffer);
      
      if ((current1 > 0) || (current2 > 0)) {
        dispAmps = max(current1, 0) + max(current2, 0);
        dispWatts = dispAmps * 120;
      }
         
      sprintf(dispBuffer, "%5.1fA   %5dW", dispAmps, dispWatts); 
      
      lcd.print(dispBuffer);
      break;
    }
    case 2: {
      sprintf(dispBuffer, "1: %5.1fA %5dW", max(current1, 0), max((int)(current1 * 120), 0));
      lcd.print(dispBuffer);
      sprintf(dispBuffer, "2: %5.1fA %5dW", max(current2, 0), max((int)(current2 * 120), 0));
      lcd.print(dispBuffer);
      break;
    }
  }
}
