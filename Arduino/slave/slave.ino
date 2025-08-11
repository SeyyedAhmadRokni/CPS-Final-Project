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
  wakeFlag = true;
}

// ===================== Setup =====================
void setup() {
  Serial.begin(9600);  
  ss.begin(9600);      

  pinMode(LED_PIN1, OUTPUT);
  pinMode(LED_PIN2, OUTPUT);
  pinMode(WAKE_PIN, INPUT);
  pinMode(WAKE_NEXT_NODE_PIN, OUTPUT);
  digitalWrite(WAKE_NEXT_NODE_PIN, LOW);

  // Attach interrupt to wake the node from sleep
  attachInterrupt(digitalPinToInterrupt(WAKE_PIN), wakeUpISR, RISING);

  // Initialize VL53L0X distance sensor
  if (!lox.begin()) {
    logEvent("❌ Failed to initialize VL53L0X sensor");
    while (1); // Stop execution if sensor init fails
  }

  // Ensure LEDs start OFF
  digitalWrite(LED_PIN1, LOW);
  digitalWrite(LED_PIN2, LOW);

  logEvent("✅ Slave initialized and going to sleep...");
}


// ===================== Power Management =====================
/**
 * @brief Puts the Arduino into idle sleep mode to save power
 */
void goToSleep() {
  set_sleep_mode(SLEEP_MODE_IDLE);
  sleep_enable();
  sleep_mode();    // MCU sleeps here until interrupt occurs
  sleep_disable();
}

// ===================== Lighting Control =====================
/**
 * @brief Sets LEDs according to a given lighting state
 */
void applyState(int state) {
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
}


// ===================== Main Loop =====================
void loop() {
  // If wake signal received from Master
  if (wakeFlag) {
    wakeFlag = false;
    logEvent("🔔 Woken up by interrupt");

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
    } 
    else {
      logEvent("🚫 No object detected");
      applyState(receivedState); // Maintain assigned state
    }

    // Go back to sleep after action
    isAwake = false;
    logEvent("😴 Going back to sleep...");
  }

  // Enter low-power idle mode
  goToSleep();
}
