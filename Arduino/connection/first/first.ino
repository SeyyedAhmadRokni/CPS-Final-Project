#include <Arduino.h>
#include <avr/sleep.h>

const int rheostatPin = A0;
const int threshold = 600;

void setup() {
  Serial.begin(9600);  // USART TX on pin 1 (Arduino Uno)
}

void loop() {
  int val = analogRead(rheostatPin);

  if (val > threshold) {
    wakeUpChain();
  } else {
    goToSleep();
  }

  delay(200);
}

void wakeUpChain() {
  Serial.println(1);
  delay(200);  // simulate pulse length or delay
  delay(1000);
}

void goToSleep() {
  set_sleep_mode(SLEEP_MODE_IDLE);
  sleep_enable();
  sleep_mode();
  sleep_disable();
}
