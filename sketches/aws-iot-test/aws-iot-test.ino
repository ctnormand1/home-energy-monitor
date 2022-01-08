/*
  aws-iot-test.ino

  This sketch tests the functionality of sending JSON formatted
  messages to an AWS IoT Core MQTT broker. Fake measurements
  at user-specified time intervals. Once a certain amount of
  measurements have been taken (also user-specified), they are
  all sent to the MQTT broker with timestamps for each one.

  Script status is printed to the serial monitor.
  
 */

#include <WiFiNINA.h>
#include <ArduinoMqttClient.h>
#include <ArduinoBearSSL.h>
#include <ArduinoECCX08.h>
#include <ArduinoJson.h>
#include "secrets.h"

// Sampling configuration
int sampleDelay = 5000;  // ms between samples
int sampleNum = 6;  // Number of samples per message

// Sensitive info stored in secrets.h
const char ssid[] = SECRET_SSID;
const char pass[] = SECRET_PASS;
const char broker[] = SECRET_BROKER;
const char* certificate = SECRET_CERTIFICATE;

// Initialize networking resources
WiFiClient wifiClient;  // Used for TCP connection
BearSSLClient sslClient(wifiClient);  // Used for SSL/TLS connection
MqttClient mqttClient(sslClient);

// Initialize JSON resources
#define JSON_BUFFER_SIZE JSON_ARRAY_SIZE(6) + 6 * JSON_OBJECT_SIZE(2)
StaticJsonDocument<JSON_BUFFER_SIZE> jsonDoc;

// Initialize counters
unsigned long lastMillis = 0;
short sampleIx = 0;

void setup() {
  // Set callback for certificate validation
  ArduinoBearSSL.onGetTime(getTime);
  
  // Set ECCX08 slot to use for private key
  // and the corresponding public certificate
  sslClient.setEccSlot(0, certificate);

  Serial.begin(9600);
  while (!Serial);
}

void loop() {
  // Check WiFi connection status, and connect if disconnected
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }

  // Check MQTT connection status and connect if disconnected
  if (!mqttClient.connected()) {
    connectMQTT();
  }

  // Record a sample at user-defined interval
  if (millis() - lastMillis > sampleDelay) {
    if (sampleIx == 0) {
      Serial.print("Collecting ");
      Serial.print(sampleNum);
      Serial.print(" samples at a rate of 1 sample per ");
      Serial.print(sampleDelay);
      Serial.println(" ms"); 
    }
    
    jsonDoc[sampleIx]["time"] = getTime();
    jsonDoc[sampleIx]["data"] = random(100, 10000);

    Serial.print((unsigned long)jsonDoc[sampleIx]["time"]);
    Serial.print(": ");
    Serial.print((int)jsonDoc[sampleIx]["data"]);
    Serial.println(" W");
    
    sampleIx++;
    lastMillis = millis();
  }

  // Send a message when the specified number of samples is reached
  if (sampleIx >= sampleNum) {
    Serial.println();
    Serial.println("Sending message to MQTT broker");
    Serial.println();
    mqttClient.beginMessage("arduino/outgoing",
      (unsigned long)measureJson(jsonDoc));
    serializeJson(jsonDoc, mqttClient);
    mqttClient.endMessage();
    sampleIx = 0;
  }
}

void connectWiFi() {
  Serial.print("Attempting to connect to SSID: ");
  Serial.print(ssid);
  Serial.print(" ");

  while (WiFi.begin(ssid, pass) != WL_CONNECTED) {
    Serial.print(".");
    delay(5000);  // On failure, wait 5s and try again
  }
  
  Serial.println();
  Serial.println("Success!");
  Serial.println();
}

void connectMQTT() {
  Serial.println("Attempting to connect to MQTT broker:");
  Serial.println(broker);

  while (!mqttClient.connect(broker, 8883)) {
    Serial.print(".");
    delay(5000); // On failure, wait 5s and try again
  }

  Serial.println();
  Serial.println("Success!");
  Serial.println();
}

unsigned long getTime() {
  return WiFi.getTime();
}
