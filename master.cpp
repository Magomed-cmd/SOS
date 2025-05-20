#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <ArduinoJson.h>

// MAC-адреса ресиверов (по порядку!)
uint8_t receiver1_mac[] = {0xD8, 0xBC, 0x38, 0xFB, 0xB8, 0xF0};
uint8_t receiver2_mac[] = {0xD0, 0xEF, 0x76, 0x45, 0x3D, 0xC8};

// Хранилища для данных
char data1[128];
char data2[128];
bool received1 = false;
bool received2 = false;

// Цвета ANSI
#define GREEN   "\033[32m"
#define RED     "\033[31m"
#define YELLOW  "\033[33m"
#define RESET   "\033[0m"

// Callback для esp32 core 2.x
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
    Serial.print(YELLOW "[INFO]" RESET " Unknown MAC: ");
    Serial.println(macStr);
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.print(YELLOW "[INFO]" RESET " Master starting, initializing ESP-NOW...\n");
  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) {
    Serial.print(RED "[ERROR]" RESET " ESP-NOW init failed!\n");
    while (true) delay(1000);
  }
  esp_now_register_recv_cb(onDataRecv);

  Serial.print(GREEN "[OK]" RESET " Waiting for both receivers...\n");
}

void loop() {
  if (received1 && received2) {
    Serial.print(GREEN "\n[OK]" RESET " Both packets received!\n");

    Serial.print(YELLOW "Receiver1: " RESET); Serial.println(data1);
    Serial.print(YELLOW "Receiver2: " RESET); Serial.println(data2);

    // Собираем JSON для отправки на сервер
    StaticJsonDocument<256> docOut;
    docOut["from_receiver1"] = data1;
    docOut["from_receiver2"] = data2;

    char outJson[256];
    serializeJson(docOut, outJson);
    Serial.print(GREEN "\n[OK] JSON to send:\n" RESET);
    Serial.println(outJson);

    // Здесь могла бы быть отправка HTTP POST (WiFi + HTTPClient)
    // Но это делается только после завершения ESP-NOW!

    // Сбрасываем флаги и ждем новые данные
    received1 = false;
    received2 = false;
    memset(data1, 0, sizeof(data1));
    memset(data2, 0, sizeof(data2));

    Serial.print(YELLOW "\n[INFO] Waiting for next cycle...\n" RESET);
  }

  delay(100); // Экономим CPU
}