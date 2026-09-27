#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <MPU6050.h>

// Defining Screen stuff

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 180
#define OLED_RESET -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32

// Defining the IMU

mpu6050 mpu;

// Defining the Track

// Defining the Heartrate sensor

// constant pins

#define SDA_PIN 21
#define SCL_PIN 22

// put function declarations here:
int myFunction(int, int);


void setup() {
  // put your setup code here, to run once:
  Wire.begin(SDA_PIN, SCL_PIN);
  int result = myFunction(2, 3);
}

void loop() {
  // put your main code here, to run repeatedly: 
}

// put function definitions here:
int myFunction(int x, int y) {
  return x + y;
}