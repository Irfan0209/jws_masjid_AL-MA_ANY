/*
  Auto Tartil System with Arduino UNO + ESP8266 + DFPlayer Mini
  --------------------------------------------------------------
  - ESP8266: menerima konfigurasi dari App Inventor via AP + HTTP GET
  - Arduino UNO: membaca konfigurasi via Serial, menyimpan konfigurasi dalam array
  - Arduino mengatur jadwal berdasarkan waktu dan memutar rekaman tartil dan adzan
  - DFPlayer Mini: memainkan file tartil & adzan
  - Relay: mengaktifkan power amplifier saat audio diputar
*/

#include <SoftwareSerial.h>
#include <DFRobotDFPlayerMini.h>
#include <EEPROM.h>
#include <TimeLib.h>
#include "OneButton.h"

SoftwareSerial dfSerial(2, 3); // RX, TX ke DFPlayer
DFRobotDFPlayerMini dfplayer;

#define RELAY_PIN 13
#define RUN_LED   9
#define NORMAL_LED 11
#define SETTING_LED 10
#define NORMAL_STATUS_LED 12

#define NORMAL_BUTTON 8
#define SETTING_BUTTON 7
#define RESET_BUTTON  6


OneButton button1(NORMAL_BUTTON, true);
OneButton button1(SETTING_BUTTON, true);
OneButton button1(RESET_BUTTON, true);

#define HARI_TOTAL 8 // 7 hari + SemuaHari (index ke-7)
#define WAKTU_TOTAL 5
#define MAX_FILE 11
#define MAX_FOLDER 10

struct WaktuConfig {
  byte aktif;
  byte aktifAdzan;
  byte fileAdzan;
  byte tartilDulu;
  uint16_t durasiTartil;
  byte folder;
  byte list[3];
};

WaktuConfig jadwal[HARI_TOTAL][WAKTU_TOTAL];
uint16_t durasiAdzan[MAX_FILE];
uint16_t durasiTartil[MAX_FOLDER][MAX_FILE];
byte volumeDFPlayer = 20;

uint8_t jamSholat[WAKTU_TOTAL] = {4, 12, 15, 18, 19};
uint8_t menitSholat[WAKTU_TOTAL] = {30, 0, 30, 0, 30};

bool tartilSedangDiputar = false;
unsigned long tartilMulaiMillis = 0;
uint16_t tartilDurasi = 0;
byte tartilFolder = 0;
byte tartilIndex = 0;
byte tartilList[3];
byte tartilCount = 0;
WaktuConfig *currentCfg = nullptr;

unsigned long lastTriggerMillis = 0;
bool sudahEksekusi = false;
bool adzanSedangDiputar = false;
unsigned long adzanMulaiMillis = 0;
uint16_t adzanDurasi = 0;

byte currentDay = 0;

// Tambahan untuk relay delay dan manual
unsigned long relayOffDelayMillis = 0;
bool relayMenungguMati = false;
bool manualSedangDiputar = false;

//variabel untuk led staus run
#define BOARD_LED_BRIGHTNESS 255  // Kecerahan maksimum 244 dari 255
#define DIMM(x) ((uint32_t)(x) * (BOARD_LED_BRIGHTNESS) / 255)
uint8_t m_Counter = 0;   // Penghitung 8-bit untuk efek breathe

//variabel mode 
bool STATUS_MODE = false;

//variabel untuk mengecek status normal
unsigned long lastTimeReceived = 0;
const unsigned long TIMEOUT_INTERVAL = 70000; // 70 detik, lebih dari 1 menit


void setup() {
  pinMode(RUN_LED, OUTPUT);
  pinMode(NORMAL_LED, OUTPUT);
  pinMode(SETTING_LED, OUTPUT);
  pinMode(NORMAL_STATUS_LED, OUTPUT);
  pinMode(RESET_BUTTON, INPUT_PULLUP);
  pinMode(NORMAL_BUTTON, INPUT_PULLUP);
  pinMode(SETTING_BUTTON, INPUT_PULLUP);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW); // Awal mati

  Serial.begin(9600);
  dfSerial.begin(9600);
 // loadFromEEPROM();
  durasiTartil[0][0] = 0; 
  durasiTartil[0][1] = 20;// Folder 1, File 1 = 20 detik
  durasiTartil[0][2] = 40;
  durasiTartil[0][3] = 100;
  durasiAdzan[1]     = 16;

  if (!dfplayer.begin(dfSerial)) {
    Serial.println("DFPlayer tidak terdeteksi!");
    while (1);
  }
  dfplayer.volume(volumeDFPlayer);
  Serial.println("Sistem Auto Tartil Siap.");
}

void loop() {
  if (sudahEksekusi && millis() - lastTriggerMillis > 60000) {
    sudahEksekusi = false;
  }
  bacaDataSerial();
  cekDanPutarSholatNonBlocking();
  cekSelesaiTartil();
  cekSelesaiAdzan();
  cekRelayOffDelay();
  cekSelesaiManual();
  cekStatusWaktu(); // Tambahan
  cekStatusMode();
  getStatusRun();
}

void bacaDataSerial() {
  static String buffer = "";
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n') {
      parseData(buffer);
      buffer = "";
    } else {
      buffer += c;
    }
  }
}

void cekButton(){
  
}

void cekStatusWaktu() {
  if (millis() - lastTimeReceived > TIMEOUT_INTERVAL) {
     digitalWrite(NORMAL_STATUS_LED, LOW);
  } else {
    digitalWrite(NORMAL_STATUS_LED, HIGH);
    //digitalWrite(LED_MERAH, LOW);
  }
}

void cekStatusMode() {
  if(STATUS_MODE == true){
    digitalWrite(SETTING_LED,HIGH);
    digitalWrite(NORMAL_LED,LOW);
  }else{
    digitalWrite(SETTING_LED,LOW);
    digitalWrite(NORMAL_LED,HIGH);
  }
}

void parseData(String data) {
  /*if (data.startsWith("TIME:")) {
    int idx = 5;
    uint8_t jam = getIntPart(data, idx);
    uint8_t menit = getIntPart(data, idx);
    uint8_t hari = getIntPart(data, idx);
    if (jam < 24 && menit < 60 && hari < 7) {
      setTime(jam, menit, 0, 1, 1, 2024);
      currentDay = hari;
    }
    return;
  }*/
  if (data.startsWith("TIME:")) {
  int idx = 5;
  uint8_t jam = getIntPart(data, idx);
  uint8_t menit = getIntPart(data, idx);
  uint8_t hari = getIntPart(data, idx);
  if (jam < 24 && menit < 60 && hari < 7) {
    setTime(jam, menit, 0, 1, 1, 2024);
    currentDay = hari;
    lastTimeReceived = millis(); // Tambahkan baris ini
  }
  return;
}

  if (data.startsWith("VOL:")) {
    volumeDFPlayer = data.substring(4).toInt();
    dfplayer.volume(volumeDFPlayer);
    saveToEEPROM();
    return;
  }

  if (data.startsWith("HR:")) {
    int idxHR = data.indexOf("HR:");
    int idxW = data.indexOf("|W:");
    if (idxHR == -1 || idxW == -1) return;

    int hari = data.substring(idxHR + 3, idxW).toInt();
    String segmen = data.substring(idxW + 3);

    int koma = 0;
    byte waktuIdx = getIntPart(segmen, koma);
    if (waktuIdx >= WAKTU_TOTAL) return;

    WaktuConfig &cfg = jadwal[hari][waktuIdx];
    cfg.aktif       = getIntPart(segmen, koma);
    cfg.aktifAdzan  = getIntPart(segmen, koma);
    cfg.fileAdzan   = getIntPart(segmen, koma);
    cfg.tartilDulu  = getIntPart(segmen, koma);
    cfg.durasiTartil= getIntPart(segmen, koma);
    cfg.folder      = getIntPart(segmen, koma);

    String listStr = segmen.substring(koma);
    for (int i = 0; i < 3; i++) {
      int dash = listStr.indexOf('-');
      if (dash == -1 && listStr.length() > 0) {
        cfg.list[i] = listStr.toInt();
        break;
      } else if (dash != -1) {
        cfg.list[i] = listStr.substring(0, dash).toInt();
        listStr = listStr.substring(dash + 1);
      } else {
        cfg.list[i] = 0;
      }
    }
    saveToEEPROM();
  }
 if (data.startsWith("PLAY:")) {
  int idx = 5;
  byte folder = getIntPart(data, idx);
  byte file = getIntPart(data, idx);
  if (folder >= 1 && folder <= MAX_FOLDER && file >= 1 && file < MAX_FILE) {
    uint16_t durasi = getDurasiTartil(folder, file);
    if (durasi > 0) {
      dfplayer.playFolder(folder, file);
      Serial.print("Memutar manual: folder "); Serial.print(folder);
      Serial.print(", file "); Serial.print(file);
      Serial.print(", durasi "); Serial.print(durasi); Serial.println(" detik");
      digitalWrite(RELAY_PIN, HIGH);
      tartilMulaiMillis = millis();
      tartilDurasi = durasi * 1000;
      tartilSedangDiputar = false;
      adzanSedangDiputar = false;
      manualSedangDiputar = true;
      relayMenungguMati = false; // Jangan matikan relay langsung, tunggu durasi selesai
    } else {
      Serial.println("Durasi file 0, tidak diputar.");
    }
  }
  return;
}

if (data.startsWith("STOP")) {
  dfplayer.stop();
  digitalWrite(RELAY_PIN, LOW);
  relayMenungguMati = false;
  tartilSedangDiputar = false;
  adzanSedangDiputar = false;
  manualSedangDiputar = false;
  Serial.println("STOP: DFPlayer dan relay dimatikan");
  return;
}
}
// Tambahkan fungsi ini ke dalam loop()
void cekSelesaiManual() {
  if (manualSedangDiputar) {
    if (millis() - tartilMulaiMillis >= tartilDurasi) {
      manualSedangDiputar = false;
      dfplayer.stop();
      relayOffDelayMillis = millis();
      relayMenungguMati = true;
      Serial.println("Manual selesai, relay akan dimatikan setelah delay.");
      Serial.println("Manual selesai, DFPlayer dan relay akan dimatikan setelah delay.");
    }
  }
}



int getIntPart(String &s, int &pos) {
  int comma = s.indexOf(',', pos);
  if (comma == -1) comma = s.length();
  int val = s.substring(pos, comma).toInt();
  pos = comma + 1;
  return val;
}

uint16_t getDurasiTartil(byte folder, byte file) {
  if (folder == 0 || folder > MAX_FOLDER || file >= MAX_FILE) return 0;
  return durasiTartil[folder - 1][file];
}

uint16_t getDurasiAdzan(byte file) {
  if (file == 0 || file >= MAX_FILE) return 0;
  return durasiAdzan[file];
}

void cekDanPutarSholatNonBlocking() {
  int hariIdx = currentDay;
  for (int w = 0; w < WAKTU_TOTAL; w++) {
    WaktuConfig &cfg = jadwal[hariIdx][w];
    if (!cfg.aktif || tartilSedangDiputar || adzanSedangDiputar) continue;
    uint16_t totalDurasiTartil = 0;
    tartilCount = 0;
    for (byte i = 0; i < 3; i++) {
      byte listFile = cfg.list[i];
      uint16_t durasi = getDurasiTartil(cfg.folder, listFile);
      if (listFile > 0 && durasi > 0) {
        tartilList[tartilCount++] = listFile;
        totalDurasiTartil += durasi;
      }
    }
    int nowDetik = hour() * 3600 + minute() * 60 + second();
    int targetDetik = jamSholat[w] * 3600 + menitSholat[w] * 60 - totalDurasiTartil;
    if (nowDetik == targetDetik && !sudahEksekusi) {
      digitalWrite(RELAY_PIN, HIGH);
      if (cfg.tartilDulu && tartilCount > 0) {
        tartilIndex = 0;
        tartilFolder = cfg.folder;
        currentCfg = &cfg;
        byte fileToPlay = tartilList[tartilIndex];
        uint16_t durasi = getDurasiTartil(tartilFolder, fileToPlay);
        dfplayer.playFolder(tartilFolder, fileToPlay);
        Serial.print("Memutar tartil: folder "); Serial.print(tartilFolder);
        Serial.print(", file "); Serial.print(fileToPlay);
        Serial.print(", durasi "); Serial.print(durasi); Serial.println(" detik");
        tartilMulaiMillis = millis();
        tartilDurasi = durasi * 1000;
        tartilSedangDiputar = true;
      } else if (cfg.aktifAdzan) {
        dfplayer.playFolder(11, cfg.fileAdzan);
        adzanMulaiMillis = millis();
        adzanDurasi = getDurasiAdzan(cfg.fileAdzan) * 1000;
        adzanSedangDiputar = true;
        Serial.println("Memutar adzan.");
      } else {
        Serial.println("Tidak ada audio, menunggu relay mati.");
      }
      lastTriggerMillis = millis();
      sudahEksekusi = true;
    }
  }
}

void cekSelesaiTartil() {
  if (!tartilSedangDiputar) return;
  if (millis() - tartilMulaiMillis >= tartilDurasi) {
    tartilIndex++;
    if (tartilIndex < tartilCount) {
      byte fileToPlay = tartilList[tartilIndex];
      uint16_t durasi = getDurasiTartil(tartilFolder, fileToPlay);
      dfplayer.playFolder(tartilFolder, fileToPlay);
      Serial.print("Memutar tartil berikutnya: folder "); Serial.print(tartilFolder);
      Serial.print(", file "); Serial.print(fileToPlay);
      Serial.print(", durasi "); Serial.print(durasi); Serial.println(" detik");
      tartilMulaiMillis = millis();
      tartilDurasi = durasi * 1000;
    } else {
      tartilSedangDiputar = false;
      if (currentCfg && currentCfg->aktifAdzan) {
        dfplayer.playFolder(11, currentCfg->fileAdzan);
        adzanMulaiMillis = millis();
        adzanDurasi = getDurasiAdzan(currentCfg->fileAdzan) * 1000;
        adzanSedangDiputar = true;
        Serial.println("Memutar adzan setelah tartil.");
      } else {
        Serial.println("Tartil selesai, relay akan dimatikan.");
      }
    }
  }
}




void cekRelayOffDelay() {
  if (relayMenungguMati && millis() - relayOffDelayMillis >= 5000) {
    digitalWrite(RELAY_PIN, LOW);
    Serial.println("cekRelayOffDelay");
    relayMenungguMati = false;
    manualSedangDiputar = false;
  }
}
void cekSelesaiAdzan() {
  if (!adzanSedangDiputar) return;
  //Serial.println("selesai adzan aktif");
  if (millis() - adzanMulaiMillis >= adzanDurasi) {
    adzanSedangDiputar = false;
    digitalWrite(RELAY_PIN, LOW); // Matikan relay setelah adzan selesai
    Serial.println("selesai adzan aktif");
  }
}

void getStatusRun(){
  unsigned long currentMillis = millis();
    static unsigned long previousMillis;
    static unsigned long interval=5000;
    // Mengatur LED dengan waveLED jika interval waktu telah tercapai
    if (currentMillis - previousMillis >= interval / 256) {  // Menggunakan interval/256 sesuai logika waveLED
      previousMillis = currentMillis;  // Perbarui waktu sebelumnya
      interval = waveLED(0, interval);  // Panggil fungsi waveLED dan dapatkan interval berikutnya
      //Serial.println("interval:"+String(interval));
    }
}

uint32_t waveLED(uint32_t, unsigned breathePeriod) {
    uint32_t brightness = (m_Counter < 128) ? m_Counter : 255 - m_Counter;

    setLED(DIMM(brightness * 2));  // Mengatur LED dengan kecerahan yang dihitung

    // Menggulung nilai m_Counter antara 0 hingga 255
    m_Counter = (m_Counter + 1) % 256;
    
    // Mengembalikan nilai interval (delay) untuk satu iterasi
    return breathePeriod;
}

void setLED(uint8_t brightness) {
  analogWrite(RUN_LED, brightness);
}

void loadFromEEPROM() {
  int addr = 0;
  EEPROM.get(addr, jadwal); addr += sizeof(jadwal);
  EEPROM.get(addr, durasiAdzan); addr += sizeof(durasiAdzan);
  EEPROM.get(addr, durasiTartil); addr += sizeof(durasiTartil);
  EEPROM.get(addr, volumeDFPlayer); addr += sizeof(volumeDFPlayer);
}

void saveToEEPROM() {
  int addr = 0;
  EEPROM.put(addr, jadwal); addr += sizeof(jadwal);
  EEPROM.put(addr, durasiAdzan); addr += sizeof(durasiAdzan);
  EEPROM.put(addr, durasiTartil); addr += sizeof(durasiTartil);
  EEPROM.put(addr, volumeDFPlayer); addr += sizeof(volumeDFPlayer);
}
