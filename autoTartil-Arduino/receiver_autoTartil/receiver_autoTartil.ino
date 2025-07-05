/*
 * PROGRAM RECEIVER UNTUK AUTO TARTIL
 * PARAMETER DATA YANG DIBUTUHKAN 
 * 
 * -TIME:04,00,0,0 
 *      04 :JAM 
 *      00 :MENIT 
 *      0  :DETIK 
 *      0  :HARI
 *      
 * -HR:0|W0:1,1,1,1,1,1-2-3
 *      Hari: Minggu (0)
 *      Waktu: Subuh (0)
 *      Aktif: ya
 *      Aktif Adzan: ya
 *      File Adzan: 3
 *      Tartil sebelum adzan: ya
 *      Folder: 1
 *      List rekaman: 1, 2, 3
 *      
 * -JWS:4,30|12,0|15,30|18,0|19,30
 *      Waktu 0: 4:30
 *      Waktu 1: 12:0
 *      Waktu 2: 15:30
 *      Waktu 3: 18:0
 *      Waktu 4: 19:30
 */

#include <ESP8266WiFi.h>
#include <WebSocketsClient.h>
#include <ArduinoOTA.h>
#include <ESP_EEPROM.h>

const char* ssid = "JAM_PANEL";
const char* password = "00000000";

// Untuk OTA mode
const char* ota_ssid = "KELUARGA02";
const char* ota_pass = "suhartono";

WebSocketsClient webSocket;

//unsigned long lastSend = 0;
unsigned long lastWiFiAttempt = 0;
const unsigned long wifiRetryInterval = 5000;

bool wifiConnected = false;
bool wsConnected = false;
bool modeSetting = false;
bool modeR = false;

#define LED_WIFI 2
#define EEPROM_SIZE 512
#define ADDR_MODE_R 100

// ------------------- EEPROM -------------------
void saveModeR(uint8_t value) {
  EEPROM.write(ADDR_MODE_R, value);
  EEPROM.commit();
}
uint8_t loadModeR() {
  return EEPROM.read(ADDR_MODE_R);
}

// ------------------- WebSocket Event -------------------
void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      //Serial.println("[WS] Terputus dari server");
      wsConnected = false;
      break;
    case WStype_CONNECTED:
      //Serial.println("[WS] Terhubung ke server");
      wsConnected = true;
      webSocket.sendTXT("CLIENT_READY");
      break;
    case WStype_TEXT: {
      String msg = String((char*)payload);
      Serial.println(msg);
      break;
    }
  }
}

// ------------------- Arduino OTA Mode -------------------
void startOTAMode() {
  //Serial.println("[OTA] Masuk ke mode OTA Upload");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ota_ssid, ota_pass);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    //Serial.print(".");
  }

  ArduinoOTA.setHostname("ESP_CLIENT");
  ArduinoOTA.begin();
}

// ------------------- Setup -------------------
void setup() {
  Serial.begin(115200);
  pinMode(LED_WIFI, OUTPUT);
  digitalWrite(LED_WIFI, LOW);
  EEPROM.begin(EEPROM_SIZE);
  modeR = loadModeR();
  
  // Cek jika mode OTA aktif
  if (modeR == 1) {
    saveModeR(0); // Reset modeR agar kembali ke normal setelah OTA
    startOTAMode();
    return;
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  lastWiFiAttempt = millis();

  Serial.println("⏳ Mencoba koneksi WiFi...");

  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(5000);
}

// ------------------- Loop -------------------
void loop() {
  // Jika mode OTA aktif, jalankan OTA
  if (modeR == 1) {
    ArduinoOTA.handle();
    return;
  }
  
  checkSerialCommand();

  if (!wifiConnected && millis() - lastWiFiAttempt >= wifiRetryInterval) {
    lastWiFiAttempt = millis();

    if (WiFi.status() == WL_CONNECTED) {
      wifiConnected = true;

      if (!modeSetting) {
        webSocket.begin("192.168.2.1", 81, "/");
      }
    } 
  }

  if (wifiConnected && !modeSetting) {
    webSocket.loop();
  }

  digitalWrite(LED_WIFI, (wifiConnected && wsConnected && !modeSetting) ? HIGH : LOW);
}

// ------------------- Perintah Serial -------------------
void checkSerialCommand() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();

    if (cmd.equalsIgnoreCase("restart")) {
      if (wsConnected) {
        webSocket.sendTXT("restart=1");
      } 
    } else if (cmd.equalsIgnoreCase("SETTING")) {
      modeSetting = true;

      if (wsConnected) {
        webSocket.disconnect();
        wsConnected = false;
      }

      if (wifiConnected) {
        WiFi.disconnect();
        wifiConnected = false;
      }

    } else if (cmd.equalsIgnoreCase("NORMAL")) {
      modeSetting = false;

      if (WiFi.status() != WL_CONNECTED) {
        WiFi.begin(ssid, password);
        lastWiFiAttempt = millis();
      }

    } else if (cmd.startsWith("modeR=1")) {
      saveModeR(1);
      delay(500);
      ESP.restart();

    } else if (cmd.equalsIgnoreCase("jadwal")) {
      if (wsConnected) {
        webSocket.sendTXT("jadwal");
      } 
    }
  }
}
