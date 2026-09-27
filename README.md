# FTH ESP32 Telemetry Project

Firmware for an ESP32 Dev Module with an ST7735 display, MPU6050 accelerometer/gyroscope, MAX30102 optical heart-rate sensor, two buzzers, a Wi-Fi access point, and a browser-based live telemetry dashboard.

## Active Firmware

PlatformIO is configured in `platformio.ini` to compile these source files:

- `src/display.cpp` owns the Arduino `setup()` and `loop()`, TFT UI, simulated cars, buzzer decisions, and service scheduling.
- `src/imu.cpp` reads the MPU6050 over I2C, calibrates its resting acceleration, and reports jerk state.
- `src/heart_rate.cpp` reads MAX30102 IR samples and calculates a rolling BPM estimate.
- `src/wifi_ap.cpp` starts the ESP32 access point.
- `src/web_server.cpp` serves the dashboard and streams telemetry.

The files `src/main.cpp`, `src/ultrasonic.cpp`, and `src/buzztest.cpp` are not included by the current `build_src_filter`. They are retained as separate work-in-progress/test sources. Do not enable `main.cpp` alongside `display.cpp` unless its duplicate Arduino entry points are removed. The ultrasonic sensors are not currently initialized or represented on the dashboard.

## Build and Upload

Open this folder as a PlatformIO project. From the project directory:

```sh
pio run
pio run --target upload
pio device monitor --baud 115200
```

The Serial Monitor reports whether the MPU6050 and MAX30102 initialized, prints IMU calibration and diagnostic readings, and shows the Wi-Fi access point address. The configured monitor speed is 115200 baud.

## Wi-Fi and Dashboard

On startup, the ESP32 creates a password-protected access point:

- SSID: `FTH-ESP32`
- Password: `FTH-Connect-2026`
- Typical AP address: `192.168.4.1`

Connect a laptop or phone to that network and open `http://192.168.4.1/`. The network is local and does not provide internet. Change the SSID/password in `src/wifi_ap.cpp` if desired; WPA2 passwords must be at least 8 characters.

The dashboard is embedded in `src/web_server.cpp` and works offline. It displays current uptime, BPM, and IMU jerk state; graphs BPM, IMU HIGH/LOW, and left/right buzzer outputs; and logs alert state transitions with uptime timestamps. Graph windows are 15, 30, or 60 seconds. Graph history is held in the browser only and is lost when the page reloads.

Routes and transport:

- `GET /` serves the dashboard.
- `GET /api/status` returns a JSON snapshot with uptime, BPM, IMU state, and connected client count.
- WebSocket port `81` streams JSON samples containing `uptime_ms`, `heart_rate_bpm`, `imu_jerking`, `buzzer_left`, and `buzzer_right` at about 10 Hz.

## Sensors and Scheduling

The MPU6050 and MAX30102 share the ESP32 I2C bus:

- SDA: GPIO 21
- SCL: GPIO 22

`src/imu.cpp` wakes the MPU6050 using register `0x6B` and reads acceleration from `0x3B`, matching the direct-register test sketch. At startup, it averages 64 samples spaced 10 ms apart to establish the resting acceleration and noise. Keep the IMU still during startup calibration. The jerk threshold is the greater of 16,000 raw counts and four times measured calibration noise. The alert state lasts 100 ms and is refreshed while acceleration remains above threshold. Serial diagnostics print acceleration, delta, threshold, and HIGH/LOW state every 250 ms. This is an experimental motion trigger, not a medical or safety-certified measurement.

The MAX30102 uses SparkFun's MAX3010x library and runs on the same `Wire` bus. Its configured LED mode reads red and IR, with 100 Hz sensor sampling and an average of four samples. Firmware reads it every 40 ms. `src/heart_rate.cpp` considers IR values below 10,000 to mean no contact, detects local peaks with a 100-count rise/fall threshold, accepts beat intervals from 300 to 1,500 ms, and averages up to four BPM values. These are starting thresholds: forehead fit, pressure, ambient light, and the specific breakout can require adjustment. BPM is an experimental estimate, not a clinical reading.

The main loop services the web server continuously, updates the IMU every 50 ms, samples heart rate every 40 ms, advances the cars every 40 ms, redraws the TFT every 100 ms, and publishes dashboard telemetry every 100 ms. Buzzer state checks continue between display redraws.

## Display and Buzzer Behavior

The ST7735 uses hardware SPI in landscape orientation, then applies an ST7735 memory-access setting to mirror the image left-to-right. Display signal pins:

| ST7735 signal | ESP32 GPIO |
| --- | ---: |
| CS | 5 |
| DC / A0 | 2 |
| RST | 4 |
| SCK / SCL | 18 |
| SDA / MOSI | 23 |

The display draws a track, a moving red car, a faster moving blue car, a fixed blue car at screen coordinate `(104, 73)`, and the HUD. Car positions are simulated; they do not come from the IMU.

Two buzzer outputs are active HIGH in the firmware:

| Buzzer | ESP32 GPIO | Trigger |
| --- | ---: | --- |
| Right | 13 | Red car reaches the fixed blue car, or IMU jerk alert |
| Left | 27 | Faster blue car approaches the red car from screen-left, or IMU jerk alert |

Car-triggered buzzer pulses last 100 ms. The left pass alert fires once on approach and rearms after the cars separate beyond the configured rearm distance. The fixed-car ping fires on entering its contact radius. An IMU jerk alert activates both buzzer outputs. These GPIO outputs are intended for compatible low-current buzzers; use transistor drivers if the buzzer current exceeds safe GPIO limits.

## Wiring

All components need a shared ground with the ESP32. Check the voltage requirements of the specific breakout boards; do not assume every bare sensor accepts 5 V.

| Device signal | ESP32 connection |
| --- | --- |
| MPU6050 SDA / SCL | GPIO 21 / GPIO 22 |
| MAX30102 SDA / SCL | GPIO 21 / GPIO 22 |
| MAX30102 INT | Not used |
| Right buzzer + | GPIO 13 |
| Left buzzer + | GPIO 27 |
| Buzzer − | GND |

The MPU6050 and MAX30102 are separate I2C devices sharing SDA/SCL. Keep the I2C wiring short and use 3.3 V-compatible logic. If ESP32 `Wire.cpp` reports repeated I2C read errors, check shared ground, power, SDA/SCL connections, and logic levels; test by disconnecting one sensor at a time while powered off.

## Dependencies

PlatformIO installs the dependencies listed in `platformio.ini`, including Adafruit GFX/ST7735, Electronic Cats MPU6050, SparkFun MAX3010x, and Links2004 WebSockets. The ESP32 Arduino framework supplies `WiFi`, `WebServer`, `Wire`, and SPI.
