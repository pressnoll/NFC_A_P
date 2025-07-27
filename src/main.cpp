#include <Arduino.h>

// Set the onboard LED pin number for most ESP32 boards
#define LED_PIN 2

void setup() {
  pinMode(LED_PIN, OUTPUT); // Initialize the LED pin as an output
}

void loop() {
  digitalWrite(LED_PIN, HIGH); // Turn the LED on
  delay(1000);                 // Wait for one second
  digitalWrite(LED_PIN, LOW);  // Turn the LED off
  delay(1000);                 // Wait for one second
}