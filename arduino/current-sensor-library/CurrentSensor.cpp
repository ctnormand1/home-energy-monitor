#include "CurrentSensor.h"

CurrentSensor::CurrentSensor(byte _inPin, float _cal) {
  inPin = _inPin;
  vRef = 1.65;
  adcBits = 12;
  adcSteps = 1 << adcBits;
}

float CurrentSensor::readCurrent() {
  return analogRead(inPin);
}
