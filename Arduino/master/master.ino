#include <Wire.h>
#include <DHT.h>
#include <SoftwareSerial.h>
#include <AP3216_WE.h>

// =========== پین‌ها ============
#define DHTPIN 10
#define DHTTYPE DHT11
#define LED_PIN1 7
#define LED_PIN2 8
#define DISTANCE_PIN 5
#define WAKE_SLAVE_PIN 6
#define DIST_THRESHOLD_MM 1000 // آستانه فاصله (1000 میلی‌متر)

DHT dht(DHTPIN, DHTTYPE);
AP3216_WE lightSensor = AP3216_WE(0x1E);
SoftwareSerial ss(10, 11); // RX, TX

// وضعیت‌ها
#define STATE_OFF 0
#define STATE_ON  1
#define STATE_BLINK 2
#define STATE_DIM 3

void logEvent(String message) {
  Serial.print("[LOG @ ");
  Serial.print(millis());
  Serial.print(" ms] ");
  Serial.println(message);
}

void setup() {
  Serial.begin(9600);
  ss.begin(9600);
  Wire.begin();

  pinMode(LED_PIN1, OUTPUT);
  pinMode(LED_PIN2, OUTPUT);
  pinMode(DISTANCE_PIN, INPUT);
  pinMode(WAKE_SLAVE_PIN, OUTPUT);
  digitalWrite(WAKE_SLAVE_PIN, LOW);

  dht.begin();
  lightSensor.init();

  logEvent("Master Ready.");
  delay(500);
}

bool isObjectDetected() {
  unsigned long duration = pulseIn(DISTANCE_PIN, HIGH);
  uint16_t distance = duration / 10;
  logEvent("Distance measured: " + String(distance) + " mm");
  return distance > 0 && distance < DIST_THRESHOLD_MM;
}

bool isNight() {
  float lux = lightSensor.getAmbientLight();
  logEvent("Ambient light: " + String(lux) + " lux");
  return lux > 930;
}

bool isRainy() {
  float hum = dht.readHumidity();
  logEvent("Humidity: " + String(hum) + " %");
  return hum > 50.0;
}

void wakeSlave() {
  digitalWrite(WAKE_SLAVE_PIN, HIGH);
  delay(50);
  digitalWrite(WAKE_SLAVE_PIN, LOW);
  logEvent("Sent wake signal to slave");
}

void sendStateToSlave(int state) {
  wakeSlave(); // ابتدا اسلیو را بیدار کن
  delay(10);   // کمی تأخیر برای آماده‌سازی
  ss.println(state);
  logEvent("Sent state to slave: " + String(state));
}

void applyState(int state) {
  switch (state) {
    case STATE_OFF:
      digitalWrite(LED_PIN1, LOW);
      digitalWrite(LED_PIN2, LOW);
      logEvent("Applied STATE_OFF: LEDs OFF");
      break;

    case STATE_DIM:
      digitalWrite(LED_PIN1, HIGH);
      digitalWrite(LED_PIN2, LOW);
      logEvent("Applied STATE_DIM: LED1 ON, LED2 OFF");
      break;

    case STATE_ON:
      digitalWrite(LED_PIN1, HIGH);
      digitalWrite(LED_PIN2, HIGH);
      logEvent("Applied STATE_ON: Both LEDs ON");
      break;

    default:
      logEvent("Unknown state");
  }
}

void loop() {
  logEvent("Loop start");

  bool night = isNight();
  bool rainy = isRainy();
  bool carDetected = isObjectDetected();

  int stateToSend;
  if (night || rainy) {
    stateToSend = carDetected ? STATE_ON : STATE_DIM;
  } else {
    stateToSend = STATE_OFF;
  }

  sendStateToSlave(stateToSend);

  if (carDetected && stateToSend == STATE_ON) {
    // چراغ‌ها کاملاً روشن
    applyState(STATE_ON);
    delay(1500);  // عبور ماشین

    // کاهش روشنایی
    digitalWrite(LED_PIN1, HIGH);
    digitalWrite(LED_PIN2, LOW);
    logEvent("Reduced brightness: LED1 ON, LED2 OFF");
  } else {
    // در غیر این صورت، وضعیت عادی
    applyState(stateToSend);
  }

  logEvent("Loop end");
  delay(5000);
}
