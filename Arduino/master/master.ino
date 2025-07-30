#include <Wire.h>
#include <Adafruit_VL53L0X.h>
#include <DHT.h>
#include <SoftwareSerial.h>
#include "AP3216C.h"

// ==== VL53L0X ====
Adafruit_VL53L0X lox = Adafruit_VL53L0X();
#define DIST_THRESHOLD_CM 100

// ==== DHT11 ====
#define DHTPIN 2
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// ==== AP3216C ====
AP3216C lightSensor;

// ==== LED ====
#define LED_PIN 9

// ==== Serial to S1 ====
SoftwareSerial ss(10, 11); // RX, TX

void setup() {
  Serial.begin(9600);
  ss.begin(9600);

  pinMode(LED_PIN, OUTPUT);
  dht.begin();
  Wire.begin();

  if (!lox.begin()) {
    Serial.println("VL53L0X not found!");
    while (1);
  }

  if (!lightSensor.begin()) {
    Serial.println("AP3216C not found!");
    while (1);
  }

  Serial.println("Master Ready.");
}

void loop() {
  // Distance
  VL53L0X_RangingMeasurementData_t measure;
  lox.rangingTest(&measure, false);

  // Light
  float lux = lightSensor.readLux();

  // Temp & Humidity
  float temp = dht.readTemperature();
  float hum = dht.readHumidity();

  bool isNight = lux < 50;
  bool objectDetected = measure.RangeStatus == 0 && measure.RangeMilliMeter < DIST_THRESHOLD_CM * 10;

  if (objectDetected) {
    ss.println("WAKE");
    ss.print("LUX:");
    ss.println(lux);
    ss.print("TEMP:");
    ss.println(temp);
    ss.print("HUM:");
    ss.println(hum);

    if (isNight || hum > 80) {
      analogWrite(LED_PIN, 255);
    } else {
      analogWrite(LED_PIN, 0);
    }
  } else {
    if (isNight || hum > 80) {
      analogWrite(LED_PIN, 77); // حدود 30٪
    } else {
      analogWrite(LED_PIN, 0);
    }
  }

  delay(500);
}
