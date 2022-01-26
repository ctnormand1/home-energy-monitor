#ifndef CURRENTSENSOR_H
#define CURRENTSENSOR_H

#include <Arduino.h>

class CurrentSensor {
  private:
    byte inPin;
    float vRef;
    unsigned int adcBits;
    unsigned int adcSteps;

    unsigned int latestRead;
    unsigned int currReadIx;
    unsigned int oldestReadIx;
    unsigned int oldestPeakIx;
    unsigned int prevReads[9];
    unsigned int avgRead;
    unsigned int peaks[15];

  public:
    CurrentSensor(byte inPin, float cal);
    void init();
    float readCurrent();
};


#endif
