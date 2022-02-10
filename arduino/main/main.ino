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
CurrentSensor sensor1(A3, 3, 300);
CurrentSensor sensor2(A5, 5, 300);

// State variables
int wifiState, mqttState, displayState;
float current1, current2;
int timeOffset = -6;

void setup() {
  Wire.begin();
  lcd.begin(Wire);
  button.begin();
  
  lcd.clear();
  lcd.setBacklight(255, 255, 255);
  lcd.setContrast(5);
  lcd.println("SmartWatt Home");
  lcd.println("Energy Monitor");

  analogReadResolution(12);
  analogReference(AR_INTERNAL1V65);

  // Set callback for certificate validation
  ArduinoBearSSL.onGetTime(getTime);

  // Set ECCX08 slot to use for private key
  // and the corresponding public certificate
  sslClient.setEccSlot(0, certificate);

  while (millis() < 5000);

  displayState = 0;
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    wifiState = 0;
    updateDisplay();
    connectWiFi();
  }
  if (!mqttClient.connected()) {
    mqttState = 0;
    updateDisplay();
    connectMQTT();
  }
  
  if (button.hasBeenClicked()) {
    button.clearEventBits();
    displayState = (displayState + 1) % 2;
  }
  updateDisplay();

  delay(2000);
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
  lcd.clear();
  switch (displayState) {
    case 0:
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
      }
      break;
    case 1:
      time_t t = getTime() + timeOffset * 3600;
      char dateTimeBuffer[16];
      sprintf(dateTimeBuffer, "%02d/%02d/%4d %02d:%02d", month(t), day(t), year(t), hour(t), minute(t));
      lcd.print(dateTimeBuffer);
      break;
  }
}
