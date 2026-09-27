#include <Arduino.h>
#include <Wire.h>
#include <MAX30105.h>
#include "heart_rate.h"

namespace {
// Tune these against the raw IR values from the sensor on the forehead.
constexpr long FOREHEAD_CONTACT_IR_THRESHOLD = 10000;
constexpr long PEAK_CHANGE_THRESHOLD = 100;
constexpr uint8_t RATE_SAMPLE_COUNT = 4;
constexpr unsigned long MIN_BEAT_INTERVAL_MS = 300;
constexpr unsigned long MAX_BEAT_INTERVAL_MS = 1500;

MAX30105 sensor;
bool sensorReady = false;
bool hasPreviousIr = false;
bool rising = false;
long rates[RATE_SAMPLE_COUNT] = {};
uint8_t nextRateIndex = 0;
uint8_t validRateCount = 0;
unsigned long lastBeatAt = 0;
long previousIr = 0;
int averageBpm = 0;
}

bool heartRateBegin() {
  sensorReady = sensor.begin(Wire, I2C_SPEED_STANDARD);
  if (!sensorReady) {
    return false;
  }

  sensor.setup(
      60,   // LED brightness
      4,    // Average samples to reduce noise and sensor processing
      2,    // Red and IR LEDs
      100,  // Sample rate
      411,  // Pulse width
      4096  // ADC range
  );
  return true;
}

void heartRateUpdate() {
  if (!sensorReady) {
    return;
  }

  long irValue = sensor.getIR();
  if (irValue < FOREHEAD_CONTACT_IR_THRESHOLD) {
    hasPreviousIr = false;
    rising = false;
    validRateCount = 0;
    nextRateIndex = 0;
    lastBeatAt = 0;
    averageBpm = 0;
    return;
  }

  if (!hasPreviousIr) {
    previousIr = irValue;
    hasPreviousIr = true;
    return;
  }

  if (irValue > previousIr + PEAK_CHANGE_THRESHOLD) {
    rising = true;
  }

  if (rising && irValue < previousIr - PEAK_CHANGE_THRESHOLD) {
    rising = false;
    unsigned long now = millis();

    if (lastBeatAt != 0) {
      unsigned long interval = now - lastBeatAt;
      if (interval >= MIN_BEAT_INTERVAL_MS && interval <= MAX_BEAT_INTERVAL_MS) {
        long bpm = 60000UL / interval;
        rates[nextRateIndex] = bpm;
        nextRateIndex = (nextRateIndex + 1) % RATE_SAMPLE_COUNT;
        if (validRateCount < RATE_SAMPLE_COUNT) {
          ++validRateCount;
        }

        long total = 0;
        for (uint8_t i = 0; i < validRateCount; ++i) {
          total += rates[i];
        }
        averageBpm = total / validRateCount;
      }
    }

    lastBeatAt = now;
  }

  previousIr = irValue;
}

int heartRateBpm() {
  return averageBpm;
}