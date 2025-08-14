#include <Wire.h>
#include "Adafruit_VL53L0X.h"
#include <avr/sleep.h>
#include <SoftwareSerial.h>

// ===================== Pin Configuration =====================
#define LED_PIN1 7                 
#define LED_PIN2 8                 
#define WAKE_PIN 2                 
#define DISTANCE_PIN 5  
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
unsigned long t_sleep_enter = 0;
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
  t_sleep_enter = millis();
  sleep_mode();    // MCU sleeps here until interrupt occurs
  sleep_disable();
}

void wakeUpISR() {
  wakeFlag = true;
  wakeup_count++;
}

bool isObjectDetected() {
  if (digitalRead(DISTANCE_PIN) == LOW) { // فعال با فشردن دکمه
    delay(20);                            // debounce
    return digitalRead(DISTANCE_PIN) == LOW;
  }
  return false;
}

void setup() {
  Serial.begin(9600);
  logEvent("Serial initialized");
  ss.begin(9600);
  logEvent("SoftwareSerial initialized");
  pinMode(LED_PIN1, OUTPUT);
  pinMode(LED_PIN2, OUTPUT);
  pinMode(WAKE_PIN, INPUT_PULLUP);
  pinMode(WAKE_NEXT_NODE_PIN, OUTPUT);
  // pinMode(DISTANCE_PIN, INPUT);
  pinMode(DISTANCE_PIN, INPUT_PULLUP);
  digitalWrite(WAKE_NEXT_NODE_PIN, HIGH);
  attachInterrupt(digitalPinToInterrupt(WAKE_PIN), wakeUpISR, LOW);


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
    total_latency += (t_wake_end - t_sleep_enter);

    // Wait for state data from Master
    unsigned long deadline = millis() + 500; // 500ms timeout
    while (ss.available() == 0 && millis() < deadline) { /* wait */ }

    if (ss.available()) {
      receivedState = ss.readStringUntil('\n').toInt();
      logEvent("📥 Received state: " + String(receivedState));
    } else {
      logEvent("⏱️ Timeout waiting for state; keep last: " + String(receivedState));
    }

    isAwake = true;
  }

  if (isAwake) {
    unsigned long startTime = millis();

  bool carDetected = false;
    while (millis() - startTime < 10000) { // حداکثر 10 ثانیه بیدار باشه
      carDetected = isObjectDetected();
      if (carDetected) {
          applyState(STATE_ON);
          delay(500);
          digitalWrite(WAKE_NEXT_NODE_PIN, LOW);
          delay(50);
          digitalWrite(WAKE_NEXT_NODE_PIN, HIGH);
          logEvent("➡️ Sent wake signal to next node");
          delay(1500);
          break;
      }
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