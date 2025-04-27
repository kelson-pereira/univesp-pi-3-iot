#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// Configurações de Hardware
#define DHT_PIN          14
#define DHT_TYPE         DHT22
#define ONEWIRE_PIN      26
#define LEVEL_PIN        27
#define BUZZER_PIN       25
#define LED_PIN          2
#define RELAY1_PIN       32
#define RELAY2_PIN       33

// Constantes
bool KEEP_ON = true;
bool KEEP_OFF = false;

// Configurações de Rede
const char* WIFI_SSID     = "SSID";
const char* WIFI_PASSWORD = "PASSWD";
const String API_URL      = "https://plantio-74e808a068fc.herokuapp.com/update/";

// Intervalos
const unsigned long SENSOR_READ_INTERVAL = 5000; // 5 segundos
const unsigned long BUZZER_BEEP_DURATION = 100;
const unsigned long LED_BLINK_INTERVAL = 100;

// Objetos globais
DHT dht(DHT_PIN, DHT_TYPE);
OneWire oneWire(ONEWIRE_PIN);
DallasTemperature tempSensors(&oneWire);
WiFiClient wifiClient;

void setupHardware() {
  Serial.begin(115200);
  
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
  pinMode(LEVEL_PIN, INPUT);
  
  digitalWrite(RELAY1_PIN, HIGH); // Inicia com relé desligado
  digitalWrite(RELAY2_PIN, HIGH);
  
  dht.begin();
  tempSensors.begin();
}

void setupOTA() {
  // Configuração básica do OTA
  ArduinoOTA.setHostname("ESP32-PlantIO"); // Nome do dispositivo na rede
  
  // Senha para proteção (opcional)
  ArduinoOTA.setPassword("12345678");

  // Callbacks para status do OTA
  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else
        type = "filesystem";
      Serial.println("Iniciando atualização OTA: " + type);
      digitalWrite(LED_PIN, LOW); // LED aceso durante atualização
    })
    .onEnd([]() {
      Serial.println("\nAtualização concluída!");
      digitalWrite(LED_PIN, HIGH);
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progresso: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Erro[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Autenticação falhou");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Falha ao iniciar");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Falha na conexão");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Falha na recepção");
      else if (error == OTA_END_ERROR) Serial.println("Falha ao finalizar");
      digitalWrite(LED_PIN, HIGH);
    });

  ArduinoOTA.begin();
  Serial.println("Atualização OTA (Over-The-Air) via Wi-Fi disponível.");
}

bool connectToWiFi() {
  Serial.print("\nConectando ao WiFi.");
  WiFi.disconnect(true);
  delay(100);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  for (int i = 0; i < 10; i++) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println(" Conectado!");
      Serial.printf("IP: %s MAC: %s\n", WiFi.localIP().toString().c_str(), WiFi.macAddress().c_str());

      // Inicializa OTA apenas após conexão Wi-Fi
      setupOTA();

      blinkLED(1, KEEP_ON);
      beepBuzzer(2, 1000);
      return true;
    }
    delay(500 * (i + 1));
    Serial.print(".");
    blinkLED(1, KEEP_OFF);
  }
  
  Serial.println("\nFalha na conexão!");
  beepBuzzer(1, 500);
  return false;
}

void readSensors(float& temperature, float& humidity, float& solutionTemp, bool& levelStatus) {
  temperature = dht.readTemperature();
  humidity = dht.readHumidity();
  tempSensors.requestTemperatures();
  solutionTemp = tempSensors.getTempCByIndex(0);
  levelStatus = digitalRead(LEVEL_PIN);
  
  Serial.printf("Temperatura: %.1f°C, Umidade: %.1f%%, Temp. Solução: %.1f°C, Nível: %s\n", 
                temperature, humidity, solutionTemp, levelStatus ? "Alto" : "Baixo");
}

void sendDataToServer(float temperature, float humidity, float tempSolution, bool levelSolution) {
  HTTPClient http;
  http.begin(API_URL.c_str());
  http.addHeader("Content-Type", "application/json");

  JsonDocument doc;
  doc["mac"] = WiFi.macAddress();
  
  JsonArray sensors = doc.createNestedArray("sensors");
  if (!isnan(temperature)) {
    addSensorData(sensors, "tmpA", temperature);
  }
  if (!isnan(humidity)) {
    addSensorData(sensors, "umdA", humidity);
  }
  if (tempSolution != DEVICE_DISCONNECTED_C) {
    addSensorData(sensors, "tmpS", tempSolution);
  }
  // Status do nível (1=alto, 0=baixo)
  addSensorData(sensors, "levS", levelSolution ? 1.0 : 0.0);

  String json;
  serializeJson(doc, json);
  
  int httpCode = http.POST(json);
  if (httpCode == HTTP_CODE_OK) {
    handleServerResponse(http.getString());
    blinkLED(3, KEEP_ON); // Feedback visual
  } else {
    Serial.println("Erro na requisição HTTP");
  }
  
  http.end();
}

void addSensorData(JsonArray& array, const char* type, float value) {
  JsonObject sensor = array.createNestedObject();
  sensor["type"] = type;
  sensor["value"] = value;
}

void handleServerResponse(const String& response) {
  JsonDocument doc;
  deserializeJson(doc, response);
  
  // Controle de relés
  digitalWrite(RELAY1_PIN, doc["light"] ? LOW : HIGH);
  digitalWrite(RELAY2_PIN, doc["pump"] ? LOW : HIGH);
  
  // Verificação de limites
  checkLimits(doc["tmpA"], doc["tmpA_min"], doc["tmpA_max"], "Temperatura ambiente");
  checkLimits(doc["umdA"], doc["umdA_min"], doc["umdA_max"], "Umidade");
  checkLimits(doc["tmpS"], doc["tmpS_min"], doc["tmpS_max"], "Temperatura da solução");
  checkLimits(doc["levS"], doc["levS_min"], doc["levS_max"], "Nível da solução");
}

void checkLimits(float value, float min, float max, const char* sensorName) {
  if (value < min || value > max) {
    Serial.printf("%s fora da faixa ideal!\n", sensorName);
    beepBuzzer(3, 1000);
  }
}

void beepBuzzer(int times, int frequency) {
  for (int i = 0; i < times; i++) {
    tone(BUZZER_PIN, frequency, BUZZER_BEEP_DURATION);
    delay(BUZZER_BEEP_DURATION);
    noTone(BUZZER_PIN);
    delay(BUZZER_BEEP_DURATION);
  }
}

void blinkLED(int times, bool keepOn) {
  for (int i = 0; i < times; i++) {
    digitalWrite(LED_PIN, keepOn ? LOW : HIGH);
    delay(LED_BLINK_INTERVAL);
    digitalWrite(LED_PIN, keepOn ? HIGH : LOW);
    delay(LED_BLINK_INTERVAL);
  }
}

void setup() {
  setupHardware();
  connectToWiFi();
}

void loop() {
  // Verifica atualizações OTA
  ArduinoOTA.handle();

  static unsigned long lastReadTime = 0;

  if (millis() - lastReadTime >= SENSOR_READ_INTERVAL) {
    if (WiFi.status() == WL_CONNECTED) {
      float temperature, humidity, tempSolution;
      bool levelSolution;
      readSensors(temperature, humidity, tempSolution, levelSolution);
      sendDataToServer(temperature, humidity, tempSolution, levelSolution);
    } else {
      connectToWiFi();
    }
    lastReadTime = millis();
  }
}