#include <Wire.h>
#include "Adafruit_VL53L0X.h"
#include <avr/sleep.h>

// پین‌ها
#define LED_PIN1 7
#define LED_PIN2 8
#define WAKE_PIN 2
#define WAKE_NEXT_NODE_PIN 3  // پین برای بیدار کردن نود بعدی

// وضعیت‌ها
#define STATE_OFF 0
#define STATE_ON  1
#define STATE_BLINK 2
#define STATE_DIM 3

Adafruit_VL53L0X lox = Adafruit_VL53L0X();
volatile bool wakeFlag = false;

int receivedState = STATE_OFF;
bool isAwake = false;

void logEvent(String message) {
  Serial.print("[LOG @ ");
  Serial.print(millis());
  Serial.print(" ms] ");
  Serial.println(message);
}

void wakeUpISR() {
  wakeFlag = true;
}

void setup() {
  Serial.begin(9600);
  pinMode(LED_PIN1, OUTPUT);
  pinMode(LED_PIN2, OUTPUT);
  pinMode(WAKE_PIN, INPUT);
  pinMode(WAKE_NEXT_NODE_PIN, OUTPUT);
  digitalWrite(WAKE_NEXT_NODE_PIN, LOW);  // آماده برای وقفه بعدی

  attachInterrupt(digitalPinToInterrupt(WAKE_PIN), wakeUpISR, RISING);

  if (!lox.begin()) {
    logEvent("Failed to boot VL53L0X");
    while (1);
  }

  digitalWrite(LED_PIN1, LOW);
  digitalWrite(LED_PIN2, LOW);

  logEvent("Slave ready and sleeping...");
}

void loop() {
  if (wakeFlag) {
    wakeFlag = false;
    logEvent("Woken up!");

    while (Serial.available() == 0);
    receivedState = Serial.readStringUntil('\n').toInt();
    logEvent("Received state: " + String(receivedState));

    isAwake = true;
  }

  if (isAwake) {
    VL53L0X_RangingMeasurementData_t measure;
    lox.rangingTest(&measure, false);

    if (measure.RangeStatus != 4 && measure.RangeMilliMeter < 200) {
      logEvent("Object detected! Distance: " + String(measure.RangeMilliMeter) + " mm");

      applyState(STATE_ON);  // روشنایی کامل
      delay(500);

      // بیدار کردن نود بعدی
      digitalWrite(WAKE_NEXT_NODE_PIN, HIGH);
      delay(50);
      digitalWrite(WAKE_NEXT_NODE_PIN, LOW);
      logEvent("Sent wake signal to next node");

      delay(1500);  // شبیه‌سازی عبور ماشین

      if (receivedState == STATE_ON) {
        // فقط LED1 روشن بمونه
        digitalWrite(LED_PIN1, HIGH);
        digitalWrite(LED_PIN2, LOW);
        logEvent("Reduced brightness: LED1 ON, LED2 OFF");
      } else {
        // بازگشت به حالت اصلی
        applyState(receivedState);
        logEvent("Returned to previous state: " + String(receivedState));
      }
    } else {
      logEvent("No object detected.");
      applyState(receivedState);
    }

    isAwake = false;
    logEvent("Going back to sleep...");
  }

  goToSleep();
}

void goToSleep() {
  set_sleep_mode(SLEEP_MODE_IDLE);
  sleep_enable();
  sleep_mode();
  sleep_disable();
}

void applyState(int state) {
  switch (state) {
    case STATE_OFF:
      digitalWrite(LED_PIN1, LOW);
      digitalWrite(LED_PIN2, LOW);
      logEvent("STATE_OFF: LEDs OFF");
      break;

    case STATE_DIM:
      digitalWrite(LED_PIN1, HIGH);
      digitalWrite(LED_PIN2, LOW);
      logEvent("STATE_DIM: LED1 ON, LED2 OFF");
      break;

    case STATE_ON:
      digitalWrite(LED_PIN1, HIGH);
      digitalWrite(LED_PIN2, HIGH);
      logEvent("STATE_ON: Both LEDs ON");
      break;

    case STATE_BLINK:
      // قابل گسترش در آینده
      logEvent("STATE_BLINK: Not implemented");
      break;

    default:
      logEvent("Unknown state: " + String(state));
      break;
  }
}
