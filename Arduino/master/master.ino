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
unsigned long t_decide_start, t_decide_end;
unsigned long t_apply_start, t_apply_end;
unsigned long t_comm_start, t_comm_end;
unsigned long total_latency = 0;
unsigned long total_wakeup = 0;
unsigned long total_mA = 0;  // Track current (simplified)
int event_count = 0;

// ---------- Initialize the AP3216 light sensor ----------
void initAP3216() {
  Wire.beginTransmission(0x1E);
  Wire.write(0x00);   // Register: system configuration
  Wire.write(0x03);   // Power ON, ALS+PS+IR active
  Wire.endTransmission();

  delay(100);  // Allow sensor to stabilize
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
  initAP3216();
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

// ---------- Check if an object (e.g., a car) is detected ----------
bool isObjectDetected() {
  logEvent("Checking distance...");

  uint32_t timeout = 30000; // Timeout for pulseIn
  unsigned long duration = pulseIn(DISTANCE_PIN, HIGH, timeout);

  if (duration == 0) {
    logEvent("Distance sensor timeout");
    return false;
  }

  uint16_t distance = duration / 10; // Convert to millimeters
  logEvent("Distance read: " + String(distance) + " mm");

  return distance > 0 && distance < DIST_THRESHOLD_MM;
}

// ---------- Check if it's currently night ----------
bool isNight() {
  logEvent("Checking light...");
  uint16_t lux = readLightRaw();
  logEvent("Ambient light (raw): " + String(lux) + " lux");
  return lux < 5; // Low light threshold for night detection
}

// ---------- Check if it's rainy based on humidity ----------
bool isRainy() {
  logEvent("Checking humidity...");
  float hum = dht.readHumidity();
  logEvent("Humidity: " + String(hum) + " %");
  return hum > 50.0; // Humidity threshold for rain
}

// ---------- Send wake signal to slave node ----------
void wakeSlave() {
  logEvent("Waking slave...");
  digitalWrite(WAKE_SLAVE_PIN, HIGH);
  delay(50);
  digitalWrite(WAKE_SLAVE_PIN, LOW);
  logEvent("Wake signal sent to slave");
}

// ---------- Send current state to slave node ----------
void sendStateToSlave(int state) {
  t_comm_start = millis();
  logEvent("Sending state to slave: " + String(state));
  wakeSlave();
  delay(10); // Small delay before sending
  ss.println(state);
  t_comm_end = millis();
  total_latency += (t_comm_end - t_comm_start); // Log communication time
  event_count++;
  logEvent("State sent to slave over SoftwareSerial");
}

// ---------- Apply lighting state locally ----------
void applyState(int state) {
  t_apply_start = millis();
  logEvent("Applying state: " + String(state));
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
  t_apply_end = millis();
  total_latency += (t_apply_end - t_apply_start); // Log LED apply time
  logEvent("LED state applied.");
}

// ---------- Main loop ----------
void loop() {
  t_event_start = millis();

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
