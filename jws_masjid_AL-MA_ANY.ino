

//SETUP DMD
#define DISPLAYS_WIDE 2
#define DISPLAYS_HIGH 1


#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
//////////
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

#include <DMDESP.h>
#include <ESP_EEPROM.h>
DMDESP  Disp(DISPLAYS_WIDE, DISPLAYS_HIGH);  // Jumlah Panel P10 yang digunakan (KOLOM,BARIS)

// Pengaturan hotspot WiFi dari ESP8266
char ssid[20]     = "JAM_PANEL";
char password[20] = "00000000";

//pengaturan wifi untuk upload program
const char* idwifi = "KELUARGA02";
const char* passwifi = "suhartono";
const char* host = "JAM_PANEL";

ESP8266WebServer server(80);

#include <Wire.h>
#include <RtcDS3231.h>


#include <Prayer.h>


#include <C:\Users\irfan\Documents\Project\jws_masjid_AL-MA_ANY\fonts/SystemFont5x7.h>
#include <C:\Users\irfan\Documents\Project\jws_masjid_AL-MA_ANY\fonts/Font4x6.h>
#include <C:\Users\irfan\Documents\Project\jws_masjid_AL-MA_ANY\fonts/System4x7.h>
#include <C:\Users\irfan\Documents\Project\jws_masjid_AL-MA_ANY\fonts/SmallCap4x6.h>
#include <C:\Users\irfan\Documents\Project\jws_masjid_AL-MA_ANY\fonts/EMSans8x16.h>
#include <C:\Users\irfan\Documents\Project\jws_masjid_AL-MA_ANY\fonts/BigNumber.h>


#define BUZZ  D4 // PIN BUZZER

#define Font0 SystemFont5x7
#define Font1 Font4x6
#define Font2 System4x7 
#define Font3 SmallCap4x6
#define Font4 EMSans8x16
#define Font5 BigNumber

//create object
RtcDS3231<TwoWire> Rtc(Wire);
RtcDateTime now;

// Constractor
Prayer JWS;
Hijriyah Hijir;

uint8_t iqomah[]        = {5,1,5,5,5,2,5};
uint8_t displayBlink[]  = {5,0,5,5,5,5,5};
uint8_t dataIhty[]      = {3,0,3,3,0,3};

struct Config {
  uint8_t durasiadzan = 40;
  uint8_t altitude = 10;
  double latitude = -7.364057;
  double longitude = 112.646222;
  uint8_t zonawaktu = 7;
  int16_t Correction = -1; //Koreksi tanggal hijriyah, -1 untuk mengurangi, 0 tanpa koreksi, 1 untuk menambah
};

Config config;



// Variabel untuk waktu, tanggal, teks berjalan, tampilan ,dan kecerahan
char text1[101], text2[101];
uint16_t   brightness    = 50;
bool       adzan         = 0;
bool       stateBuzzer   = 1;
uint8_t    DWidth        = Disp.width();
uint8_t    DHeight       = Disp.height();
uint8_t    sholatNow     = -1;
bool       reset_x       = 0; 

/*======library tambahan=======*/
bool flagAnim = false;
uint8_t    speedDate      = 40; // Kecepatan default date
uint8_t    speedText1     = 40; // Kecepatan default text  
uint8_t    speedText2     = 40;
float      dataFloat[10];
int        dataInteger[10];
uint8_t    indexText;
uint8_t    list,lastList;
bool       stateMode       = 0;
bool       stateBuzzWar    = 0;
uint8_t       counterName     = 0;
/*============== end ================*/

enum Show{
  ANIM_CLOCK_BIG,
  ANIM_DATE,
  ANIM_NAME,
  ANIM_TEXT1,
  ANIM_TEXT2,
  ANIM_SHOLAT,
  ANIM_ADZAN,
  ANIM_IQOMAH,
  ANIM_BLINK,
  UPLOAD
};

Show show = ANIM_CLOCK_BIG;


#define EEPROM_SIZE 512

/*
// Alamat EEPROM untuk tiap variabel
#define ADDR_TEXT       0
#define ADDR_BRIGHTNESS 110
#define ADDR_SPEEDTX    112
#define ADDR_SPEEDDT    114
#define ADDR_LATITUDE   116
#define ADDR_LONGITUDE  120
#define ADDR_TZ         124
#define ADDR_ALTITUDE   126
#define ADDR_IQOMAH     128  // misal 6 byte
#define ADDR_BLINK      134
#define ADDR_IHTY       140
#define ADDR_BUZZER     146
#define ADDR_PASSWORD   150
#define ADDR_DURASIADZAN  174
#define ADDR_CORRECTION   176
*/
#define EEPROM_SIZE       512

// Alamat EEPROM
#define ADDR_TEXT1        0     // text1, max 100 bytes
#define ADDR_TEXT2       100   // text2, max 100 bytes
#define ADDR_BRIGHTNESS  200
#define ADDR_SPEEDTX1    202
#define ADDR_SPEEDTX2    204   // Tambahan untuk speed text 2
#define ADDR_SPEEDDT     206
#define ADDR_LATITUDE    208
#define ADDR_LONGITUDE   212
#define ADDR_TZ          216
#define ADDR_ALTITUDE    218
#define ADDR_IQOMAH      220  // 6 byte
#define ADDR_BLINK       226  // 6 byte
#define ADDR_IHTY        232  // 6 byte
#define ADDR_BUZZER      238
#define ADDR_PASSWORD    240  // 8 byte
#define ADDR_DURASIADZAN 248
#define ADDR_CORRECTION  250
#define ADDR_MODE        256


void saveStringToEEPROM(int startAddr, String data, int maxLength) {
  for (int i = 0; i < maxLength; i++) {
    if (i < data.length()) {
      EEPROM.write(startAddr + i, data[i]);
    } else {
      EEPROM.write(startAddr + i, 0); // null terminate / padding
    }
  }
}

void saveFloatToEEPROM(int addr, float value) {
  byte *data = (byte*)(void*)&value;
  for (int i = 0; i < sizeof(float); i++) {
    EEPROM.write(addr + i, data[i]);
  }
}

void saveIntToEEPROM(int addr, int16_t value) {
  EEPROM.write(addr, lowByte(value));
  EEPROM.write(addr + 1, highByte(value));
}

//----------------------web server---------------------------//
// Fungsi untuk mengatur jam, tanggal, running text, dan kecerahan
void handleSetTime() {
  Serial.println("hansle run");

  String data;
  if (server.hasArg("Tm")) {
    data = server.arg("Tm");
    data = "Tm=" + data;
    Serial.println(data);
    getData(data);
    server.send(200, "text/plain", "OK");//"Settingan jam berhasil diupdate");
  }
  if (server.hasArg("text")) {
    data = server.arg("text");
    data = "text=" + data;
    Serial.println(data);
    getData(data);
    server.send(200, "text/plain", "OK");//"Settingan text berhasil diupdate");
  }
  
  if (server.hasArg("Br")) {
    data  = server.arg("Br");
    data = "Br=" + data;
    Serial.println(data);
    getData(data);
    server.send(200, "text/plain", "OK");//"Kecerahan berhasil diupdate");
  }
  if (server.hasArg("Spdt")) {
    data = server.arg("Spdt"); // Atur kecepatan date
    data = "Spdt=" + data;
    Serial.println(data);
    getData(data);
    server.send(200, "text/plain", "OK");//"Kecepatan kalender berhasil diupdate");
  }
  if (server.hasArg("Sptx1")) {
    data = server.arg("Sptx1"); // Atur kecepatan text
    data = "Sptx1=" + data;
    Serial.println(data);
    getData(data);
    server.send(200, "text/plain", "OK");//"Kecepatan nama 1 berhasil diupdate");
  }
  if (server.hasArg("Sptx2")) {
    data = server.arg("Sptx2"); // Atur kecepatan text
    data = "Sptx2=" + data;
    Serial.println(data);
    getData(data);
    server.send(200, "text/plain", "OK");//"Kecepatan nama 2 berhasil diupdate");
  }
  if (server.hasArg("Iq")) {
    data = server.arg("Iq"); // Atur koreksi iqomah
    data = "Iq=" + data;
    Serial.println(data);
    getData(data);
    server.send(200, "text/plain", "OK");//"iqomah diupdate");
  }
  if (server.hasArg("Dy")) {
    data = server.arg("Dy"); // Atur durasi adzan
    data = "Dy=" + data;
    Serial.println(data);
    getData(data);
    server.send(200, "text/plain", "OK");//"displayBlink diupdate");
  }
  if (server.hasArg("Kr")) {
    data = server.arg("Kr"); // Atur koreksi waktu jadwal sholat
    data = "Kr=" + data;
    Serial.println(data);
    getData(data);
    server.send(200, "text/plain", "OK");//"Selisih jadwal sholat diupdate");
  }
  if (server.hasArg("Lt")) {
    data = server.arg("Lt"); // Atur latitude
    data = "Lt=" + data;
    Serial.println(data);
    getData(data);
    server.send(200, "text/plain", "OK");//"latitude diupdate");
  }
  if (server.hasArg("Lo")) {
    data = server.arg("Lo"); // Atur latitude
    data = "Lo=" + data;
    Serial.println(data);
    getData(data);
    server.send(200, "text/plain", "OK");//"longitude diupdate");
  }
  if (server.hasArg("Tz")) {
    data = server.arg("Tz"); // Atur latitude
    data = "Tz=" + data;
    Serial.println(data);
    getData(data);
    server.send(200, "text/plain", "OK");//"timezone diupdate");
  }
  if (server.hasArg("Al")) {
    data = server.arg("Al"); // Atur latitude
    data = "Al=" + data;
    Serial.println(data);
    getData(data);
    server.send(200, "text/plain", "OK");//"altitude diupdate");
  }
  if (server.hasArg("Da")) { 
    data = server.arg("Da"); 
    data = "Da=" + data;
    Serial.println(data);
    getData(data);
    server.send(200, "text/plain", "OK");// "durasi adzan diupdate");
  }
  if (server.hasArg("CoHi")) {
    data = server.arg("CoHi"); // Atur latitude
    data = "CoHi=" + data;
    Serial.println(data);
    getData(data);
    server.send(200, "text/plain", "OK");//"coreksi hijriah diupdate");
  }

  if (server.hasArg("Bzr")) {
    data = server.arg("Bzr"); // Atur status buzzer
    data = "Bzr=" + data;
    Serial.println(data);
    getData(data);
    server.send(200, "text/plain","OK");// (stateBuzzer) ? "Suara Diaktifkan" : "Suara Dimatikan");
  }
  if (server.hasArg("mode")) {
    data = server.arg("mode"); // Atur status buzzer
    data = "mode=" + data;
    Serial.println(data);
    getData(data);
    server.send(200, "text/plain","OK");// (stateBuzzer) ? "Suara Diaktifkan" : "Suara Dimatikan");
  }
  if (server.hasArg("status")) {
    server.send(200, "text/plain", "CONNECTED");
  }
 
  if (server.hasArg("newPassword")) {
      data = server.arg("newPassword");
      data = "newPassword=" + data;
      Serial.println(data);
      getData(data);
      server.send(200, "text/plain","OK");// "Password WiFi diupdate");
    } 
  data="";
  }
  
//=============================================================//

//----------------------------------------------------------------------
// HJS589 P10 FUNGSI TAMBAHAN UNTUK NODEMCU ESP8266

void ICACHE_RAM_ATTR refresh() {
  Disp.refresh();
  timer0_write(ESP.getCycleCount() + 80000);
}

void Disp_init_esp() {
  
  Disp.start();
  Disp.clear();
  Disp.setBrightness(brightness);
  Serial.println("Setup dmd selesai");

  noInterrupts();
  timer0_isr_init();
  timer0_attachInterrupt(refresh);
  timer0_write(ESP.getCycleCount() + 80000);
  interrupts();
}

IPAddress local_IP(192, 168, 2, 1);      // IP Address untuk AP
IPAddress gateway(192, 168, 2, 1);       // Gateway
IPAddress subnet(255, 255, 255, 0);      // Subnet mask

void AP_init() {
  
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(local_IP, gateway, subnet);
  WiFi.softAP(ssid,password);
  WiFi.setSleepMode(WIFI_NONE_SLEEP); // Pastikan WiFi tidak sleep

  delay(1000);
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);

  server.on("/setPanel", handleSetTime);
  server.begin();
  
  Serial.println("Server dimulai.");  
}

void ONLINE(){

 WiFi.mode(WIFI_STA);
 WiFi.softAPConfig(local_IP, gateway, subnet);
 WiFi.begin(idwifi,passwifi);
 while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    digitalWrite(BUZZ,LOW);
    delay(5000);
    ESP.restart();
  }
  
  ArduinoOTA.setHostname(host);
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else {  // U_FS
      type = "filesystem";
    }

    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    Serial.println("Start updating " + type);
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
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();
}

void setup() {
  Serial.begin(115200);
  EEPROM.begin(EEPROM_SIZE);
  
  pinMode(BUZZ, OUTPUT); 
  digitalWrite(BUZZ,LOW);
  delay(200);
  digitalWrite(BUZZ,HIGH);
  int rtn = I2C_ClearBus(); // clear the I2C bus first before calling Wire.begin()
    if (rtn != 0) {
      Serial.println(F("I2C bus error. Could not clear"));
      if (rtn == 1) {
        Serial.println(F("SCL clock line held low"));
      } else if (rtn == 2) {
        Serial.println(F("SCL clock line held low by slave clock stretch"));
      } else if (rtn == 3) {
        Serial.println(F("SDA data line held low"));
      }
    } 
    else { // bus clear, re-enable Wire, now can start Wire Arduino master
      Wire.begin();
    }
  
  Rtc.Begin();
  Rtc.Enable32kHzPin(false);
  Rtc.SetSquareWavePin(DS3231SquareWavePin_ModeNone);
  //loadFromEEPROM();
//  delay(1000);
//  if(stateMode){
//    show = UPLOAD;
//    ONLINE();
//  }else{
//    Disp_init_esp();
//    AP_init();
//  }
  Disp_init_esp();
  delay(1000);
for(int i = 0; i < 4; i++)
 {
      Buzzer(1);
      delay(80);
      Buzzer(0);
      delay(80);
 }

}

void loop() {
  
// stateMode == 1? ArduinoOTA.handle() : server.handleClient();
// check();
  islam();

 switch(show){
  case ANIM_CLOCK_BIG :
    anim_JG();
  break;

  case ANIM_DATE :
    drawDate();
  break;

  case ANIM_NAME :
    (counterName==0)?drawName():scrollText();
  break;

  case ANIM_TEXT1:
    drawText1();
  break;

  case ANIM_TEXT2 :
    drawText2();
  break;

  case ANIM_SHOLAT :
    drawJadwalSholat();
  break;
  
  case ANIM_ADZAN :
//    drawAzzan();
  break;

  case ANIM_IQOMAH :
    drawIqomah();
  break;

  case ANIM_BLINK :
    blinkBlock();
  break;

  case UPLOAD :
    buzzerUpload();
  break;
 };

  buzzerWarning(stateBuzzWar);
  yield();
}

void getData(String input) {
  int eq = input.indexOf('=');
  if (eq != -1) {
    String key = input.substring(0, eq);
    String value = input.substring(eq + 1);
    
    if (key == "Tm") {
      String setJam = value;
      RtcDateTime now = Rtc.GetDateTime();
      uint8_t colon = value.indexOf(':');
      uint8_t dash1 = value.indexOf('-');
      uint8_t dash2 = value.indexOf('-', dash1 + 1);
      uint8_t dash3 = value.indexOf('-', dash2 + 1);

      if (colon != -1 && dash1 != -1 && dash2 != -1 && dash3 != -1) {
        uint8_t jam = value.substring(0, colon).toInt();
        uint8_t menit = value.substring(colon + 1, dash1).toInt();
        uint8_t tanggal = value.substring(dash1 + 1, dash2).toInt();
        uint8_t bulan = value.substring(dash2 + 1, dash3).toInt();
        uint16_t tahun = value.substring(dash3 + 1).toInt();
        Rtc.SetDateTime(RtcDateTime(tahun, bulan, tanggal, jam, menit, now.Second()));
      }
    }

    else if (key == "text") {
      int separatorIndex = value.indexOf('-');
      if (separatorIndex != -1) {
        int indexText = value.substring(0, separatorIndex).toInt();
        String pesan = value.substring(separatorIndex + 1);

        if (pesan.length() > 100) pesan = pesan.substring(0, 100);

        if (indexText == 1) {
          pesan.toCharArray(text1, 101);
          saveStringToEEPROM(ADDR_TEXT1, String(text1), 100);
        } else if (indexText == 2) {
          pesan.toCharArray(text2, 101);
          saveStringToEEPROM(ADDR_TEXT2, String(text2), 100);
        }
      }
      Buzzer(1);
      delay(500);
      ESP.restart();
    }


    else if (key == "Br") {
      brightness = map(value.toInt(), 0, 100, 10, 255);
      Disp.setBrightness(brightness);
      saveIntToEEPROM(ADDR_BRIGHTNESS, brightness);
    }

    else if (key == "Sptx1") {
      speedText1 = map(value.toInt(), 0, 100, 10, 80);
      saveIntToEEPROM(ADDR_SPEEDTX1, speedText1);
    }

    else if (key == "Sptx2") {
      speedText2 = map(value.toInt(), 0, 100, 10, 80);
      saveIntToEEPROM(ADDR_SPEEDTX2, speedText2);
    }

    else if (key == "Spdt") {
      speedDate = map(value.toInt(), 0, 100, 10, 80);
      saveIntToEEPROM(ADDR_SPEEDDT, speedDate);
    }

    else if (key == "Lt") {
      config.latitude = roundf(value.toFloat() * 1000000.0) / 1000000.0;
      saveFloatToEEPROM(ADDR_LATITUDE, config.latitude);
    }

    else if (key == "Lo") {
      config.longitude = roundf(value.toFloat() * 1000000.0) / 1000000.0;
      saveFloatToEEPROM(ADDR_LONGITUDE, config.longitude);
    }

    else if (key == "Tz") {
      config.zonawaktu = value.toInt();
      saveIntToEEPROM(ADDR_TZ, config.zonawaktu);
    }

    else if (key == "Al") {
      config.altitude = value.toInt();
      saveIntToEEPROM(ADDR_ALTITUDE, config.altitude);
    }

    else if (key == "Iq") {
      int separatorIndex = value.indexOf('-');
      int indexSholat = value.substring(0, separatorIndex).toInt();
      int indexKoreksi = value.substring(separatorIndex + 1).toInt();  
      iqomah[indexSholat] = indexKoreksi;
      EEPROM.write(ADDR_IQOMAH + indexSholat, indexKoreksi);
    }

    else if (key == "Dy") {
      int separatorIndex = value.indexOf('-');
      int indexSholat = value.substring(0, separatorIndex).toInt();
      int indexKoreksi = value.substring(separatorIndex + 1).toInt();  
      displayBlink[indexSholat] = indexKoreksi;
      EEPROM.write(ADDR_BLINK + indexSholat, indexKoreksi);
    }

    else if (key == "Kr") {
      int separatorIndex = value.indexOf('-');
      int indexSholat = value.substring(0, separatorIndex).toInt();
      int indexKoreksi = value.substring(separatorIndex + 1).toInt();  
      dataIhty[indexSholat] = indexKoreksi;
      EEPROM.write(ADDR_IHTY + indexSholat, indexKoreksi);
    }

    else if (key == "Da") {
      config.durasiadzan = value.toInt();
      EEPROM.write(ADDR_DURASIADZAN, config.durasiadzan & 0xFF);
      EEPROM.write(ADDR_DURASIADZAN + 1, (config.durasiadzan >> 8) & 0xFF);
    }

    else if (key == "CoHi") {
      config.Correction = value.toInt();
      EEPROM.write(ADDR_CORRECTION, config.Correction & 0xFF);
      EEPROM.write(ADDR_CORRECTION + 1, (config.Correction >> 8) & 0xFF);
}


    else if (key == "Bzr") {
      stateBuzzer = value.toInt();
      EEPROM.write(ADDR_BUZZER, stateBuzzer);
    }

    else if (key == "mode") {
      stateMode = value.toInt();
      EEPROM.write(ADDR_MODE, stateMode);
      delay(1000);
      ESP.restart();
    }

    else if (key == "newPassword") {
      if (value.length() == 8) {
        value.toCharArray(password, value.length() + 1);
        saveStringToEEPROM(ADDR_PASSWORD, value, 8);
        server.send(200, "text/plain", "Password WiFi diupdate");
        Buzzer(1);
        delay(500);
        ESP.restart();
      }
    }

    EEPROM.commit(); // Penting! simpan perubahan
  }
}


void loadFromEEPROM() {
  Serial.println("=== Membaca Data dari EEPROM ===");
 
  for (int i = 0; i < 100; i++) {
    text1[i] = EEPROM.read(ADDR_TEXT1 + i);
    if (text1[i] == 0) break;
  }
  Serial.print("Text1: ");
  Serial.println(text1);
  
  for (int i = 0; i < 100; i++) {
    text2[i] = EEPROM.read(ADDR_TEXT2 + i);
    if (text2[i] == 0) break;
  }
  Serial.print("Text2: ");
  Serial.println(text2);

  brightness = EEPROM.read(ADDR_BRIGHTNESS);
  Serial.print("Brightness: ");
  Serial.println(brightness);

  speedText1 = EEPROM.read(ADDR_SPEEDTX1);
  Serial.print("Speed Text1: ");
  Serial.println(speedText1);

  speedText2 = EEPROM.read(ADDR_SPEEDTX2);
  Serial.print("Speed Text2: ");
  Serial.println(speedText2);

  speedDate = EEPROM.read(ADDR_SPEEDDT);
  Serial.print("Speed Date: ");
  Serial.println(speedDate);

  // Latitude
  float latVal;
  byte *ptrLat = (byte*)(void*)&latVal;
  for (int i = 0; i < sizeof(float); i++) {
    ptrLat[i] = EEPROM.read(ADDR_LATITUDE + i);
  }
  config.latitude = latVal;
  Serial.print("Latitude: ");
  Serial.println(config.latitude, 6);

  // Longitude
  float lonVal;
  byte *ptrLon = (byte*)(void*)&lonVal;
  for (int i = 0; i < sizeof(float); i++) {
    ptrLon[i] = EEPROM.read(ADDR_LONGITUDE + i);
  }
  config.longitude = lonVal;
  Serial.print("Longitude: ");
  Serial.println(config.longitude, 6);

  config.zonawaktu = EEPROM.read(ADDR_TZ) | (EEPROM.read(ADDR_TZ + 1) << 8);
  Serial.print("Zona Waktu: ");
  Serial.println(config.zonawaktu);

  config.altitude = EEPROM.read(ADDR_ALTITUDE) | (EEPROM.read(ADDR_ALTITUDE + 1) << 8);
  Serial.print("Altitude: ");
  Serial.println(config.altitude);

  for (int i = 0; i < 6; i++) {
    iqomah[i] = EEPROM.read(ADDR_IQOMAH + i);
    Serial.print("Iqomah[");
    Serial.print(i);
    Serial.print("]: ");
    Serial.println(iqomah[i]);
  }

  for (int i = 0; i < 6; i++) {
    displayBlink[i] = EEPROM.read(ADDR_BLINK + i);
    Serial.print("Blink[");
    Serial.print(i);
    Serial.print("]: ");
    Serial.println(displayBlink[i]);
  }

  for (int i = 0; i < 6; i++) {
    dataIhty[i] = EEPROM.read(ADDR_IHTY + i);
    Serial.print("Ihtiyath[");
    Serial.print(i);
    Serial.print("]: ");
    Serial.println(dataIhty[i]);
  }

  stateBuzzer = EEPROM.read(ADDR_BUZZER);
  Serial.print("Buzzer: ");
  Serial.println(stateBuzzer);

  stateMode = EEPROM.read(ADDR_MODE);
  Serial.print("mode: ");
  Serial.println(stateMode);

  for (int i = 0; i < 8; i++) {
    password[i] = EEPROM.read(ADDR_PASSWORD + i);
  }
  password[8] = '\0';
  Serial.print("Password: ");
  Serial.println(password);

  // Tambahan yang baru:
  config.durasiadzan = EEPROM.read(ADDR_DURASIADZAN) | (EEPROM.read(ADDR_DURASIADZAN + 1) << 8);
  Serial.print("Durasi Adzan: ");
  Serial.println(config.durasiadzan);

  config.Correction = EEPROM.read(ADDR_CORRECTION) | (EEPROM.read(ADDR_CORRECTION + 1) << 8);
  Serial.print("Correction: ");
  Serial.println(config.Correction);

  Serial.println("=== Selesai Membaca EEPROM ===\n");
}

 //----------------------------------------------------------------------
// I2C_ClearBus menghindari gagal baca RTC (nilai 00 atau 165)

int I2C_ClearBus() {
  
#if defined(TWCR) && defined(TWEN)
  TWCR &= ~(_BV(TWEN)); //Disable the Atmel 2-Wire interface so we can control the SDA and SCL pins directly
#endif

  pinMode(SDA, INPUT_PULLUP); // Make SDA (data) and SCL (clock) pins Inputs with pullup.
  pinMode(SCL, INPUT_PULLUP);

  delay(2500);  // Wait 2.5 secs. This is strictly only necessary on the first power
  // up of the DS3231 module to allow it to initialize properly,
  // but is also assists in reliable programming of FioV3 boards as it gives the
  // IDE a chance to start uploaded the program
  // before existing sketch confuses the IDE by sending Serial data.

  boolean SCL_LOW = (digitalRead(SCL) == LOW); // Check is SCL is Low.
  if (SCL_LOW) { //If it is held low Arduno cannot become the I2C master. 
    return 1; //I2C bus error. Could not clear SCL clock line held low
  }

  boolean SDA_LOW = (digitalRead(SDA) == LOW);  // vi. Check SDA input.
  int clockCount = 20; // > 2x9 clock

  while (SDA_LOW && (clockCount > 0)) { //  vii. If SDA is Low,
    clockCount--;
  // Note: I2C bus is open collector so do NOT drive SCL or SDA high.
    pinMode(SCL, INPUT); // release SCL pullup so that when made output it will be LOW
    pinMode(SCL, OUTPUT); // then clock SCL Low
    delayMicroseconds(10); //  for >5uS
    pinMode(SCL, INPUT); // release SCL LOW
    pinMode(SCL, INPUT_PULLUP); // turn on pullup resistors again
    // do not force high as slave may be holding it low for clock stretching.
    delayMicroseconds(10); //  for >5uS
    // The >5uS is so that even the slowest I2C devices are handled.
    SCL_LOW = (digitalRead(SCL) == LOW); // Check if SCL is Low.
    int counter = 20;
    while (SCL_LOW && (counter > 0)) {  //  loop waiting for SCL to become High only wait 2sec.
      counter--;
      delay(100);
      SCL_LOW = (digitalRead(SCL) == LOW);
    }
    if (SCL_LOW) { // still low after 2 sec error
      return 2; // I2C bus error. Could not clear. SCL clock line held low by slave clock stretch for >2sec
    }
    SDA_LOW = (digitalRead(SDA) == LOW); //   and check SDA input again and loop
  }
  if (SDA_LOW) { // still low
    return 3; // I2C bus error. Could not clear. SDA data line held low
  }

  // else pull SDA line low for Start or Repeated Start
  pinMode(SDA, INPUT); // remove pullup.
  pinMode(SDA, OUTPUT);  // and then make it LOW i.e. send an I2C Start or Repeated start control.
  // When there is only one I2C master a Start or Repeat Start has the same function as a Stop and clears the bus.
  /// A Repeat Start is a Start occurring after a Start with no intervening Stop.
  delayMicroseconds(10); // wait >5uS
  pinMode(SDA, INPUT); // remove output low
  pinMode(SDA, INPUT_PULLUP); // and make SDA high i.e. send I2C STOP control.
  delayMicroseconds(10); // x. wait >5uS
  pinMode(SDA, INPUT); // and reset pins as tri-state inputs which is the default state on reset
  pinMode(SCL, INPUT);
  return 0; // all ok
}

void buzzerUpload(){

    static bool state;
    static uint32_t save = 0;
    static uint8_t  con = 0;
    uint32_t tmr = millis();
    
    if(tmr - save > 1000 ){
      save = tmr;
      state = !state;
      digitalWrite(BUZZ, state);
      
    }
}

void buzzerWarning(int cek){

   static bool state = false;
   static uint32_t save = 0;
   uint32_t tmr = millis();
   static uint8_t con = 0;
    
    if(tmr - save > 2500 && cek == 1){
      save = tmr;
      state = !state;
      digitalWrite(BUZZ, state);
      //Serial.println("active");
      if(con <= 6) { con++; }
      if(con == 7) { cek = 0; con = 0; state = false; stateBuzzWar = 0; }
      Serial.println("con:" + String(con));
    } 
    
}

void Buzzer(uint8_t state)
  {
    if(!stateBuzzer) return;
    
    switch(state){
      case 0 :
        digitalWrite(BUZZ,HIGH);
      break;
      case 1 :
        digitalWrite(BUZZ,LOW);
      break;
    };
  }
