
#include <Arduino.h>
#include <avr/sleep.h>

const int inPin2 = 2;
const int inPin3 = 3;

volatile bool wakeFlag = false;

void wakeUpISR() {
  wakeFlag = true;
}

void setup() {
  pinMode(inPin2, INPUT);
  pinMode(inPin3, INPUT);

  Serial.begin(9600);
  attachInterrupt(digitalPinToInterrupt(inPin2), wakeUpISR, RISING);
}

void loop() {
  if (wakeFlag) {
    wakeFlag = false;

    int val2 = digitalRead(inPin2);
    int val3 = digitalRead(inPin3);

    if(val2){

      //////////
      // TODO //
      //////////


      Serial.println("Arduino waking up...");
      Serial.print("Input pin 2: ");
      Serial.print(val2);
      Serial.print(" | Input pin 3: ");
      Serial.println(val3);
    }

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