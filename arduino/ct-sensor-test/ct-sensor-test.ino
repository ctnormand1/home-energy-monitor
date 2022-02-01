/*
 * CT Sensor Test
 * 
 * This sketch generates sample output for TESTING
 * from a connected split core current transformer. 
 * ADC sampling rate on the Arduino Nano 33 IoT is capped by default 
 * at 1200 Hz. It's possible to sample faster than this, but that would
 * require me to edit things in the registry. The default sampling rate is
 * sufficient for my needs.
 */

byte inPin = A1;  // Analog input pin

// How many samples should we take? The arduino samples at about 1200 Hz, 
// so 300 samples would get us 250 milliseconds of data. Seems sufficient.
const int numSamples = 300;

// Filtering parameters
const int smoothingBufferLen = 3;  // Number of samples for the initial moving average
const int extremaRadius = 5;       // How many samples to the left and right for determining local extrema
const int extremaBufferLen = 6;    // How many extrema to to use for calculating the centerline voltage

// Arrayz 4 dayz 
int samples[numSamples], smoothingBuffer[smoothingBufferLen], extremaBuffer[extremaBufferLen];
int movingAvgTotal, startMillis, endMillis;
int extremaFlag, extremaCounter, vPeak;
float centerLine, diff, avgDiff, vPeakAvg, power, vrms, vAvg;
long vPeakTotal;
boolean isExtrema;

void setup() {
  // Change the reference voltage and the ADC resolution
  analogReference(AR_INTERNAL1V65);
  analogReadResolution(12);

  Serial.begin(9600);
  while (!Serial) {}  // Wait for serial connection
}

void loop() {
  // Fill smoothing buffer
  for (int i = 0; i < smoothingBufferLen; i++) {
    smoothingBuffer[i % smoothingBufferLen] = analogRead(inPin);
  }
  
  startMillis = millis();  // Start the real sampling.
  
  // Average the smoothing buffer and write result to samples array.
  // Then take a new reading and add that to the smoothing buffer.
  for (int i = 0; i < numSamples; i++) {
    samples[i] = average(smoothingBuffer, smoothingBufferLen);
    smoothingBuffer[i % smoothingBufferLen] = analogRead(inPin);
  }

  endMillis = millis();  // Sampling done.

  // Print smoothed sensor data to serial
  for (int i = 0; i < numSamples; i++) {
    Serial.println(samples[i]);
  }
  Serial.println();

  // Loop through the samples array and look for local extrema.
  extremaCounter = 0;  // Initialize the counter
  extremaFlag = 1;     // 1: local max, -1: local min
  vPeakTotal = 0;      // Initialize for vPeak continuous average

  for (int i = extremaRadius; i < (numSamples - extremaRadius); i++) {
    // Extrema are found by looping through a moving window of samples.
    isExtrema = true;
    for (int j = i - extremaRadius; j <= (i + extremaRadius); j++) {
      // Loop through window to determine if samples[i] is an extrema.
      if (((samples[i] - samples[j]) * extremaFlag) < 0) {
        isExtrema = false;
        break;
      }
    }

    // If not an extrema, short ciruit and advance the window.
    if (!isExtrema) {
      continue;
    }

    // If we made it this far, samples[i] is an extrema.
    // Once the extremaBuffer is full, begin calculating moving average.
    if (extremaCounter >= extremaBufferLen) {
      vAvg = average(extremaBuffer, extremaBufferLen);
      vPeakAvg = averageDiff(extremaBuffer, vAvg, extremaBufferLen);
      
      // Don't allow vPeak to increase by more than 5% above average.
      vPeak = min(abs((float)samples[i] - vAvg), vPeakAvg * 1.05);
      vPeakTotal += vPeak;

      // Print the index and filtered extrema value to serial
      Serial.print(i);
      Serial.print(", ");
      Serial.println(extremaFlag * vPeak + vAvg);
    }

    // Add the latest extrema to the buffer
    extremaBuffer[extremaCounter % extremaBufferLen] = samples[i];
    extremaCounter++;
    extremaFlag *= -1;
  }

  // Print elapsed time
  Serial.println();
  Serial.println(endMillis - startMillis);
  
  // I only want to do this once, so just chill here.
  while (1);
}


float average(int arr[], int len) {
  // Compute the average of an array.
  long total = 0;
  for (int i = 0; i < len; i++) {
    total += arr[i];
  }
  return (float)total/len;
}


float averageDiff(int * arr, float vCenter, int len) {
  // Compute the average distance from center.
  float totalDiff = 0;
  for (int i = 0; i < len; i++) {
    totalDiff += abs((float)arr[i] - vCenter);
  }
  return totalDiff / len;
}  
