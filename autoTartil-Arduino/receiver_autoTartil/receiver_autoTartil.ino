#include <ESP8266WiFi.h>
#include <WebSocketsClient.h>
#include <ESP_EEPROM.h>

const char* ssid = "JAM_PANEL_1";        // SSID dari ESP Server (AP)
const char* password = "00000000";       // Password AP

WebSocketsClient webSocket;

unsigned long lastSend = 0;

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.println("[WS] Terputus dari server");
      break;
    case WStype_CONNECTED:
      Serial.println("[WS] Terhubung ke server");
      // Kirim pesan saat pertama konek
      webSocket.sendTXT("Client: Terhubung");
      break;
    case WStype_TEXT:
      Serial.print("[WS] Pesan dari server: ");
      Serial.println((char*)payload);
      break;
  }
}

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  Serial.print("Menghubungkan ke WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nTerhubung ke AP Server");

  // Koneksi WebSocket ke ESP Server
  webSocket.begin("192.168.2.1", 81, "/"); // IP Server + Port WebSocket
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(5000); // Otomatis reconnect
}

void loop() {
  webSocket.loop();

  // Kirim data setiap 5 detik
  if (millis() - lastSend > 5000) {
    lastSend = millis();
    webSocket.sendTXT("DATA_CLIENT: Halo dari ESP Client");
  }
}
