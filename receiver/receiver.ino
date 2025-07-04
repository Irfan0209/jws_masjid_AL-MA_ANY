#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <ESP_EEPROM.h>

#define EEPROM_SIZE 512
#define ADDR_MODE        0
#define ADDR_PASSWORD    2

char ssid[20]     = "JAM_PANEL_1";
char password[20] = "00000000";

const char* idwifi = "KELUARGA02";
const char* passwifi = "suhartono";
const char* host = "JAM_PANEL";

ESP8266WebServer server(80);
WebSocketsServer webSocket(81);
bool stateMode = 0;

IPAddress local_IP(192, 168, 2, 1);
IPAddress gateway(192, 168, 2, 1);
IPAddress subnet(255, 255, 255, 0);

// --- EEPROM Helper ---
void saveStringToEEPROM(int startAddr, String data, int maxLength) {
  for (int i = 0; i < maxLength; i++) {
    EEPROM.write(startAddr + i, (i < data.length()) ? data[i] : 0);
  }
}

// --- WebSocket Event Handler ---
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_CONNECTED:
      Serial.printf("[WS] Client %u connected\n", num);
      break;
    case WStype_DISCONNECTED:
      Serial.printf("[WS] Client %u disconnected\n", num);
      break;
    case WStype_TEXT:
      String msg = String((char*)payload);
      Serial.printf("[WS] Received: %s\n", msg.c_str());
      getData(msg);
      String balasan = "ESP menerima: " + msg;
      webSocket.sendTXT(num, balasan);

      break;
  }
}

// --- HTTP Handler ---
void handleSetTime() {
  String data = "";
  if (server.hasArg("Tm")) {
    data = "Tm=" + server.arg("Tm");
    Serial.println(data);
    getData(data);
    server.send(200, "text/plain", "OK");
  }
  if (server.hasArg("mode")) {
    data = "mode=" + server.arg("mode");
    Serial.println(data);
    getData(data);
    server.send(200, "text/plain", "OK");
  }
  EEPROM.commit();
}

// --- Web Server Init for AP ---
void AP_init() {
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(local_IP, gateway, subnet);
  WiFi.softAP(ssid, password);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  delay(1000);

  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());

  server.on("/setPanel", handleSetTime);
  server.begin();
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  Serial.println("Web & WebSocket Server started");
}

// --- Online Mode (OTA via WiFi) ---
void ONLINE(){
  WiFi.mode(WIFI_STA);
  WiFi.begin(idwifi, passwifi);

  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("OTA WiFi gagal. Rebooting...");
    delay(5000);
    ESP.restart();
  }

  ArduinoOTA.setHostname(host);
  ArduinoOTA.setPassword("123456");  // Ganti sesuai keinginanmu

  ArduinoOTA.onStart([]() {
    Serial.println("Start updating...");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
    stateMode = 0;
    EEPROM.write(ADDR_MODE, stateMode);
    EEPROM.commit();
    delay(1000);
    ESP.restart();
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA Error[%u]: ", error);
  });

  ArduinoOTA.begin();
}

// --- Data Handler ---
void getData(String input){
  Serial.println("getData: " + input);
  // Proses sesuai kebutuhan
}

// --- EEPROM Loader ---
void loadFromEEPROM() {
  stateMode = EEPROM.read(ADDR_MODE);
  for (int i = 0; i < 8; i++) {
    password[i] = EEPROM.read(ADDR_PASSWORD + i);
  }
  password[8] = '\0';
}

// --- SETUP ---
void setup() {
  Serial.begin(115200);
  EEPROM.begin(EEPROM_SIZE);
  loadFromEEPROM();

  if(stateMode){
    ONLINE();  // Mode OTA aktif
  } else {
    AP_init(); // Mode Server/AP aktif
  }
}

// --- LOOP ---
void loop() {
  if (stateMode) {
    ArduinoOTA.handle();
  } else {
    server.handleClient();
    webSocket.loop();
  }
}
