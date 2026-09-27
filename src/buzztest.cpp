#include <Arduino.h>

const int buzzerPin = 13; // Pin connected to the buzzer

void setup()
{
    pinMode(buzzerPin, OUTPUT);
}

void loop()
{
    digitalWrite(buzzerPin, HIGH); // Turn on the buzzer

    delay(1000);
    digitalWrite(buzzerPin, LOW); // Turn off the buzzer
    delay(1000);
    }