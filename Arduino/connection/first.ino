#include <Arduino.h>
#include <avr/sleep.h>

const int rheostatPin = A0;
const int outPin = 3;
const int threshold = 600;  // example threshold for resistance

void setup() {
  pinMode(outPin, OUTPUT);
  digitalWrite(outPin, LOW);
  Serial.begin(9600);
}

void loop() {
  int val = analogRead(rheostatPin);
  
  if (val > threshold) {
    wakeUpChain();
  }
  else {
    goToSleep();
  }
  
  delay(200);  // small delay
}

void wakeUpChain() {
  Serial.println("Rheostat threshold exceeded, waking chain.");
  digitalWrite(outPin, HIGH);
  delay(200);  // send pulse
  digitalWrite(outPin, LOW);
  delay(1000); // wait a bit before next check
}

void goToSleep() {
  Serial.println("Going to sleep...");
  set_sleep_mode(SLEEP_MODE_IDLE);  // light sleep mode, can change for deeper sleep
  sleep_enable();
  sleep_mode();
  // Arduino sleeps here until interrupt (e.g. from rheostat or other pin)
  sleep_disable();
}
