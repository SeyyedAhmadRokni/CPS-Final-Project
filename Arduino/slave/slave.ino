#include <Wire.h>
#include "Adafruit_VL53L0X.h"
#include <avr/sleep.h>
#include <SoftwareSerial.h>

// ===================== Pin Configuration =====================
#define LED_PIN1 7                 
#define LED_PIN2 8                 
#define WAKE_PIN 2                 
#define WAKE_NEXT_NODE_PIN 6       

#define SOFT_RX 11                 
#define SOFT_TX 10                 

SoftwareSerial ss(SOFT_RX, SOFT_TX); 

// ===================== Lighting States =======================
#define STATE_OFF 0
#define STATE_ON 1
#define STATE_BLINK 2
#define STATE_DIM 3

Adafruit_VL53L0X lox = Adafruit_VL53L0X(); 

// ===================== State Variables =======================
volatile bool wakeFlag = false;  // Set by interrupt when WAKE_PIN is triggered
int receivedState = STATE_OFF;   // Lighting state sent by the Master
bool isAwake = false;            // Flag to track if the node is active

// ===================== Metrics Logging =======================
unsigned long t_wake_start, t_wake_end;
unsigned long t_state_start, t_state_end;
unsigned long total_latency = 0;
unsigned long wakeup_count = 0;
unsigned long total_mA = 0;  // Placeholder for current (mA)
int event_count = 0;

void logEvent(String message) {
  Serial.print("[LOG @ ");
  Serial.print(millis());
  Serial.print(" ms] ");
  Serial.println(message);
}

void initVL53L0X() {
  if (!lox.begin()) {
    logEvent("❌ Failed to initialize VL53L0X sensor");
    while (1); // Stop execution if sensor init fails
  }
}

void applyState(int state) {
  t_state_start = millis();
  switch (state) {
    case STATE_OFF:
      digitalWrite(LED_PIN1, LOW);
      digitalWrite(LED_PIN2, LOW);
      logEvent("🔌 STATE_OFF: LEDs OFF");
      break;

    case STATE_DIM:
      digitalWrite(LED_PIN1, HIGH);
      digitalWrite(LED_PIN2, LOW);
      logEvent("🌙 STATE_DIM: LED1 ON, LED2 OFF");
      break;

    case STATE_ON:
      digitalWrite(LED_PIN1, HIGH);
      digitalWrite(LED_PIN2, HIGH);
      logEvent("💡 STATE_ON: Both LEDs ON");
      break;

    case STATE_BLINK:
      logEvent("⚠️ STATE_BLINK: Not implemented");
      break;

    default:
      logEvent("❓ Unknown state: " + String(state));
      break;
  }
  t_state_end = millis();
  total_latency += (t_state_end - t_state_start); // Log state application time
}

void goToSleep() {
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();
  sleep_mode();    // MCU sleeps here until interrupt occurs
  sleep_disable();
}

void wakeUpISR() {
  t_wake_start = millis();
  wakeFlag = true;
  wakeup_count++;
}

void setup() {
  Serial.begin(9600);
  pinMode(LED_PIN1, OUTPUT);
  pinMode(LED_PIN2, OUTPUT);
  pinMode(WAKE_PIN, INPUT);
  pinMode(WAKE_NEXT_NODE_PIN, OUTPUT);
  digitalWrite(WAKE_NEXT_NODE_PIN, LOW);

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
    t_wake_end = millis();
    logEvent("🔔 Woken up by interrupt");

    // Log wakeup time (latency)
    total_latency += (t_wake_end - t_wake_start); 

    // Wait for state data from Master
    while (ss.available() == 0);
    receivedState = ss.readStringUntil('\n').toInt();
    logEvent("📥 Received state: " + String(receivedState));

    isAwake = true;
  }

  if (isAwake) {
    unsigned long startTime = millis();
    bool objectDetected = false;

    while (millis() - startTime < 10000) { // حداکثر 10 ثانیه بیدار باشه
        VL53L0X_RangingMeasurementData_t measure;
        lox.rangingTest(&measure, false);

        if (measure.RangeStatus != 4 && measure.RangeMilliMeter < 200) {
            objectDetected = true;
            break;
        }
        delay(50); // کمی تاخیر برای جلوگیری از فشار به CPU
    }

    if (objectDetected) {
        // همان کاری که الان برای جسم انجام میدی
        applyState(STATE_ON);
        delay(500);
        digitalWrite(WAKE_NEXT_NODE_PIN, HIGH);
        delay(50);
        digitalWrite(WAKE_NEXT_NODE_PIN, LOW);
        logEvent("➡️ Sent wake signal to next node");
        delay(1500);
    } else {
        logEvent("🚫 No object detected within timeout");
    }

    applyState(receivedState);
    logEvent("↩️ Returned to previous state");
    isAwake = false;
}

  goToSleep();

  if (wakeup_count >= 10) {
    logEvent("Average Latency (ms): " + String(total_latency / wakeup_count));
    logEvent("Wakeup Rate: " + String(wakeup_count / 10) + " per second");
    logEvent("Average mA: " + String(total_mA / wakeup_count));
    total_latency = 0;
    wakeup_count = 0;
    total_mA = 0;
  }
}
