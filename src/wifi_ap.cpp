#include <Arduino.h>
#include <WiFi.h>
#include "wifi_ap.h"

namespace {
constexpr char AP_SSID[] = "FTH-ESP32";
constexpr char AP_PASSWORD[] = "FTH-Connect-2026";
}

bool startAccessPoint() {
  WiFi.mode(WIFI_AP);

  if (!WiFi.softAP(AP_SSID, AP_PASSWORD, 1, false, 4)) {
    return false;
  }

  Serial.print("Wi-Fi network: ");
  Serial.println(AP_SSID);
  Serial.print("Connect to: ");
  Serial.println(WiFi.softAPIP());
  return true;
}