#include <Wire.h>
#include <VL53L0X.h>  // برای سنسور فاصله VL53L0X

VL53L0X distanceSensor;

const int ledPin = 7;
String inputBuffer = "";
int currentState = 0;

void setup() {
  Serial.begin(9600);
  Wire.begin();

  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);

  distanceSensor.setTimeout(500);
  if (!distanceSensor.init()) {
    Serial.println("Sensor init failed");
    while (1);
  }
  distanceSensor.startContinuous();

  Serial.println("Slave ready (2-bit mode).");
}

void loop() {
  // 1. دریافت عدد ۲ بیتی (بین 0 تا 3) از برد قبلی
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n') {
      handleInput(inputBuffer);
      inputBuffer = "";
    } else {
      inputBuffer += c;
    }
  }

  // 2. هر 1 ثانیه فاصله‌سنج بخونه و ارسال کنه
  static unsigned long lastRead = 0;
  if (millis() - lastRead > 1000) {
    lastRead = millis();
    int distance = distanceSensor.readRangeContinuousMillimeters();
    if (!distanceSensor.timeoutOccurred()) {
      Serial.print("DIST:");
      Serial.println(distance);  // Propagate فاصله به برد بعدی
    } else {
      Serial.println("DIST:ERR");
    }
  }
}

// تابع برای تحلیل و اجرای دیتای دریافتی (۰ تا ۳)
void handleInput(String data) {
  data.trim();

  int value = data.toInt(); // فرض می‌گیریم فقط اعداد 0 تا 3 میان
  if (value >= 0 && value <= 3) {
    currentState = value;

    // اعمال استیت روی خروجی
    switch (currentState) {
      case 0:
        digitalWrite(ledPin, LOW);
        Serial.println("STATE 0: LED OFF");
        break;
      case 1:
        digitalWrite(ledPin, HIGH);
        Serial.println("STATE 1: LED ON");
        break;
      case 2:
        blinkLED();
        Serial.println("STATE 2: Blink");
        break;
      case 3:
        analogWrite(ledPin, 128); // PWM حالت نیمه‌روشن
        Serial.println("STATE 3: Dim");
        break;
    }

    // Propagate همون دیتا به برد بعدی
    Serial.println(value);
  } else {
    Serial.println("INVALID INPUT");
  }
}

// تابع کمکی برای چشمک زدن LED
void blinkLED() {
  for (int i = 0; i < 3; i++) {
    digitalWrite(ledPin, HIGH);
    delay(200);
    digitalWrite(ledPin, LOW);
    delay(200);
  }
}
