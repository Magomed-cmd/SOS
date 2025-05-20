#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <ArduinoJson.h>

// MAC мастера (точно укажи MAC из esptool)
uint8_t master_mac[] = {0x30, 0xC9, 0x22, 0xF2, 0x80, 0xFC};

#define GREEN   "\033[32m"
#define RED     "\033[31m"
#define YELLOW  "\033[33m"
#define RESET   "\033[0m"

void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print(YELLOW "[INFO]" RESET " Send status: ");
  if (status == ESP_NOW_SEND_SUCCESS) {
    Serial.print(GREEN "SUCCESS" RESET "\n");
  } else {
    Serial.print(RED "FAIL" RESET "\n");
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.print(YELLOW "[INFO]" RESET " Receiver started, initializing ESP-NOW...\n");
  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) {
    Serial.print(RED "[ERROR]" RESET " ESP-NOW init failed!\n");
    while (true) delay(1000);
  }

  esp_now_register_send_cb(onDataSent);

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, master_mac, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.print(RED "[ERROR]" RESET " Failed to add master as peer!\n");
    while (true) delay(1000);
  } else {
    Serial.print(GREEN "[OK]" RESET " Master added as peer\n");
  }
}

void loop() {
  // Подготовим JSON-данные
  StaticJsonDocument<64> doc;
  doc["data"] = "Hi from receiver!";
  char buffer[64];
  size_t len = serializeJson(doc, buffer);

  // Отправка данных мастеру
  Serial.print(YELLOW "[INFO]" RESET " Sending data to master...\n");
  esp_err_t result = esp_now_send(master_mac, (uint8_t *)buffer, len);

  if (result == ESP_OK) {
    Serial.print(GREEN "[OK]" RESET " Data sent, waiting for status...\n");
  } else {
    Serial.print(RED "[ERROR]" RESET " Failed to send data!\n");
  }

  delay(5000); // Каждые 5 секунд
}