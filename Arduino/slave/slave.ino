#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

// nRF24L01 setup
RF24 radio(9, 10); // CE, CSN
const byte address[6] = "00001";

// Output device (e.g., LED or relay)
const int ledPin = 7;

void setup() {
  Serial.begin(9600);
  
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);

  // Initialize nRF24L01
  radio.begin();
  radio.openReadingPipe(0, address);
  radio.setPALevel(RF24_PA_LOW);
  radio.startListening(); // Set to receive mode

  Serial.println("Receiver Ready.");
}

void loop() {
  if (radio.available()) {
    char msg[32] = {0}; // Buffer for incoming message
    radio.read(&msg, sizeof(msg));

    Serial.print("Received: ");
    Serial.println(msg);

    // Act based on message
    if (strcmp(msg, "TURN_ON") == 0) {
      digitalWrite(ledPin, HIGH);
      Serial.println("Action: Device ON");
    } else if (strcmp(msg, "TURN_OFF") == 0) {
      digitalWrite(ledPin, LOW);
      Serial.println("Action: Device OFF");
    }
  }

  delay(100);
}
