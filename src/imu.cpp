#include <Arduino.h>
#include <Wire.h>
#include <math.h>
#include "imu.h"

namespace {
constexpr uint8_t MPU_ADDRESS = 0x68;
constexpr uint8_t ACCEL_DATA_REGISTER = 0x3B;
constexpr uint8_t POWER_MANAGEMENT_REGISTER = 0x6B;
constexpr uint8_t CALIBRATION_SAMPLES = 64;
constexpr unsigned long CALIBRATION_SAMPLE_INTERVAL_MS = 10;
constexpr unsigned long DIAGNOSTIC_INTERVAL_MS = 250;
constexpr float MIN_JERK_DELTA_COUNTS = 16000.0f;
constexpr float NOISE_THRESHOLD_MULTIPLIER = 4.0f;
constexpr unsigned long JERK_ALERT_DURATION_MS = 100;

bool sensorReady = false;
float restingAx = 0;
float restingAy = 0;
float restingAz = 0;
float jerkThresholdCounts = MIN_JERK_DELTA_COUNTS;
int16_t currentAx = 0;
int16_t currentAy = 0;
int16_t currentAz = 0;
unsigned long jerkAlertUntil = 0;
unsigned long lastDiagnosticAt = 0;

bool readAcceleration(int16_t &ax, int16_t &ay, int16_t &az) {
  Wire.beginTransmission(MPU_ADDRESS);
  Wire.write(ACCEL_DATA_REGISTER);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }

  if (Wire.requestFrom(MPU_ADDRESS, (uint8_t)6, (uint8_t)true) != 6) {
    while (Wire.available()) {
      Wire.read();
    }
    return false;
  }

  uint16_t rawAx = ((uint16_t)Wire.read() << 8) | Wire.read();
  uint16_t rawAy = ((uint16_t)Wire.read() << 8) | Wire.read();
  uint16_t rawAz = ((uint16_t)Wire.read() << 8) | Wire.read();
  ax = (int16_t)rawAx;
  ay = (int16_t)rawAy;
  az = (int16_t)rawAz;
  return true;
}
}

bool imuBegin() {
  Wire.begin(21, 22);
  jerkAlertUntil = 0;

  Wire.beginTransmission(MPU_ADDRESS);
  if (Wire.endTransmission() != 0) {
    sensorReady = false;
    return false;
  }

  // Match the working raw-I2C sketch: wake the MPU6050 from sleep mode.
  Wire.beginTransmission(MPU_ADDRESS);
  Wire.write(POWER_MANAGEMENT_REGISTER);
  Wire.write(0);
  if (Wire.endTransmission() != 0) {
    sensorReady = false;
    return false;
  }
  delay(100);

  int16_t initialAx;
  int16_t initialAy;
  int16_t initialAz;
  sensorReady = readAcceleration(initialAx, initialAy, initialAz);
  if (!sensorReady) {
    return false;
  }

  // Keep the IMU still while averaging its resting gravity vector and noise.
  int64_t sumX = 0;
  int64_t sumY = 0;
  int64_t sumZ = 0;
  int64_t sumDeltaSquared = 0;
  int16_t previousX = 0;
  int16_t previousY = 0;
  int16_t previousZ = 0;

  for (uint8_t sample = 0; sample < CALIBRATION_SAMPLES; ++sample) {
    int16_t ax;
    int16_t ay;
    int16_t az;
    if (!readAcceleration(ax, ay, az)) {
      sensorReady = false;
      return false;
    }

    sumX += ax;
    sumY += ay;
    sumZ += az;

    if (sample > 0) {
      int32_t dx = (int32_t)ax - previousX;
      int32_t dy = (int32_t)ay - previousY;
      int32_t dz = (int32_t)az - previousZ;
      sumDeltaSquared += (int64_t)dx * dx + (int64_t)dy * dy + (int64_t)dz * dz;
    }

    previousX = ax;
    previousY = ay;
    previousZ = az;
    delay(CALIBRATION_SAMPLE_INTERVAL_MS);
  }

  restingAx = (float)sumX / CALIBRATION_SAMPLES;
  restingAy = (float)sumY / CALIBRATION_SAMPLES;
  restingAz = (float)sumZ / CALIBRATION_SAMPLES;

  float noiseRms = sqrtf((float)sumDeltaSquared / (CALIBRATION_SAMPLES - 1));
  jerkThresholdCounts = fmaxf(
      MIN_JERK_DELTA_COUNTS,
      noiseRms * NOISE_THRESHOLD_MULTIPLIER
  );

  Serial.printf(
      "IMU resting baseline ACC: %.0f %.0f %.0f; jerk threshold: %.0f counts\n",
      restingAx,
      restingAy,
      restingAz,
      jerkThresholdCounts
  );
  return true;
}

void imuUpdate() {
  if (!sensorReady) {
    return;
  }

  if (!readAcceleration(currentAx, currentAy, currentAz)) {
    sensorReady = false;
    Serial.println("MPU6050 read failed; IMU alarm disabled.");
    return;
  }

  // Compare against the calibrated resting vector, not a fractional raw-count threshold.
  float dx = currentAx - restingAx;
  float dy = currentAy - restingAy;
  float dz = currentAz - restingAz;
  float accelerationChange = sqrtf(
      dx * dx +
      dy * dy +
      dz * dz
  );

  unsigned long now = millis();
  if (accelerationChange >= jerkThresholdCounts) {
    jerkAlertUntil = now + JERK_ALERT_DURATION_MS;
  }

  if (now - lastDiagnosticAt >= DIAGNOSTIC_INTERVAL_MS) {
    lastDiagnosticAt = now;
    Serial.printf(
        "IMU ACC: %d %d %d; delta: %.0f / %.0f; jerk: %s\n",
        currentAx,
        currentAy,
        currentAz,
        accelerationChange,
        jerkThresholdCounts,
        accelerationChange >= jerkThresholdCounts ? "HIGH" : "LOW"
    );
  }
}

bool imuIsJerking() {
  return sensorReady && (long)(millis() - jerkAlertUntil) < 0;
}