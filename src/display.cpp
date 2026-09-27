#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include "imu.h"
#include "heart_rate.h"
#include "wifi_ap.h"
#include "web_server.h"

constexpr uint8_t BUZZER_RIGHT_PIN = 13;
constexpr uint8_t BUZZER_LEFT_PIN = 27;
constexpr unsigned long BUZZ_DURATION_MS = 100;
constexpr int STATIONARY_CAR_X = 104;
constexpr int STATIONARY_CAR_Y = 73;
constexpr int STATIONARY_CAR_TRIGGER_RADIUS = 6;
constexpr int PASS_TRIGGER_DISTANCE = 12;
constexpr int PASS_REARM_DISTANCE = 20;
constexpr unsigned long IMU_UPDATE_INTERVAL_MS = 50;
constexpr unsigned long HEART_RATE_UPDATE_INTERVAL_MS = 40;
constexpr unsigned long DISPLAY_UPDATE_INTERVAL_MS = 100;
constexpr unsigned long MOTION_STEP_INTERVAL_MS = 40;


// =====================================================
// DISPLAY PINS
// =====================================================

#define TFT_CS    5
#define TFT_DC    2
#define TFT_RST   4

#define TFT_SCLK  18
#define TFT_MOSI  23

Adafruit_ST7735 tft = Adafruit_ST7735(
  TFT_CS,
  TFT_DC,
  TFT_RST
);

// Keep landscape rotation 1, but mirror left-to-right across the short vertical axis.
void mirrorDisplayLeftToRight() {
  constexpr uint8_t MADCTL_REGISTER = 0x36;
  constexpr uint8_t LANDSCAPE_1_MIRRORED_X = 0x20;

  tft.startWrite();
  tft.writeCommand(MADCTL_REGISTER);
  tft.spiWrite(LANDSCAPE_1_MIRRORED_X);
  tft.endWrite();
}

// =====================================================
// COLORS
// =====================================================

#define BG        0x18E3   // dark gray
#define ROAD_EDGE ST77XX_WHITE
#define ROAD      0xBDF7   // light gray
#define CAR       ST77XX_RED
#define TEXT      ST77XX_WHITE

// =====================================================
// MONZA-STYLE TRACK
// Based on the silhouette in your reference image
// =====================================================

const int trackX[] = {
  46, 44, 42, 41, 40, 40, 41, 43,
  47, 51, 56, 61, 67, 74, 82, 91,
  100, 109, 118, 126, 133, 138, 140,
  140, 138, 135, 130, 123, 115, 106,
  96, 86, 76, 66, 58, 52, 48, 45,
  43, 43, 45, 49, 55, 63, 72, 82,
  93, 104, 115, 125, 134
};

const int trackY[] = {
  106, 101, 95, 88, 80, 71, 62, 53,
  45, 38, 32, 29, 30, 35, 42, 48,
  53, 56, 57, 57, 57, 59, 63,
  68, 72, 75, 76, 76, 76, 76,
  76, 76, 76, 76, 75, 73, 69, 65,
  61, 58, 55, 53, 51, 49, 47, 45,
  43, 41, 39, 37, 35
};

const int N = sizeof(trackX) / sizeof(trackX[0]);

// =====================================================
// CAR POSITION
// =====================================================

float carPosition = 0.0;
float blueCarPosition = 0.0;

int oldCarX;
int oldCarY;
int oldBlueX;
int oldBlueY;

bool passAlertArmed = true;
bool passBuzzerActive = false;
bool previousStationaryCarContact = false;
bool stationaryBuzzerActive = false;
unsigned long passBuzzerOffAt = 0;
unsigned long stationaryBuzzerOffAt = 0;
bool leftBuzzerOutput = false;
bool rightBuzzerOutput = false;

// Timings

unsigned long seconds;
unsigned long minutes;
unsigned long ms;

// =====================================================
// DRAW THICK LINE
// =====================================================

void drawThickLine(
  int x1,
  int y1,
  int x2,
  int y2,
  uint16_t color,
  int width
) {

  for (int i = -width / 2; i <= width / 2; i++) {

    tft.drawLine(
      x1 + i,
      y1,
      x2 + i,
      y2,
      color
    );

    tft.drawLine(
      x1,
      y1 + i,
      x2,
      y2 + i,
      color
    );
  }
}

// =====================================================
// DRAW TRACK
// =====================================================

void drawTrack() {

  // Background
  tft.fillScreen(BG);

  // ---------------------------------------------
  // Track outer border
  // ---------------------------------------------

  for (int i = 0; i < N - 1; i++) {

    drawThickLine(
      trackX[i],
      trackY[i],
      trackX[i + 1],
      trackY[i + 1],
      ROAD_EDGE,
      10
    );
  }

  // Close track
  drawThickLine(
    trackX[N - 1],
    trackY[N - 1],
    trackX[0],
    trackY[0],
    ROAD_EDGE,
    10
  );

  // ---------------------------------------------
  // Track road
  // ---------------------------------------------

  for (int i = 0; i < N - 1; i++) {

    drawThickLine(
      trackX[i],
      trackY[i],
      trackX[i + 1],
      trackY[i + 1],
      ROAD,
      6
    );
  }

  // Close road
  drawThickLine(
    trackX[N - 1],
    trackY[N - 1],
    trackX[0],
    trackY[0],
    ROAD,
    6
  );
}

// =====================================================
// DRAW HUD
// =====================================================

void drawHUD() {
  tft.setTextColor(TEXT);
  tft.setTextSize(1);

  // -------------------------
  // LEFT
  // -------------------------

  tft.setCursor(2, 00);
  tft.print("LAP");
  tft.setCursor(2, 12);
  tft.print("01/10");

  
  // -------------------------
  // RIGHT
  // -------------------------
  tft.setCursor(110, 0);
  tft.print("DRS: ON");
}

// Refresh the BPM field occasionally so it stays readable without redrawing every loop.
void drawHeartRate() {
  static unsigned long lastDrawAt = 0;
  static bool hasDrawn = false;
  unsigned long now = millis();

  if (hasDrawn && now - lastDrawAt < 250) {
    return;
  }

  hasDrawn = true;
  lastDrawAt = now;
  tft.fillRect(96, 115, 64, 10, BG);
  tft.setCursor(98, 115);
  tft.setTextColor(TEXT);
  tft.setTextSize(1);
  tft.print("HR: ");

  int bpm = heartRateBpm();
  if (bpm > 0) {
    tft.print(bpm);
  } else {
    tft.print("--");
  }
}

// =====================================================
// GET CAR POSITION
// =====================================================

void getCarPosition(float position, int &x, int &y) {
  while (position < 0) {
    position += N;
  }
  while (position >= N) {
    position -= N;
  }

  int i = (int)position;
  int next = (i + 1) % N;
  float fraction = position - i;

  x = trackX[i] + (trackX[next] - trackX[i]) * fraction;
  y = trackY[i] + (trackY[next] - trackY[i]) * fraction;
}

// =====================================================
// DRAW CAR
// =====================================================

void drawCar() {

  int x;
  int y;

  getCarPosition(
    carPosition,
    x,
    y
  );

  tft.fillCircle(
    x,
    y,
    3,
    CAR
  );

  tft.fillCircle(
    x,
    y,
    1,
    ST77XX_WHITE
  );

  oldCarX = x;
  oldCarY = y;
}
// draw stationary car that isnt moving and to show the car on the track and the moving car avoids it. This is to show the car on the track and the moving car avoids it.


void drawStationaryCar()
{
    tft.fillCircle(
    STATIONARY_CAR_X,
    STATIONARY_CAR_Y,
        3,
    ST77XX_BLUE
    );
    
    tft.fillCircle(
    STATIONARY_CAR_X,
    STATIONARY_CAR_Y,
        1,
        ST77XX_WHITE
    );
}

// drawing and erasing moving reckless car

void drawBlueCar(int x, int y)
{
    tft.fillCircle(
        x,
        y,
        3,
        ST77XX_BLUE
    );

    tft.fillCircle(
        x,
        y,
        1,
        ST77XX_WHITE
    );

    oldBlueX = x;
    oldBlueY = y;
}

void eraseBlueCar()
{
    tft.fillCircle(
        oldBlueX,
        oldBlueY,
        4,
        ROAD
    );
}

// Beep once as the faster blue car approaches from screen-left; rearm after it separates.
void updatePassBuzzer()
{
  unsigned long now = millis();

  int redX, redY;
  int blueX, blueY;
  getCarPosition(carPosition, redX, redY);
  getCarPosition(blueCarPosition, blueX, blueY);

  int dx = blueX - redX;
  int dy = blueY - redY;
  int distanceSquared = dx * dx + dy * dy;
  int triggerDistanceSquared = PASS_TRIGGER_DISTANCE * PASS_TRIGGER_DISTANCE;
  int rearmDistanceSquared = PASS_REARM_DISTANCE * PASS_REARM_DISTANCE;

  if (distanceSquared > rearmDistanceSquared) {
    passAlertArmed = true;
  }

  bool approachingFromLeft = blueX < redX && distanceSquared <= triggerDistanceSquared;
  if (passAlertArmed && approachingFromLeft) {
    passBuzzerActive = true;
    passBuzzerOffAt = now + BUZZ_DURATION_MS;
    passAlertArmed = false;
  }

  if (passBuzzerActive && (long)(now - passBuzzerOffAt) >= 0) {
    passBuzzerActive = false;
  }

}

// Ping the right buzzer once when the red car reaches the fixed blue car.
void updateStationaryCarBuzzer() {
  unsigned long now = millis();
  int redX, redY;
  getCarPosition(carPosition, redX, redY);

  int dx = redX - STATIONARY_CAR_X;
  int dy = redY - STATIONARY_CAR_Y;
  int triggerRadiusSquared = STATIONARY_CAR_TRIGGER_RADIUS * STATIONARY_CAR_TRIGGER_RADIUS;
  bool touchingStationaryCar = dx * dx + dy * dy <= triggerRadiusSquared;

  if (touchingStationaryCar && !previousStationaryCarContact) {
    stationaryBuzzerActive = true;
    stationaryBuzzerOffAt = now + BUZZ_DURATION_MS;
  }

  if (stationaryBuzzerActive && (long)(now - stationaryBuzzerOffAt) >= 0) {
    stationaryBuzzerActive = false;
  }

  previousStationaryCarContact = touchingStationaryCar;
}

void applyBuzzerOutputs() {
  bool imuAlarm = imuIsJerking();
  rightBuzzerOutput = stationaryBuzzerActive || imuAlarm;
  leftBuzzerOutput = passBuzzerActive || imuAlarm;

  digitalWrite(BUZZER_RIGHT_PIN, rightBuzzerOutput ? HIGH : LOW);
  digitalWrite(BUZZER_LEFT_PIN, leftBuzzerOutput ? HIGH : LOW);
}

// =====================================================
// ERASE OLD CAR
// =====================================================

void eraseCar() {

  // Restore the road underneath the old car.
  // The car stays near the center of the road,
  // so this doesn't touch the outer border.

  tft.fillCircle(
    oldCarX,
    oldCarY,
    4,
    ROAD
  );
}

// =====================================================
// SETUP
// =====================================================

void setup() {
  pinMode(BUZZER_RIGHT_PIN, OUTPUT);
  pinMode(BUZZER_LEFT_PIN, OUTPUT);
  digitalWrite(BUZZER_RIGHT_PIN, LOW);
  digitalWrite(BUZZER_LEFT_PIN, LOW);
  Serial.begin(115200);
  if (!startAccessPoint()) {
    Serial.println("Wi-Fi access point failed to start.");
  } else if (!startWebServer()) {
    Serial.println("HTTP server failed to start.");
  }

  if (!imuBegin()) {
    Serial.println("MPU6050 not detected; IMU alarm is disabled.");
  } else {
    Serial.println("MPU6050 initialized.");
  }
  if (!heartRateBegin()) {
    Serial.println("MAX30102 not detected; heart-rate display is disabled.");
  } else {
    Serial.println("MAX30102 initialized.");
  }

  // Start the faster car six track points behind the red car.
  blueCarPosition = carPosition - 6.0f;
  if (blueCarPosition < 0) {
    blueCarPosition += N;
  }

  SPI.begin(
    TFT_SCLK,
    -1,
    TFT_MOSI,
    TFT_CS
  );

  tft.initR(INITR_BLACKTAB);

  // 160 x 128 landscape
  tft.setRotation(1);
  mirrorDisplayLeftToRight();

  // Draw everything ONCE
  drawTrack();
  drawHUD();

  // Initial car
  getCarPosition(
    carPosition,
    oldCarX,
    oldCarY
  );

  drawCar();

  getCarPosition(blueCarPosition, oldBlueX, oldBlueY);
  drawBlueCar(oldBlueX, oldBlueY);
  drawStationaryCar();
}

// =====================================================
// LOOP
// =====================================================

void loop() {
  serviceWebServer();

  unsigned long now = millis();
  static unsigned long lastImuUpdate = 0;
  static unsigned long lastHeartRateUpdate = 0;
  static unsigned long lastDisplayUpdate = 0;
  static unsigned long lastMotionUpdate = 0;

  if (now - lastImuUpdate >= IMU_UPDATE_INTERVAL_MS) {
    lastImuUpdate = now;
    imuUpdate();
  }

  if (now - lastHeartRateUpdate >= HEART_RATE_UPDATE_INTERVAL_MS) {
    lastHeartRateUpdate = now;
    heartRateUpdate();
  }

  if (now - lastMotionUpdate >= MOTION_STEP_INTERVAL_MS) {
    unsigned long elapsedMotion = now - lastMotionUpdate;
    unsigned long steps = elapsedMotion / MOTION_STEP_INTERVAL_MS;
    lastMotionUpdate += steps * MOTION_STEP_INTERVAL_MS;

    carPosition += 0.12f * steps;
    blueCarPosition += 0.15f * steps;
    while (carPosition >= N) {
      carPosition -= N;
    }
    while (blueCarPosition >= N) {
      blueCarPosition -= N;
    }
  }

  updatePassBuzzer();
  updateStationaryCarBuzzer();
  applyBuzzerOutputs();

  if (now - lastDisplayUpdate >= DISPLAY_UPDATE_INTERVAL_MS) {
    lastDisplayUpdate = now;
    eraseCar();
    eraseBlueCar();

    ms = now % 1000;
    seconds = (now / 1000) % 60;
    minutes = now / 60000;

    tft.fillRect(2, 115, 75, 10, BG);
    tft.setCursor(2, 115);
    tft.setTextColor(TEXT);
    tft.setTextSize(1);
    tft.printf("%02lu:%02lu.%03lu", minutes, seconds, ms);

    drawCar();
    getCarPosition(blueCarPosition, oldBlueX, oldBlueY);
    drawBlueCar(oldBlueX, oldBlueY);
    drawStationaryCar();
    drawHeartRate();
  }

  publishTelemetry(
      now,
      heartRateBpm(),
      imuIsJerking(),
      leftBuzzerOutput,
      rightBuzzerOutput
  );

  delay(5);
}