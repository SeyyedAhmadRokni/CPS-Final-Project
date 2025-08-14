#include <Wire.h>
#include <DHT.h>
#include <SoftwareSerial.h>
#include <AP3216_WE.h>

// =========== Pin Definitions ============
#define DHTPIN 12              // DHT11 sensor data pin
#define DHTTYPE DHT11          // DHT sensor type
#define LED_PIN1 7             // LED output pin 1
#define LED_PIN2 8             // LED output pin 2
#define DISTANCE_PIN 5         // Distance sensor signal pin
#define WAKE_SLAVE_PIN 6       // Pin to wake the slave node
#define DIST_THRESHOLD_MM 1000 // Max detection distance in millimeters

// Sensor & communication objects
DHT dht(DHTPIN, DHTTYPE);
AP3216_WE lightSensor = AP3216_WE(0x1E);  // Ambient light sensor I2C address
SoftwareSerial ss(10, 11); // RX, TX for slave communication

// Lighting states
#define STATE_OFF 0
#define STATE_ON  1
#define STATE_BLINK 2
#define STATE_DIM 3

// Logs to calculate metrics
unsigned long t_event_start, t_event_end;
unsigned long t_comm_start, t_comm_end;
unsigned long t_apply_start, t_apply_end;
unsigned long total_latency = 0;
unsigned long total_wakeup = 0;
unsigned long total_mA = 0;  // Track current (simplified)
int event_count = 0;

unsigned long last_polling_time = 0;
unsigned long polling_window_size = 0; // Window size of polling in milliseconds

// ================== Polling Window Size Calculation ==================
// Dynamic calculation of polling window size 

// Assumptions (These can be dynamically calculated or measured)
float v_max = 0.25;   // Max object speed in m/s (can be adjusted based on system requirements)
float L_zone = 0.25;  // Detection zone length in meters (can be dynamically measured if needed)

// Time assumptions (these can be adjusted as well based on actual system performance)
unsigned long t_read = 0;      // Time to read sensor (milliseconds) 
unsigned long t_decide = 0;    // Time for CPU decision (milliseconds) 
unsigned long t_apply = 0;     // Time to apply output (milliseconds)
unsigned long t_ISR = 0;       // Worst-case interrupt delay (milliseconds)
unsigned long t_margin = 15;   // Safety margin for jitter/noise (milliseconds)

// Calculate the object presence time in the sensor's detection zone (t_obj)
unsigned long t_obj = (L_zone / v_max) * 1000;  // in milliseconds

// Calculate maximum polling window size (T_poll,max)
unsigned long calculatePollingWindowSize() {
  
  // Calculate reading time dynamically (e.g., by measuring time to read the sensor)
  unsigned long start_read = millis();
  uint16_t lux = readLightRaw();
  unsigned long end_read = millis();
  t_read = end_read - start_read;  
  
  // Calculate decision time dynamically (e.g., time to decide based on logic)
  unsigned long start_decision = millis();
  int stateToSend = decideState();  
  unsigned long end_decision = millis();
  t_decide = end_decision - start_decision; 
  
  // Calculate apply time dynamically (e.g., time to apply the output to LEDs)
  unsigned long start_apply = millis();
  applyState(stateToSend); 
  unsigned long end_apply = millis();
  t_apply = end_apply - start_apply;  
  
  // Simulate interrupt delay (this would typically be measured in a real system)
  unsigned long start_ISR = millis();
  triggerISR(); 
  unsigned long end_ISR = millis();
  t_ISR = end_ISR - start_ISR;  

  // Calculate polling window size based on dynamic values
  unsigned long T_poll_max = t_obj - (t_read + t_decide + t_apply + t_ISR) - t_margin;
  return T_poll_max;
}

// ---------- Logging helper function ----------
void logEvent(String message) {
  Serial.print("[LOG @ ");
  Serial.print(millis());
  Serial.print(" ms] ");
  Serial.println(message);
}

// ---------- Setup routine ----------
void setup() {
  Serial.begin(9600);
  logEvent("Serial initialized");

  ss.begin(9600);  // For communication with slave node
  logEvent("SoftwareSerial initialized");

  Wire.begin();    // Start I2C bus
  logEvent("Wire initialized");

  // Configure I/O pins
  pinMode(LED_PIN1, OUTPUT);
  pinMode(LED_PIN2, OUTPUT);
  pinMode(DISTANCE_PIN, INPUT);
  pinMode(WAKE_SLAVE_PIN, OUTPUT);
  digitalWrite(WAKE_SLAVE_PIN, LOW);
  logEvent("Pin modes set");

  dht.begin(); // Start DHT11 temperature/humidity sensor
  logEvent("DHT initialized");

  logEvent("Master Ready.");
  delay(500);
}

// ---------- Read raw ambient light value from AP3216 ----------
uint16_t readLightRaw() {
  Wire.beginTransmission(0x1E);
  Wire.write(0x0C); // ALS data low byte register
  Wire.endTransmission();

  Wire.requestFrom(0x1E, 2);
  if (Wire.available() < 2) return 0;

  uint8_t low = Wire.read();
  uint8_t high = Wire.read();

  uint16_t lux = ((uint16_t)high << 8) | low;
  return lux;
}

// ---------- Simulate Decision Logic ----------
int decideState() {
  // Simulate decision-making logic
  return STATE_ON;  // For simplicity, just returning STATE_ON
}

// ---------- Simulate ISR trigger ----------
void triggerISR() {
  // Simulate an interrupt (no real effect in this context)
  delay(1);  // Simulating the time it takes for ISR handling
}

// ---------- Apply lighting state locally ----------
void applyState(int state) {
  // Simulate applying the state to LEDs
  switch (state) {
    case STATE_OFF:
      digitalWrite(LED_PIN1, LOW);
      digitalWrite(LED_PIN2, LOW);
      break;
    case STATE_DIM:
      digitalWrite(LED_PIN1, HIGH);
      digitalWrite(LED_PIN2, LOW);
      break;
    case STATE_ON:
      digitalWrite(LED_PIN1, HIGH);
      digitalWrite(LED_PIN2, HIGH);
      break;
    default:
      logEvent("Unknown state: " + String(state));
  }
}

// ---------- Main loop ----------
void loop() {
  t_event_start = millis();

  // Calculate polling window size (using the formula)
  unsigned long T_poll_max = calculatePollingWindowSize();
  logEvent("Calculated Maximum Polling Window Size (T_poll,max): " + String(T_poll_max) + " ms");

  // Read environmental and object detection data
  bool night = isNight();
  bool rainy = isRainy();
  bool carDetected = isObjectDetected();

  // Decision-making: choose lighting state based on conditions
  int stateToSend;
  if (night || rainy) {
    stateToSend = carDetected ? STATE_ON : STATE_DIM;
  } else {
    stateToSend = STATE_OFF;
  }

  logEvent("Decision made → State: " + String(stateToSend) +
           ", night: " + String(night) +
           ", rainy: " + String(rainy) +
           ", car: " + String(carDetected));

  // Send chosen state to slave node
  sendStateToSlave(stateToSend);

  // Apply lighting state locally
  if (carDetected && stateToSend == STATE_ON) {
    // Briefly turn on both LEDs, then dim for power saving
    applyState(STATE_ON);
    delay(1500);
    digitalWrite(LED_PIN1, HIGH);
    digitalWrite(LED_PIN2, LOW);
    logEvent("Reduced brightness: LED1 ON, LED2 OFF");
  } else {
    applyState(stateToSend);
  }

  // Log current ambient light
  uint16_t lux = readLightRaw();
  Serial.println("Ambient light: " + String(lux) + " lux");

  t_event_end = millis();
  total_latency += (t_event_end - t_event_start); // Log total event processing time
  
  // Log metrics every 10 events
  if (event_count >= 10) {
    logEvent("Average Latency (ms): " + String(total_latency / event_count));
    logEvent("Wakeup Rate: " + String(total_wakeup / event_count) + " per second");
    logEvent("Average mA: " + String(total_mA / event_count));
    total_latency = 0;
    total_wakeup = 0;
    total_mA = 0;
    event_count = 0;
  }

  logEvent("Loop finished\n");
  delay(5000); // Wait before next cycle
}
