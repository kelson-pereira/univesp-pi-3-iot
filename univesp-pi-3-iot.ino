/* Wi-Fi STA Connect and Disconnect Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.

*/
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#define DHT_PIN 25
#define DHT_TYPE DHT22
#define BUZZER_PIN 32
#define ONEWIRE_PIN 33

DHT dht(DHT_PIN, DHT_TYPE);
OneWire oneWire(ONEWIRE_PIN);
DallasTemperature sensors(&oneWire);

const char *ssid = "SSID";
const char *password = "PASSWD";

String ledStatusURL = "https://plantio-74e808a068fc.herokuapp.com/led/status/";
String ledToggleURL = "https://plantio-74e808a068fc.herokuapp.com/led/toggle/";

int btnGPIO = 0;
int btnState = false;
int LED_PIN = 2;
int RELAY1_PIN = 26;
int RELAY2_PIN = 27;

void setup() {
  Serial.begin(115200);
  delay(10);
  
  digitalWrite(RELAY1_PIN, HIGH);
  digitalWrite(RELAY2_PIN, HIGH);

  // Set GPIO0 Boot button as input
  pinMode(btnGPIO, INPUT);

  // Set LED as output
  pinMode(LED_PIN, OUTPUT);

  // Set BUZZER as output
  pinMode(BUZZER_PIN, OUTPUT);

  // Set RELAY as output
  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);

  // Init DHT22 sensor
  dht.begin();

  // Init DS18B20 sensor
  sensors.begin();

  // We start by connecting to a WiFi network
  // To debug, please enable Core Debug Level to Verbose

  Serial.println();
  Serial.print("[WiFi] Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);
  // Auto reconnect is set true as default
  // To set auto connect off, use the following function
  //    WiFi.setAutoReconnect(false);

  // Will try for about 10 seconds (20x 500ms)
  int tryDelay = 500;
  int numberOfTries = 20;

  // Wait for the WiFi event
  while (true) {
    switch (WiFi.status()) {
      case WL_NO_SSID_AVAIL: Serial.println("[WiFi] SSID not found"); break;
      case WL_CONNECT_FAILED:
        Serial.print("[WiFi] Failed - WiFi not connected! Reason: ");
        return;
        break;
      case WL_CONNECTION_LOST: Serial.println("[WiFi] Connection was lost"); break;
      case WL_SCAN_COMPLETED:  Serial.println("[WiFi] Scan is completed"); break;
      case WL_DISCONNECTED:    Serial.println("[WiFi] WiFi is disconnected"); break;
      case WL_CONNECTED:
        Serial.println("[WiFi] WiFi is connected!");
        Serial.print("[WiFi] IP address: ");
        Serial.println(WiFi.localIP());
        return;
        break;
      default:
        Serial.print("[WiFi] WiFi Status: ");
        Serial.println(WiFi.status());
        break;
    }
    delay(tryDelay);

    if (numberOfTries <= 0) {
      Serial.print("[WiFi] Failed to connect to WiFi!");
      // Use disconnect function to force stop trying to connect
      WiFi.disconnect();
      return;
    } else {
      numberOfTries--;
    }
  }
}

void loop() {
  // Read the button state
  btnState = digitalRead(btnGPIO);

  if (WiFi.status()== WL_CONNECTED) {
    HTTPClient http;
    JsonDocument json;
    Serial.println("Lendo DHT22...");
    float tmpA = dht.readTemperature();
    float umdA = dht.readHumidity();
    if (!isnan(tmpA)) {
      Serial.print("Temperatura: ");
      Serial.print(tmpA);
      Serial.println("°C");
    }
    if (!isnan(umdA)) {
      Serial.print("Umidade: ");
      Serial.print(umdA);
      Serial.println("%");
    }
    Serial.println("Lendo DS18B20...");
    sensors.requestTemperatures();
    float tmpS = sensors.getTempCByIndex(0);
    if (!isnan(tmpS)){
      Serial.print("Temperatura da solucao: ");
      Serial.print(tmpS);
      Serial.println("°C");
    }
    if (tmpS > 32.00) {
      tone(BUZZER_PIN, 1000);
      Serial.println("Temperatura muito elevada!!!");
      delay(1000);
      noTone(BUZZER_PIN);
    }
    if (btnState == LOW) {
      //// Disconnect from WiFi
      //Serial.println("[WiFi] Disconnecting from WiFi!");
      //// This function will disconnect and turn off the WiFi (NVS WiFi data is kept)
      //if (WiFi.disconnect(true, false)) {
      //  Serial.println("[WiFi] Disconnected from WiFi!");
      //}
      Serial.print("[WiFi] LED toogle: ");
      http.begin(ledToggleURL.c_str());
      int httpResponseCode = http.GET();
      if (httpResponseCode == 200) {
        Serial.println("OK");
      } else {
        Serial.print("Error code: ");
        Serial.println(httpResponseCode);
      }
      http.end();
      delay(5000);
    } else {
      Serial.print("[WiFi] LED status: ");
      http.begin(ledStatusURL.c_str());
      int httpResponseCode = http.GET();
      if (httpResponseCode == 200) {
        // Serial.print("HTTP Response code: ");
        // Serial.println(httpResponseCode);
        String payload = http.getString();
        // Serial.println(payload);
        deserializeJson(json, payload);
        bool status = json["status"];
        Serial.println(status);
        if (status == true) {
          digitalWrite(LED_PIN, HIGH);
          digitalWrite(RELAY1_PIN, LOW);
          digitalWrite(RELAY2_PIN, LOW);
        } else {
          digitalWrite(LED_PIN, LOW);
          digitalWrite(RELAY1_PIN, HIGH);
          digitalWrite(RELAY2_PIN, HIGH);
        }
      }
      else {
        Serial.print("Error code: ");
        Serial.println(httpResponseCode);
      }
      http.end();
      delay(5000);
    }
  }
}
