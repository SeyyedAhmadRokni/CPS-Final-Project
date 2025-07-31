#include <Arduino.h>
#include <avr/sleep.h>

const int inPin2 = 2;   // Input pin 2
const int inPin3 = 3;   // Input pin 3
const int outPin0 = 0;  // Output pin 0 (Serial RX pin)
const int outPin1 = 1;  // Output pin 1 (Serial TX pin)

volatile bool wakeFlag = false;

void wakeUpISR() {
  wakeFlag = true;
}

void setup() {
  pinMode(inPin2, INPUT);
  pinMode(inPin3, INPUT);

  pinMode(outPin0, OUTPUT);
  pinMode(outPin1, OUTPUT);

  digitalWrite(outPin0, LOW);
  digitalWrite(outPin1, LOW);

  Serial.begin(9600);
  attachInterrupt(digitalPinToInterrupt(inPin2), wakeUpISR, RISING);
}

void loop() {
  if (wakeFlag) {
    wakeFlag = false;

    int val2 = digitalRead(inPin2);
    int val3 = digitalRead(inPin3);

    digitalWrite(outPin0, val2);
    digitalWrite(outPin1, val3);

    Serial.println("Arduino going to wake up...");
    Serial.print("Input pin 2: ");
    Serial.print(val2);
    Serial.print(" -> Output pin 0: ");
    Serial.print(val2);

    Serial.print(" | Input pin 3: ");
    Serial.print(val3);
    Serial.print(" -> Output pin 1: ");
    Serial.println(val3);

  } else {
    goToSleep();
  }
}

void goToSleep() {
  Serial.println("Arduino going to sleep...");

  set_sleep_mode(SLEEP_MODE_IDLE);
  sleep_enable();
  sleep_mode();
  sleep_disable();
}
