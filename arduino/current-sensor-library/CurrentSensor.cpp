#include "Arduino.h"
#include "CurrentSensor.h"

CurrentSensor::CurrentSensor(byte _inPin, byte _checkPin, float _cal) {
  inPin = _inPin;
  checkPin = _checkPin;
  cal = _cal;
  numReadings = 0;
  readingsTotal = 0;
  pinMode(checkPin, INPUT_PULLUP);
}

float CurrentSensor::average(int arr[], int len) {
  // Compute the average of an array.
  long total = 0;
  for (int i = 0; i < len; i++) {
    total += arr[i];
  }
  return (float)total/len;
}

float CurrentSensor::averageDiff(int arr[], float vCenter, int len) {
  // Compute the average distance from center.
  float totalDiff = 0;
  for (int i = 0; i < len; i++) {
    totalDiff += abs((float)arr[i] - vCenter);
  }
  return totalDiff / len;
}

boolean CurrentSensor::isConnected() {
  return !digitalRead(checkPin);
}

boolean CurrentSensor::getReading() {
  // Return false if the sensor is disconnected
  if (!isConnected()) {
    return false;
  }

  // Get samples with analogRead and apply a rolling average
  for (int i = 0; i < smoothingBufferLen; i++) {
    smoothingBuffer[i % smoothingBufferLen] = analogRead(inPin);
  }
  for (int i = 0; i < numSamples; i++) {
    samples[i] = average(smoothingBuffer, smoothingBufferLen);
    smoothingBuffer[i % smoothingBufferLen] = analogRead(inPin);
  }

  // Loop through samples and identify local extrema
  extremaCounter = 0;  // Initialize counter
  extremaFlag = 1;     // 1: local max, -1: local min
  vPeakTotal = 0;      // initialize for running average

  for (int i = extremaRadius; i < (numSamples - extremaRadius); i++) {
    isExtrema = true;
    for (int j = i - extremaRadius; j <= (i + extremaRadius); j++) {
      if (((samples[i] - samples[j]) * extremaFlag) < 0) {
        isExtrema = false;
        break;
      }
    }

    // If the sample is not an extrema, break loop and advance search window
    if (!isExtrema) {
      continue;
    }

    // If we made it this far, the sample is a local extrema.
    if (extremaCounter >= extremaBufferLen) {
      vAvg = average(extremaBuffer, extremaBufferLen);
      vPeakAvg = averageDiff(extremaBuffer, vAvg, extremaBufferLen);

      // Don't allow vPeak to increase by more than 5% above average
      vPeakTotal += min(abs((float)samples[i] - vAvg), vPeakAvg * 1.05);
    }

    // Add new extrema to buffer
    extremaBuffer[extremaCounter % extremaBufferLen] = samples[i];
    extremaCounter ++;
    extremaFlag *= -1;
  }

  readingsTotal += vPeakTotal / extremaCounter;
  numReadings ++;
  return true;
}

float CurrentSensor::returnReading() {
  float current;
  if ((numReadings == 0) || (!isConnected())) {
    current = -1;
  }
  else {
    current = ((float)readingsTotal / numReadings) * vRef * cal * 0.707 / adcSteps;
  }
  readingsTotal = 0;
  numReadings = 0;
  return current;
}
