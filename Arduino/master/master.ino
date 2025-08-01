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

// Potentiometer pin
const int potPin = A0;

const int lightPin = A1;


void setup() {
  Serial.begin(9600);
  ss.begin(9600);

  pinMode(LED_PIN, OUTPUT);
  pinMode(potPin, INPUT);
  pinMode(lightPin, INPUT);
  dht.begin();
  Wire.begin();
  Serial.println("Mellow!");

  // Initialize VL53L0X
  // if (!lox.begin()) {
  //   Serial.println("VL53L0X not found!");
  //   while (1);
  // }

  // Initialize AP3216 with AP3216_WE library
  // lightSensor.init();
  
  // Configure sensor settings
  // lightSensor.setLuxRange(RANGE_20661);  // Set to highest range
  // lightSensor.setMode(AP3216_ALS);      // Ambient light sensing mode

  Serial.println("Master Ready.");
}

bool detectingObject() {
  int potValue = analogRead(potPin);
  // Convert to voltage (0-5V)
  float voltage = potValue * (5.0 / 1023.0);
  Serial.print("Raw Value: ");
  Serial.print(potValue);
  Serial.print("\tVoltage: ");
  Serial.println(voltage);
  return voltage >= 2.5;

  //in main program return this

  // VL53L0X_RangingMeasurementData_t measure;
  // lox.rangingTest(&measure, false);
  // return measure.RangeStatus == 0 && measure.RangeMilliMeter < DIST_THRESHOLD_CM * 10;
}

void sendMessageToNext(){

}

float getLightStatus() {
  bool isNight = 1;
  
  // float lux = lightSensor.getAmbientLight();
  // isNight = lux < 50;

  float lux = analogRead(lightPin);
  Serial.print("LUX:");
  Serial.println(lux);
  isNight = lux > 930;
  return lux;
}

void loop() {

  bool isNight = getLightStatus();
  // lux = lightSensor.getAmbientLight();

  // // Temp & Humidity
  float temp;
  // temp = dht.readTemperature();

  float hum;
  // hum = dht.readHumidity();
  
  bool objectDetected = detectingObject();

  if (objectDetected) {
    ss.println("WAKE");
    
    ss.print("TEMP:");
    ss.println(temp);
    ss.print("HUM:");
    ss.println(hum);

    sendMessageToNext();

  //   if (isNight || hum > 80) {
  //     analogWrite(LED_PIN, 255);  // 100% brightness
  //   } else {
  //     analogWrite(LED_PIN, 0);    // Off during day
  //   }
  // } else {
  //   if (isNight || hum > 80) {
  //     analogWrite(LED_PIN, 77);   // ~30% brightness
  //   } else {
  //     analogWrite(LED_PIN, 0);    // Off during day
  //   }
  }

  delay(5000);
}