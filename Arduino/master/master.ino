#include <Wire.h>
#include <Adafruit_VL53L0X.h>
#include <DHT.h>
#include <SoftwareSerial.h>
#include <AP3216_WE.h>

// ==== VL53L0X ====
Adafruit_VL53L0X lox = Adafruit_VL53L0X();
#define DIST_THRESHOLD_CM 100

// ==== DHT11 ====
#define DHTPIN 2
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// ==== AP3216C ====
#define AP3216_I2C_ADDR 0x1E  // Default I2C address
AP3216_WE lightSensor = AP3216_WE(AP3216_I2C_ADDR);

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

  // Initialize VL53L0X
  if (!lox.begin()) {
    Serial.println("VL53L0X not found!");
    while (1);
  }

  // Initialize AP3216 with AP3216_WE library
  lightSensor.init();
  
  // Configure sensor settings
  lightSensor.setLuxRange(RANGE_20661);  // Set to highest range
  lightSensor.setMode(AP3216_ALS);      // Ambient light sensing mode

  Serial.println("Master Ready.");
}

void loop() {
  // Distance measurement
  VL53L0X_RangingMeasurementData_t measure;
  lox.rangingTest(&measure, false);

  // Light measurement - changed to getAmbientLight()
  float lux = lightSensor.getAmbientLight();

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
      analogWrite(LED_PIN, 255);  // 100% brightness
    } else {
      analogWrite(LED_PIN, 0);    // Off during day
    }
  } else {
    if (isNight || hum > 80) {
      analogWrite(LED_PIN, 77);   // ~30% brightness
    } else {
      analogWrite(LED_PIN, 0);    // Off during day
    }
  }

  delay(500);
}