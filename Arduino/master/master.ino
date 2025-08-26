#include <Wire.h>
#include <DHT.h>
#include <SoftwareSerial.h>
#include <AP3216_WE.h>
#include "Adafruit_VL53L0X.h"

// =========== Pin Definitions ============
#define DHTPIN 12
#define DHTTYPE DHT11
#define LED_PIN1 7
#define LED_PIN2 8
#define DISTANCE_PIN 5 
#define WAKE_SLAVE_PIN 6
#define DIST_THRESHOLD_MM 1000 // Max detection distance in millimeters
#define SOFT_TX 11
#define SOFT_RX 10
#define AP3216_ADDR 0x1E  
DHT dht(DHTPIN, DHTTYPE);

SoftwareSerial ss(SOFT_RX, SOFT_TX);
// Logs to calculate metrics
unsigned long t_event_start, t_event_end;
unsigned long t_comm_start, t_comm_end;
unsigned long t_apply_start, t_apply_end;
unsigned long total_latency = 0;
unsigned long total_wakeup = 0;
unsigned long total_mA = 0; // Track current (simplified)
int event_count = 0;
unsigned long last_polling_time = 0;
unsigned long polling_window_size = 0; // Window size of polling in milliseconds
// ================== Polling Window Size Calculation ==================
float v_max = 0.25;                            // Max object speed in m/s
float L_zone = 0.25;                           // Detection zone length in meters
unsigned long t_read = 0;                      // Time to read sensor (milliseconds)
unsigned long t_decide = 0;                    // Time for CPU decision (milliseconds)
unsigned long t_apply = 0;                     // Time to apply output (milliseconds)
unsigned long t_ISR = 0;                       // Worst-case interrupt delay (milliseconds)
unsigned long t_margin = 15;                   // Safety margin for jitter/noise (milliseconds)
unsigned long t_obj = (L_zone / v_max) * 1000; // Object presence time in milliseconds

// State tracking
bool wasActive = false; // Track if night/rain was previously active
bool isActive = false;  // Current night/rain state

uint16_t readLightRaw()
{
    Wire.beginTransmission(AP3216_ADDR);
    Wire.write(0x0C);
    Wire.endTransmission();

    Wire.requestFrom(AP3216_ADDR, 2);
    if (Wire.available() < 2)
        return 0;
    
    uint8_t low = Wire.read();
    uint8_t high = Wire.read();
    uint16_t lux = ((uint16_t)high << 8) | low;
    return lux;
}
bool isNight()
{
    logEvent("Checking light...");
    uint16_t lux = readLightRaw();
    logEvent("Ambient light (raw): " + String(lux) + " lux");
    return lux < 5;
}
bool isRainy() {
    logEvent("Checking humidity...");
    float hum = dht.readHumidity();
    if (isnan(hum)) {
        logEvent("Failed to read humidity");
        return false;
    }
    logEvent("Humidity: " + String(hum) + " %");
    return hum > 50.0;
}
bool isObjectDetected() {
    logEvent("Checking distance...");
    uint16_t duration = pulseIn(DISTANCE_PIN, HIGH, 60000);
    if (duration == 0) {
        logEvent("Distance sensor timeout");
        return false;
    }
    uint16_t distance = duration / 10; // Convert to millimeters
    logEvent("Distance read: " + String(distance) + " mm");
    return distance > 0 && distance < DIST_THRESHOLD_MM;
}
void wakeSlave() {
    logEvent("Waking slave...");
    digitalWrite(WAKE_SLAVE_PIN, LOW); // Set to LOW and keep it low for LOW level interrupt
    logEvent("Wake signal sent (LOW level), WAKE_SLAVE_PIN state: " + String(digitalRead(WAKE_SLAVE_PIN)));
    total_wakeup++;
}
void endWakeSlave() {
    digitalWrite(WAKE_SLAVE_PIN, HIGH); // Reset to HIGH after command is sent
    logEvent("Wake signal ended (back to HIGH)");
}
void sendCommandToSlave(char cmd) {
    t_comm_start = millis();
    logEvent("Sending command to slave: " + String(cmd));
    for (int i = 0; i < 1; i++)
    { // Retry up to 3 times
        wakeSlave();
        delay(20); // Increased delay for slave to fully wake and prepare Serial
        endWakeSlave();
        //logEvent("SOFT_TX state before send: " + String(digitalRead(SOFT_TX)));
        ss.print(cmd);
        ss.flush(); // Ensure data is sent
        delay(100); // Wait for transmission
        logEvent("Command attempt " + String(i + 1) + ": " + String(cmd));
        delay(200); // Delay between retries
    }
    t_comm_end = millis();
    total_latency += (t_comm_end - t_comm_start);
    event_count++;
    logEvent("Command sent to slave over SoftwareSerial");
}
unsigned long calculatePollingWindowSize() {
    unsigned long start_read = millis();
    uint16_t lux = readLightRaw();
    unsigned long end_read = millis();
    t_read = end_read - start_read;
    unsigned long start_decision = millis();
    isActive = isNight() || isRainy();
    unsigned long end_decision = millis();
    t_decide = end_decision - start_decision;
    unsigned long start_apply = millis();
    digitalWrite(LED_PIN2, HIGH); // Dummy apply on LED_PIN2 to avoid blinking LED1
    digitalWrite(LED_PIN2, LOW);
    unsigned long end_apply = millis();
    t_apply = end_apply - start_apply;
    unsigned long start_ISR = millis();
    unsigned long end_ISR = millis();
    t_ISR = end_ISR - start_ISR;
    unsigned long T_poll_max = t_obj - (t_read + t_decide + t_apply + t_ISR) - t_margin;
    return T_poll_max;
}
void logEvent(String message) {
    Serial.print("[LOG @ ");
    Serial.print(millis());
    Serial.print(" ms] ");
    Serial.println(message);
}
void applyLED1(bool on) {
    digitalWrite(LED_PIN1, on ? HIGH : LOW);
    logEvent(on ? "Master LED1 ON (night/rain)" : "🔌 Master LED1 OFF");
}
void applyLED2(bool on) {
    digitalWrite(LED_PIN2, on ? HIGH : LOW);
    logEvent(on ? "Master LED2 ON (car)" : "⏹️ Master LED2 OFF");
}
void initAP3216() {
  Wire.beginTransmission(AP3216_ADDR);
  Wire.write(0x00);      // رجیستر کنترل
  Wire.write(0x03);      // فعال کردن ALS + PS + IR
  Wire.endTransmission();

  delay(100);  // فرصت راه‌اندازی به سنسور بده
}
void setup() {
    Serial.begin(9600);
    logEvent("Serial initialized");
    ss.begin(9600);
    logEvent("SoftwareSerial initialized");
    Wire.begin();
    logEvent("Wire initialized");
    // Initialize light sensor
    initAP3216();
    pinMode(LED_PIN1, OUTPUT);
    pinMode(LED_PIN2, OUTPUT);
    pinMode(DISTANCE_PIN, INPUT);
    pinMode(WAKE_SLAVE_PIN, OUTPUT);


    digitalWrite(WAKE_SLAVE_PIN, HIGH); // Initial HIGH
    logEvent("Pin modes set");
    dht.begin();
    logEvent("DHT initialized");
    logEvent("Master starting, waiting 5 seconds for slaves to initialize...");
    delay(5000); // Wait 10 seconds for slaves to start
    logEvent("Master Ready.");
}


void loop() {
    t_event_start = millis();
    unsigned long T_poll_max = calculatePollingWindowSize();
    logEvent("Calculated Maximum Polling Window Size (T_poll,max): " + String(T_poll_max) + " ms");
    isActive = isNight() || isRainy();
    logEvent("Environment check: night/rain active = " + String(isActive));
    if (isActive && !wasActive) {
        applyLED1(true);
        sendCommandToSlave('1');
        logEvent("Activated night/rain mode, propagated to slaves");
    }
    else if (!isActive && wasActive) {
        applyLED1(false);
        sendCommandToSlave('0');
        logEvent("Deactivated night/rain mode, propagated to slaves");
    }
    wasActive = isActive;
    if (isActive) {
        if (isObjectDetected()) {
            applyLED2(true);
            sendCommandToSlave('2');
            delay(2000);
            applyLED2(false);
            logEvent("Handled car detection, propagated to slaves");
        }
    }
    uint16_t lux = readLightRaw();
    Serial.println("Ambient light: " + String(lux) + " lux");
    t_event_end = millis();
    total_latency += (t_event_end - t_event_start);
    if (event_count >= 10) {
        logEvent("Average Latency (ms): " + String(total_latency / event_count));
        logEvent("Wakeup Rate: " + String(total_wakeup / event_count) + " per second");
        logEvent("Average mA: " + String(total_mA / event_count));
        total_latency = 0;
        total_wakeup = 0;
        total_mA = 0;
        event_count = 0;
    }
    logEvent("Loop finished");
    delay(2000); // Increased for lower power, but still responsive
}