#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

// Sensors
const int rainPin = A0;    // Analog Rain Sensor
const int pirPin = 2;      // Digital PIR Sensor

// Switches
const int switch1 = 3;
const int switch2 = 4;

// nRF24L01
RF24 radio(9, 10); // CE, CSN
const byte address[6] = "00001";

// Variables
bool motionDetected = false;
int rainLevel = 0;

void setup() {
  Serial.begin(9600);

  pinMode(pirPin, INPUT);
  pinMode(switch1, INPUT_PULLUP);
  pinMode(switch2, INPUT_PULLUP);

  // Initialize nRF
  radio.begin();
  radio.openWritingPipe(address);
  radio.setPALevel(RF24_PA_LOW);
  radio.stopListening();  // Set to transmit mode

  Serial.println("System Ready.");
}

void loop() {
  // Read inputs
  motionDetected = digitalRead(pirPin);
  rainLevel = analogRead(rainPin);
  bool sw1 = !digitalRead(switch1); // with pull-up, pressed = LOW
  bool sw2 = !digitalRead(switch2);

  // Print to serial monitor
  Serial.print("Motion: ");
  Serial.print(motionDetected);
  Serial.print(" | Rain: ");
  Serial.print(rainLevel);
  Serial.print(" | SW1: ");
  Serial.print(sw1);
  Serial.print(" | SW2: ");
  Serial.println(sw2);

  // Simple decision example: send lighting command
  if (motionDetected || sw1 || sw2 || rainLevel < 400) {
    const char msg[] = "TURN_ON";
    radio.write(&msg, sizeof(msg));
    Serial.println("Command Sent: TURN_ON");
  } else {
    const char msg[] = "TURN_OFF";
    radio.write(&msg, sizeof(msg));
    Serial.println("Command Sent: TURN_OFF");
  }

  delay(500);
}
