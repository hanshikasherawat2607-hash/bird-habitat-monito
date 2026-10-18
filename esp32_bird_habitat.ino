/**
 * ==============================================================================
 * Project: ESP32-Based Smart Bird Habitat Monitoring Node
 * Sensors: 
 *   - DHT22 (Temperature & Relative Humidity) on GPIO 4
 *   - BH1750 (Ambient Light Intensity in Lux) on I2C (GPIO 21 SDA, GPIO 22 SCL)
 * Microcontroller: ESP32 Dev Module / NodeMCU-32S
 * Communication: Wi-Fi HTTP POST JSON Telemetry
 * Target Endpoint: http://<SERVER_IP>:3000/api/readings
 * ==============================================================================
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <DHT.h>
#include <BH1750.h>

// ==============================================================================
// 1. NETWORK & SERVER CONFIGURATION
// ==============================================================================
// REPLACE with your local 2.4 GHz Wi-Fi credentials:
const char* WIFI_SSID     = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

// REPLACE with the IP address of the machine running the AviSense server:
// Find your PC's IP using 'ipconfig' (Windows) or 'ifconfig' (Mac/Linux).
const char* SERVER_URL    = "http://192.168.1.100:3000/api/readings";

// Unique Device Identifier for this aviary node:
const char* DEVICE_ID     = "ESP32-AVIARY-NODE-01";

// ==============================================================================
// 2. PIN DEFINITIONS & SENSOR INITIALIZATION
// ==============================================================================
#define DHTPIN        4       // GPIO 4 connected to DHT22 DATA pin
#define DHTTYPE       DHT22   // DHT 22 (AM2302, AM2321)
#define STATUS_LED    2       // Built-in blue LED for visual telemetry heartbeat

DHT dht(DHTPIN, DHTTYPE);
BH1750 lightMeter;

// Sampling interval in milliseconds (e.g., 5000ms = 5 seconds)
const unsigned long SAMPLING_INTERVAL_MS = 5000;
unsigned long lastSampleTime = 0;

bool bh1750Found = false;

// ==============================================================================
// 3. SETUP
// ==============================================================================
void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(STATUS_LED, OUTPUT);
  digitalWrite(STATUS_LED, LOW);

  Serial.println("\n=======================================================");
  Serial.println("🌿 AviSense ESP32 Smart Bird Habitat Telemetry Node");
  Serial.println("=======================================================");

  // 1. Initialize DHT22 Sensor
  Serial.println("[SENSOR] Initializing DHT22 (Temp & Humidity)...");
  dht.begin();

  // 2. Initialize I2C and BH1750 Light Sensor
  Serial.println("[SENSOR] Initializing I2C & BH1750 (Ambient Light Lux)...");
  Wire.begin(21, 22); // SDA = GPIO 21, SCL = GPIO 22

  if (lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE)) {
    Serial.println("[SENSOR] BH1750 light sensor initialized successfully.");
    bh1750Found = true;
  } else {
    Serial.println("[WARNING] BH1750 not detected. Check I2C wiring (SDA=21, SCL=22).");
  }

  // 3. Connect to Wi-Fi
  connectToWiFi();
}

// ==============================================================================
// 4. MAIN LOOP
// ==============================================================================
void loop() {
  // Ensure Wi-Fi remains connected
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WIFI] Connection lost. Reconnecting...");
    connectToWiFi();
  }

  unsigned long currentMillis = millis();
  if (currentMillis - lastSampleTime >= SAMPLING_INTERVAL_MS) {
    lastSampleTime = currentMillis;

    // Read Sensors
    float humidity = dht.readHumidity();
    float temperature = dht.readTemperature(); // Celsius
    float lux = 0.0;

    if (bh1750Found) {
      lux = lightMeter.readLightLevel();
      if (lux < 0) lux = 0.0;
    }

    // Check if DHT read failed
    if (isnan(humidity) || isnan(temperature)) {
      Serial.println("[ERROR] Failed to read from DHT22 sensor!");
      return;
    }

    // Print reading to Serial Console
    Serial.printf("\n[TELEMETRY] Temp: %.1f °C | Humidity: %.1f %% | Light: %.0f Lux | WiFi RSSI: %d dBm\n",
                  temperature, humidity, lux, WiFi.RSSI());

    // Send HTTP POST payload to server
    sendTelemetryToServer(temperature, humidity, lux);
  }
}

// ==============================================================================
// 5. WI-FI CONNECTION HELPER
// ==============================================================================
void connectToWiFi() {
  Serial.printf("[WIFI] Connecting to SSID: %s ", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 25) {
    delay(500);
    Serial.print(".");
    digitalWrite(STATUS_LED, !digitalRead(STATUS_LED)); // Toggle LED
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    digitalWrite(STATUS_LED, HIGH); // Solid ON when connected
    Serial.println("\n[WIFI] Connected successfully!");
    Serial.printf("[WIFI] Assigned IP Address: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("[WIFI] Signal Strength (RSSI): %d dBm\n", WiFi.RSSI());
  } else {
    digitalWrite(STATUS_LED, LOW);
    Serial.println("\n[WIFI] Failed to connect. Will retry during next cycle.");
  }
}

// ==============================================================================
// 6. HTTP POST TELEMETRY DISPATCHER
// ==============================================================================
void sendTelemetryToServer(float temperature, float humidity, float lux) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[HTTP] Cannot send: Wi-Fi disconnected.");
    return;
  }

  HTTPClient http;
  http.begin(SERVER_URL);
  http.addHeader("Content-Type", "application/json");

  // Construct JSON payload string
  char jsonBuffer[256];
  snprintf(jsonBuffer, sizeof(jsonBuffer),
    "{\"device_id\":\"%s\",\"temperature\":%.1f,\"humidity\":%.1f,\"light_lux\":%.0f,\"rssi\":%d}",
    DEVICE_ID, temperature, humidity, lux, WiFi.RSSI()
  );

  // Blink LED to indicate transmission
  digitalWrite(STATUS_LED, LOW);
  int httpResponseCode = http.POST(jsonBuffer);
  digitalWrite(STATUS_LED, HIGH);

  if (httpResponseCode > 0) {
    Serial.printf("[HTTP] POST Result Code: %d (Payload: %s)\n", httpResponseCode, jsonBuffer);
  } else {
    Serial.printf("[HTTP] POST Failed. Error: %s\n", http.errorToString(httpResponseCode).c_str());
  }

  http.end();
}
