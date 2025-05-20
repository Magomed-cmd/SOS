#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include "esp_sleep.h"

// MAC-адреса ресиверов
uint8_t receiver1_mac[] = {0xD8, 0xBC, 0x38, 0xFB, 0xB8, 0xF0};
uint8_t receiver2_mac[] = {0xD0, 0xEF, 0x76, 0x45, 0x3D, 0xC8};

// WiFi и сервер
const char* ssid = "Edem";
const char* password = "12345678";
const char* serverUrl = "http://192.168.1.141:8080/data";

// Хранилища для данных
char data1[128], data2[128];
bool received1 = false, received2 = false;

// Цвета для Serial
#define GREEN   "\033[32m"
#define RED     "\033[31m"
#define YELLOW  "\033[33m"
#define RESET   "\033[0m"

// Callback ESP-NOW
void onDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  StaticJsonDocument<128> doc;
  DeserializationError error = deserializeJson(doc, incomingData, len);
  if (error) {
    Serial.print(RED "[ERROR]" RESET " JSON parse error: ");
    Serial.println(error.c_str());
    return;
  }
  if (memcmp(mac, receiver1_mac, 6) == 0) {
    strncpy(data1, (const char *)incomingData, len);
    data1[len] = 0;
    received1 = true;
    Serial.print(GREEN "[OK]" RESET " Packet from Receiver 1\n");
  } else if (memcmp(mac, receiver2_mac, 6) == 0) {
    strncpy(data2, (const char *)incomingData, len);
    data2[len] = 0;
    received2 = true;
    Serial.print(GREEN "[OK]" RESET " Packet from Receiver 2\n");
  } else {
    Serial.print(YELLOW "[INFO]" RESET " Unknown MAC: "); Serial.println(macStr);
  }
}

void setupEspNow() {
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.print(RED "[ERROR]" RESET " ESP-NOW init failed!\n");
    while (true) delay(1000);
  }
  esp_now_register_recv_cb(onDataRecv);
  Serial.print(GREEN "[OK]" RESET " ESP-NOW ready, waiting for packets...\n");
}

void setup() {
  Serial.begin(115200);
  delay(500);

  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER) {
    Serial.print(YELLOW "[INFO]" RESET " Woke up from deep sleep. Ready for new cycle!\n");
  }

  setupEspNow();
}

void loop() {
  if (received1 && received2) {
    Serial.print(GREEN "\n[OK]" RESET " Both packets received!\n");
    Serial.print(YELLOW "Receiver1: " RESET); Serial.println(data1);
    Serial.print(YELLOW "Receiver2: " RESET); Serial.println(data2);

    // Собираем правильный вложенный JSON для отправки
    StaticJsonDocument<256> docOut;
    StaticJsonDocument<64> d1, d2;

    DeserializationError err1 = deserializeJson(d1, data1);
    DeserializationError err2 = deserializeJson(d2, data2);

    if (!err1) {
      docOut["from_receiver1"].set(d1.as<JsonVariant>());
    } else {
      docOut["from_receiver1"] = nullptr;
    }
    if (!err2) {
      docOut["from_receiver2"].set(d2.as<JsonVariant>());
    } else {
      docOut["from_receiver2"] = nullptr;
    }

    char outJson[256];
    serializeJson(docOut, outJson);

    Serial.print(GREEN "\n[OK] JSON to send:\n" RESET);
    Serial.println(outJson);

    // Отключаем ESP-NOW, включаем WiFi
    esp_now_deinit();
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    Serial.print(YELLOW "[INFO]" RESET " Connecting to WiFi...\n");
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED) {
      if (millis() - start > 10000) {
        Serial.print(RED "[ERROR]" RESET " WiFi connect timeout!\n");
        break;
      }
      delay(200);
      Serial.print(".");
    }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.print(GREEN "\n[OK]" RESET " WiFi connected!\n");
    }

    HTTPClient http;
    http.begin(serverUrl);
    http.addHeader("Content-Type", "application/json");
    int httpResponseCode = http.POST((uint8_t*)outJson, strlen(outJson));
    if (httpResponseCode > 0) {
      Serial.print(GREEN "[OK]" RESET " Data sent, server response: ");
      Serial.println(httpResponseCode);
      String resp = http.getString();
      Serial.println(resp);
    } else {
      Serial.print(RED "[ERROR]" RESET " HTTP POST failed: ");
      Serial.println(httpResponseCode);
    }
    http.end();

    WiFi.disconnect(true);
    delay(200);

    Serial.print(YELLOW "\n[INFO] Going to deep sleep for 5 seconds...\n" RESET);
    esp_sleep_enable_timer_wakeup(5 * 1000000ULL); // 5 сек
    esp_deep_sleep_start();
    // Дальше код не выполняется до следующего пробуждения!
  }
  delay(100);
}