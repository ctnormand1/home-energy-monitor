#ifndef CURRENTSENSOR_H
#define CURRENTSENSOR_H

#include <Arduino.h>

class CurrentSensor {
  private:

    // ADC settings
    const float vRef = 1.65;  // ADC reference voltage
    const int adcBits = 12;  // ADC bits
    const int adcSteps = 1 << 12;  // ADC steps

    // Sampling params
    const int numSamples = 300;  // For sample rate of 1200 Hz
    const int smoothingBufferLen = 3;
    const int extremaRadius = 5;
    const int extremaBufferLen = 6;

    // Sampling resources
    int samples[300], smoothingBuffer[3];
    int extremaBuffer[6];
    int movingAvgTotal, extremaFlag, extremaCounter, vPeak, numReadings;
    float centerLine, diff, avgDiff, vPeakAvg, power, vrms, vAvg, cal;
    long vPeakTotal, readingsTotal;
    boolean isExtrema;

    // Functions for averaging
    float average(int arr[], int len);
    float averageDiff(int arr[], float vCenter, int len);

    // Input pins
    byte inPin, checkPin;  // inPin = sensor input, checkPin = connector check

  public:
    CurrentSensor(byte inPin, byte checkPin, float cal);
    void init();
    boolean getReading();
    float returnReading();
    boolean isConnected();
};


#endif
