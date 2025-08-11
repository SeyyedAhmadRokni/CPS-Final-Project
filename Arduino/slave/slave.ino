#include <Wire.h>
#include "Adafruit_VL53L0X.h"
#include <avr/sleep.h>
#include <SoftwareSerial.h>

// ===================== Pin Configuration =====================
#define LED_PIN1 7                 // First LED output pin
#define LED_PIN2 8                 // Second LED output pin
#define WAKE_PIN 2                 // Pin used to wake this slave via interrupt
#define WAKE_NEXT_NODE_PIN 6       // Pin to send wake signal to the next node in the chain

#define SOFT_RX 11                 // SoftwareSerial RX pin
#define SOFT_TX 10                 // SoftwareSerial TX pin

// ===================== Communication =========================
SoftwareSerial ss(SOFT_RX, SOFT_TX); // Serial interface for receiving state from Master

// ===================== Lighting States =======================
#define STATE_OFF 0
#define STATE_ON 1
#define STATE_BLINK 2
#define STATE_DIM 3

// ===================== Sensor Object =========================
Adafruit_VL53L0X lox = Adafruit_VL53L0X(); // ToF distance sensor object

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

// ===================== Utility Functions =====================
/**
 * @brief Logs a message with timestamp for debugging
 */
void logEvent(String message) {
  Serial.print("[LOG @ ");
  Serial.print(millis());
  Serial.print(" ms] ");
  Serial.println(message);
}

/**
 * @brief Interrupt Service Routine to wake up the slave node
 */
void wakeUpISR() {
  t_wake_start = millis();
  wakeFlag = true;
  wakeup_count++;
}

/**
 * @brief Initializes the VL53L0X distance sensor
 */
void initVL53L0X() {
  if (!lox.begin()) {
    logEvent("❌ Failed to initialize VL53L0X sensor");
    while (1); // Stop execution if sensor init fails
  }
}

/**
 * @brief Apply state to LEDs and log the action
 */
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

/**
 * @brief Puts the Arduino into idle sleep mode to save power
 */
void goToSleep() {
  set_sleep_mode(SLEEP_MODE_IDLE);
  sleep_enable();
  sleep_mode();    // MCU sleeps here until interrupt occurs
  sleep_disable();
}

// ===================== Main Loop =====================
void loop() {
  // If wake signal received from Master
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

  // If node is awake, check surroundings
  if (isAwake) {
    VL53L0X_RangingMeasurementData_t measure;
    lox.rangingTest(&measure, false); // Read distance

    // If valid reading and object detected within 200 mm
    if (measure.RangeStatus != 4 && measure.RangeMilliMeter < 200) {
      logEvent("📏 Object detected at " + String(measure.RangeMilliMeter) + " mm");

      // Turn LEDs to full brightness temporarily
      applyState(STATE_ON);
      delay(500);

      // Wake the next node in the network
      digitalWrite(WAKE_NEXT_NODE_PIN, HIGH);
      delay(50);
      digitalWrite(WAKE_NEXT_NODE_PIN, LOW);
      logEvent("➡️ Sent wake signal to next node");

      // Maintain high brightness for a while
      delay(1500);  

      // Return to received state after detection
      if (receivedState == STATE_ON) {
        digitalWrite(LED_PIN1, HIGH);
        digitalWrite(LED_PIN2, LOW);
        logEvent("💡 Reduced brightness: LED1 ON, LED2 OFF");
      } else {
        applyState(receivedState);
        logEvent("↩️ Returned to previous state: " + String(receivedState));
      }
    } else {
      logEvent("🚫 No object detected");
      applyState(receivedState); // Maintain assigned state
    }

    // Go back to sleep after action
    isAwake = false;
    logEvent("😴 Going back to sleep...");
  }

  // Enter low-power idle mode
  goToSleep();

  // Log metrics every 10 wakeups
  if (wakeup_count >= 10) {
    logEvent("Average Latency (ms): " + String(total_latency / wakeup_count));
    logEvent("Wakeup Rate: " + String(wakeup_count / 10) + " per second");
    logEvent("Average mA: " + String(total_mA / wakeup_count));
    total_latency = 0;
    wakeup_count = 0;
    total_mA = 0;
  }
}
