#include <Wire.h>
#include <DHT.h>
#include <SoftwareSerial.h>
#include <AP3216_WE.h>

// =========== پین‌ها ============ 
#define DHTPIN 12
#define DHTTYPE DHT11
#define LED_PIN1 7
#define LED_PIN2 8
#define DISTANCE_PIN 5  
#define WAKE_SLAVE_PIN 6
#define DIST_THRESHOLD_MM 1000

DHT dht(DHTPIN, DHTTYPE);
AP3216_WE lightSensor = AP3216_WE(0x1E);
SoftwareSerial ss(10, 11); // RX, TX

#define STATE_OFF 0
#define STATE_ON  1
#define STATE_BLINK 2
#define STATE_DIM 3

void initAP3216() {
  Wire.beginTransmission(0x1E);
  Wire.write(0x00);      // رجیستر کنترل
  Wire.write(0x03);      // فعال کردن ALS + PS + IR
  Wire.endTransmission();

  delay(100);  // فرصت راه‌اندازی به سنسور بده
}

void logEvent(String message) {
  Serial.print("[LOG @ ");
  Serial.print(millis());
  Serial.print(" ms] ");
  Serial.println(message);
}

void setup() {
  Serial.begin(9600);
  logEvent("Serial initialized");

  ss.begin(9600);
  logEvent("SoftwareSerial initialized");

  Wire.begin();
  initAP3216();
  logEvent("Wire initialized");

  pinMode(LED_PIN1, OUTPUT);
  pinMode(LED_PIN2, OUTPUT);
  pinMode(DISTANCE_PIN, INPUT);
  pinMode(WAKE_SLAVE_PIN, OUTPUT);
  digitalWrite(WAKE_SLAVE_PIN, LOW);
  logEvent("Pin modes set");

  dht.begin();
  logEvent("DHT initialized");

  // lightSensor.init();  // این خط رو موقتاً کامنت کن
  logEvent("Skipped light sensor init for debug");

  logEvent("Master Ready.");
  delay(500);
}

uint16_t readLightRaw() {
  Wire.beginTransmission(0x1E);
  Wire.write(0x0C);  // رجیستر نور محیط
  Wire.endTransmission();

  Wire.requestFrom(0x1E, 2);
  if (Wire.available() < 2) return 0;

  uint8_t low = Wire.read();
  uint8_t high = Wire.read();

  uint16_t lux = ((uint16_t)high << 8) | low;
  return lux;
}

bool isObjectDetected() {
  logEvent("Checking distance...");

  // منتظر یک سیگنال پایدار باش
  uint32_t timeout = 30000;
  unsigned long duration = pulseIn(DISTANCE_PIN, HIGH, timeout);

  if (duration == 0) {
    logEvent("Distance sensor timeout");
    return false;
  }

  uint16_t distance = duration / 10;
  logEvent("Distance read: " + String(distance) + " mm");

  return distance > 0 && distance < DIST_THRESHOLD_MM;
}


bool isNight() {
  logEvent("Checking light...");
  uint16_t lux = readLightRaw();
  logEvent("Ambient light (raw): " + String(lux) + " lux");
  return lux < 5;
}

bool isRainy() {
  logEvent("Checking humidity...");
  float hum = dht.readHumidity();
  logEvent("Humidity: " + String(hum) + " %");
  return hum > 50.0;
}

void wakeSlave() {
  logEvent("Waking slave...");
  digitalWrite(WAKE_SLAVE_PIN, HIGH);
  delay(50);
  digitalWrite(WAKE_SLAVE_PIN, LOW);
  logEvent("Wake signal sent to slave");
}

void sendStateToSlave(int state) {
  logEvent("Sending state to slave: " + String(state));
  wakeSlave();
  delay(10);
  ss.println(state);
  logEvent("State sent to slave over SoftwareSerial");
}

void applyState(int state) {
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
}

void loop() {
  logEvent("Loop started");

  bool night = isNight();
  bool rainy = isRainy();
  bool carDetected = isObjectDetected();

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

  sendStateToSlave(stateToSend);

  if (carDetected && stateToSend == STATE_ON) {
    applyState(STATE_ON);
    delay(1500);
    digitalWrite(LED_PIN1, HIGH);
    digitalWrite(LED_PIN2, LOW);
    logEvent("Reduced brightness: LED1 ON, LED2 OFF");
  } else {
    applyState(stateToSend);
  }

  uint16_t lux = readLightRaw();
  Serial.println("Ambient light: " + String(lux) + " lux");


  logEvent("Loop finished\n");
  delay(5000);
}
