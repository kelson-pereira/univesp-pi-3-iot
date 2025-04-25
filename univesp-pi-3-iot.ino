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

#ifdef ESP32
  #include "esp32-hal-ledc.h"
#endif

#define DHT_PIN 14
#define DHT_TYPE DHT22
#define ONEWIRE_PIN 26
#define BUZZER_PIN 25

DHT dht(DHT_PIN, DHT_TYPE);
OneWire oneWire(ONEWIRE_PIN);
DallasTemperature sensors(&oneWire);

const char *ssid = "SSID";
const char *password = "PASSWD";

String postUpdateURL = "https://plantio-74e808a068fc.herokuapp.com/update/";

int btnGPIO = 0;
int btnState = false;
int LED_PIN = 2;
int RELAY1_PIN = 32;
int RELAY2_PIN = 33;

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
      digitalWrite(LED_PIN, LOW);
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
  if (WiFi.status() == WL_CONNECTED) {
    digitalWrite(LED_PIN, HIGH);
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
    http.begin(postUpdateURL.c_str());
    http.addHeader("Content-Type", "application/json");
    JsonDocument data;
    data["mac"] = WiFi.macAddress();
    JsonArray sensors = data.createNestedArray("sensors");
    JsonObject obj_tmpA = sensors.createNestedObject();
    obj_tmpA["type"] = "tmpA";
    obj_tmpA["value"] = tmpA;
    JsonObject obj_umdA = sensors.createNestedObject();
    obj_umdA["type"] = "umdA";
    obj_umdA["value"] = umdA;
    JsonObject obj_tmpS = sensors.createNestedObject();
    obj_tmpS["type"] = "tmpS";
    obj_tmpS["value"] = tmpS;
    String jsonString;
    serializeJson(data, jsonString);
    Serial.println(jsonString);
    Serial.print("[WiFi] POST data... ");
    int httpResponseCode = http.POST(jsonString);
    if (httpResponseCode == 200) {
      Serial.println("OK");
      digitalWrite(LED_PIN, LOW);
      delay(100);
      digitalWrite(LED_PIN, HIGH);
      delay(100);
      digitalWrite(LED_PIN, LOW);
      delay(100);
      digitalWrite(LED_PIN, HIGH);
      String payload = http.getString();
      Serial.println(payload);
      deserializeJson(json, payload);
      bool light = json["light"];
      if (light == true) {
        Serial.println("Light: Ligado");
        digitalWrite(RELAY1_PIN, LOW);
      } else {
        digitalWrite(RELAY1_PIN, HIGH);
      }
      bool pump = json["pump"];
      if (pump == true) {
        Serial.println("Pump: Ligado");
        digitalWrite(RELAY2_PIN, LOW);
      } else {
        digitalWrite(RELAY2_PIN, HIGH);
      }
    } else {
      Serial.println("Error");
    }
    http.end();
    float tmpS_max = json["tmpS_max"];
    float tmpS_min = json["tmpS_min"];
    if (tmpS > tmpS_max || tmpS < tmpS_min) {
      Serial.println("Temperatura fora da faixa ideal!");
      tone(BUZZER_PIN, 1000);
      delay(100);
      noTone(BUZZER_PIN);
      delay(100);
      tone(BUZZER_PIN, 1000);
      delay(100);
      noTone(BUZZER_PIN);
    }
    float tmpA_max = json["tmpA_max"];
    float tmpA_min = json["tmpA_min"];
    if (tmpA > tmpA_max || tmpA < tmpA_min) {
      Serial.println("Temperatura ambiente fora da faixa ideal!");
      tone(BUZZER_PIN, 1000);
      delay(100);
      noTone(BUZZER_PIN);
      delay(100);
      tone(BUZZER_PIN, 1000);
      delay(100);
      noTone(BUZZER_PIN);
    }
    float umdA_max = json["umdA_max"];
    float umdA_min = json["umdA_min"];
    if (umdA > umdA_max || umdA < umdA_min) {
      Serial.println("Umidade fora da faixa ideal!");
      tone(BUZZER_PIN, 1000);
      delay(100);
      noTone(BUZZER_PIN);
      delay(100);
      tone(BUZZER_PIN, 1000);
      delay(100);
      noTone(BUZZER_PIN);
    }
    delay(4500);
  }
}
