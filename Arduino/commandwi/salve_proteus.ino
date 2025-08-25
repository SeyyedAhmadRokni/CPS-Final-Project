#include <Wire.h>
#include <avr/sleep.h>
#include <SoftwareSerial.h>

// ===================== Pin Configuration =====================
#define LED_PIN1 7                 
#define LED_PIN2 8                 
#define WAKE_PIN 2                 
#define WAKE_COMMAND_PIN 3                 
#define BUTTON_PIN 5               // Simulates object detection
#define WAKE_NEXT_NODE_PIN 6       

#define SOFT_TX 11                 
#define SOFT_RX 10                 

SoftwareSerial ss(SOFT_RX, SOFT_TX); 

// ===================== State Variables =======================
volatile bool wakeFlag = false;  // Set by interrupt when WAKE_PIN is triggered
volatile bool wakeCommandFlag = false;  // Set by interrupt when WAK is triggered
char receivedCommand = ' ';      // Command sent by the previous node ('0', '1', '2')
bool isAwake = false;            // Flag to track if the node is active

// ===================== Metrics Logging =======================
unsigned long t_sleep_enter = 0;
unsigned long t_wake_start, t_wake_end;
unsigned long t_wake_command_start;
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

void applyLED1(bool on) {
  digitalWrite(LED_PIN1, on ? HIGH : LOW);
  logEvent(on ? "🌙 LED1 ON (DIM mode)" : "🔌 LED1 OFF");
}

void applyLED2(bool on) {
  digitalWrite(LED_PIN2, on ? HIGH : LOW);
  logEvent(on ? "💡 LED2 ON (Car mode)" : "⏹️ LED2 OFF");
}

void goToSleep() {
  logEvent("💤 Entering sleep mode");
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();
  t_sleep_enter = millis();
  sleep_mode();    // MCU sleeps here until interrupt occurs
  sleep_disable();
  logEvent("💤 Exited sleep mode");
}

void wakeUpISR() {
  wakeFlag = true;
  wakeup_count++;
  t_wake_start = millis();
  logEvent("🔔 ISR triggered! WAKE_PIN state: " + String(digitalRead(WAKE_PIN)));
}

void wakeCommandUpISR() {
  wakeCommandFlag = true;
  wakeup_count++;
  t_wake_command_start = millis();
  logEvent("🔔 COMMAND ISR triggered! WAKE_PIN state: " + String(digitalRead(WAKE_COMMAND_PIN)));
}

bool isButtonPressed() {
  logEvent("Checking button...");
  if (digitalRead(BUTTON_PIN) == LOW) { // Active low for button press
    delay(20);                          // Debounce
    if (digitalRead(BUTTON_PIN) == LOW) {
      logEvent("🚗 Button pressed (car detected)");
      return true;
    }
  }
  return false;
}

void sendCommandToNext(char cmd) {
  logEvent("📤 Sending command to next slave: " + String(cmd));
  ss.print(cmd);
  ss.flush();
  delay(50);  // Ensure transmission
}

void wakeNext() {
  logEvent("🔔 Waking next slave...");
  digitalWrite(WAKE_NEXT_NODE_PIN, LOW);  // LOW level for next slave
  logEvent("Wake signal sent (LOW level), WAKE_NEXT_NODE_PIN state: " + String(digitalRead(WAKE_NEXT_NODE_PIN)));
}

void endWakeNext() {
  digitalWrite(WAKE_NEXT_NODE_PIN, HIGH);  // Reset to HIGH
  logEvent("Wake signal ended for next (back to HIGH)");
}

void setup() {
  Serial.begin(9600);
  logEvent("Serial initialized");
  ss.begin(9600);
  logEvent("SoftwareSerial initialized");
  pinMode(LED_PIN1, OUTPUT);
  pinMode(LED_PIN2, OUTPUT);
  pinMode(WAKE_PIN, INPUT_PULLUP);
  pinMode(WAKE_COMMAND_PIN, INPUT_PULLUP);
  pinMode(WAKE_NEXT_NODE_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  digitalWrite(WAKE_NEXT_NODE_PIN, HIGH);  // Initial HIGH
  digitalWrite(LED_PIN1, LOW);
  digitalWrite(LED_PIN2, LOW);
  logEvent("Pin modes set: WAKE_PIN state = " + String(digitalRead(WAKE_PIN)));

  logEvent("Testing interrupt setup...");
  attachInterrupt(digitalPinToInterrupt(WAKE_PIN), wakeUpISR, LOW);  // Changed to LOW for power-down mode
  attachInterrupt(digitalPinToInterrupt(WAKE_COMMAND_PIN), wakeCommandUpISR, LOW);  // Changed to LOW for power-down mode
  logEvent("Interrupt attached, initial WAKE_PIN state: " + String(digitalRead(WAKE_PIN)));

  logEvent("Slave ready and entering sleep...");
  goToSleep();  // Start in sleep mode
}

void loop() {
  if (wakeFlag || wakeCommandFlag) {
    wakeFlag = false;
    wakeCommandFlag = false;
    t_wake_end = millis();
    logEvent("🔔 Woken up by interrupt, WAKE_PIN state: " + String(digitalRead(WAKE_PIN)));
    total_latency += (t_wake_end - t_sleep_enter);
    isAwake = true;

    // Wait for command from previous node
    unsigned long startWait = millis();
    logEvent("Waiting for command, initial SOFT_RX state: " + String(digitalRead(SOFT_RX)));
    while (!ss.available() && (millis() - startWait < 4000)) {
      delay(10);
    }

    if (ss.available()) {
      receivedCommand = ss.read();
      logEvent("📥 Received command: " + String(receivedCommand));
    } else {
      logEvent("⌛ Timeout waiting for command, SOFT_RX state: " + String(digitalRead(SOFT_RX)));
      receivedCommand = ' ';  // Invalid
    }

    // Process command
    if (receivedCommand == '1' || (!wakeCommandFlag && wakeFlag)) {  // Propagate LED1 ON (night/rain)
      applyLED1(true);
      wakeNext();
      delay(100);
      sendCommandToNext('1');
      delay(100);
      endWakeNext();
      isAwake = false;

    } else if (receivedCommand == '0' || (wakeCommandFlag && !wakeFlag)) {  // Propagate LED1 OFF (day/no rain)
      applyLED1(false);
      wakeNext();
      delay(100);
      sendCommandToNext('0');
      delay(100);
      endWakeNext();
      isAwake = false;

    } else if (receivedCommand == '2' || (wakeCommandFlag && wakeFlag)) {  // Car detection mode: Turn on LED2, wait for button
      applyLED2(true);
      bool buttonHandled = false;

      while (!buttonHandled) {
        if (isButtonPressed()) {
          buttonHandled = true;
          wakeNext();
          delay(100);
          sendCommandToNext('2');
          delay(2000);  // Wait 2 seconds
          applyLED2(false);
          endWakeNext();
        }
        delay(10);
      }
      isAwake = false;

    } else {
      logEvent("❓ Invalid command received: " + String(receivedCommand));
    }

    event_count++;
    if (event_count >= 10) {
      logEvent("Average Latency (ms): " + String(total_latency / event_count));
      logEvent("Wakeup Count: " + String(wakeup_count));
      total_latency = 0;
      event_count = 0;
      wakeup_count = 0;
    }
  }

  // Sleep if not awake
  if (!isAwake) {
    goToSleep();
  }
}