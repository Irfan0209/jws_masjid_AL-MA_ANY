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

SoftwareSerial dfSerial(2, 3); // RX, TX ke DFPlayer
DFRobotDFPlayerMini dfplayer;

#define RELAY_PIN 13
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

void setup() {
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW); // Awal mati

  Serial.begin(9600);
  dfSerial.begin(9600);
 // loadFromEEPROM();
  durasiTartil[0][0] = 10; // Folder 1, File 1 = 10 detik
  durasiTartil[0][1] = 12;
  durasiTartil[0][2] = 8;
  durasiAdzan[0]     = 15;

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

void parseData(String data) {
  if (data.startsWith("TIME:")) {
    int idx = 5;
    uint8_t jam = getIntPart(data, idx);
    uint8_t menit = getIntPart(data, idx);
    uint8_t hari = getIntPart(data, idx);
    if (jam < 24 && menit < 60 && hari < 7) {
      setTime(jam, menit, 0, 1, 1, 2024);
      currentDay = hari;
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
      dfplayer.playFolder(folder, file);
      digitalWrite(RELAY_PIN, HIGH);
      relayOffDelayMillis = millis() + (getDurasiTartil(folder, file) * 1000);
      relayMenungguMati = true;
      manualSedangDiputar = true;
    }
    return;
  }
  if (data.startsWith("PAUSE")) {
    dfplayer.pause();
    return;
  }
  if (data.startsWith("STOP")) {
    dfplayer.stop();
    relayOffDelayMillis = millis();
    relayMenungguMati = true;
    tartilSedangDiputar = false;
    adzanSedangDiputar = false;
    manualSedangDiputar = false;
    return;
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
    for (int i = 0; i < 3; i++) {
      if (cfg.list[i] > 0) totalDurasiTartil += getDurasiTartil(cfg.folder, cfg.list[i]);
    }
    int nowDetik = hour() * 3600 + minute() * 60 + second();
    int targetDetik = jamSholat[w] * 3600 + menitSholat[w] * 60 - totalDurasiTartil;
    if (nowDetik == targetDetik && !sudahEksekusi) {
      digitalWrite(RELAY_PIN, HIGH);
      if (cfg.tartilDulu && totalDurasiTartil > 0) {
        tartilCount = 0;
        for (byte i = 0; i < 3; i++) { //rubah jumblah daftar list yang akan diputar
          if (cfg.list[i] > 0) tartilList[tartilCount++] = cfg.list[i];
        }
        if (tartilCount > 0) {
          tartilIndex = 0;
          tartilFolder = cfg.folder;
          currentCfg = &cfg;
          dfplayer.playFolder(tartilFolder, tartilList[tartilIndex]);
          tartilMulaiMillis = millis();
          tartilDurasi = getDurasiTartil(tartilFolder, tartilList[tartilIndex]) * 1000;
          tartilSedangDiputar = true;
          Serial.println("tartilCount:" + String(tartilCount));
        }
      } else if (cfg.aktifAdzan) {
        dfplayer.playFolder(11,cfg.fileAdzan);
        adzanMulaiMillis = millis();
        adzanDurasi = getDurasiAdzan(cfg.fileAdzan) * 1000;
        adzanSedangDiputar = true;
      } else {
        relayOffDelayMillis = millis();
        relayMenungguMati = true;
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
      dfplayer.playFolder(tartilFolder, tartilList[tartilIndex]);
      tartilMulaiMillis = millis();
      tartilDurasi = getDurasiTartil(tartilFolder, tartilList[tartilIndex]) * 1000;
    } else {
      tartilSedangDiputar = false;
      if (currentCfg && currentCfg->aktifAdzan) {
        dfplayer.playFolder(11,currentCfg->fileAdzan);
        adzanMulaiMillis = millis();
        adzanDurasi = getDurasiAdzan(currentCfg->fileAdzan) * 1000;
        adzanSedangDiputar = true;
      }
//      } else {
//        relayOffDelayMillis = millis();
//        relayMenungguMati = true;
//      }
    }
  }
}
/*
void cekDanPutarSholatNonBlocking() {
  int hariIdx = currentDay;

  for (int w = 0; w < WAKTU_TOTAL; w++) {
    WaktuConfig &cfg = jadwal[hariIdx][w];
    if (!cfg.aktif || tartilSedangDiputar || adzanSedangDiputar) continue;

    uint16_t totalDurasiTartil = 0;
    for (int i = 0; i < 3; i++) {
      if (cfg.list[i] > 0) {
        totalDurasiTartil += getDurasiTartil(cfg.folder, cfg.list[i]);
      }
    }

    int nowDetik = hour() * 3600 + minute() * 60 + second();
    int targetDetik = jamSholat[w] * 3600 + menitSholat[w] * 60 - totalDurasiTartil;

    if (nowDetik == targetDetik && !sudahEksekusi) {
      digitalWrite(RELAY_PIN, HIGH); // Aktifkan relay

      if (cfg.tartilDulu && totalDurasiTartil > 0) {
        tartilCount = 0;
        for (byte i = 0; i < 3; i++) {
          if (cfg.list[i] > 0) {
            tartilList[tartilCount++] = cfg.list[i];
          }
        }
        if (tartilCount > 0) {
          tartilIndex = 0;
          tartilFolder = cfg.folder;
          currentCfg = &cfg;
          dfplayer.playFolder(tartilFolder, tartilList[tartilIndex]);
          tartilMulaiMillis = millis();
          tartilDurasi = getDurasiTartil(tartilFolder, tartilList[tartilIndex]) * 1000;
          tartilSedangDiputar = true;
        }
      } else if (cfg.aktifAdzan) {
        dfplayer.play(cfg.fileAdzan);
        adzanMulaiMillis = millis();
        adzanDurasi = getDurasiAdzan(cfg.fileAdzan) * 1000;
        adzanSedangDiputar = true;
      } else {
        digitalWrite(RELAY_PIN, LOW); // Tidak ada audio, matikan relay
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
      dfplayer.playFolder(tartilFolder, tartilList[tartilIndex]);
      tartilMulaiMillis = millis();
      tartilDurasi = getDurasiTartil(tartilFolder, tartilList[tartilIndex]) * 1000;
    } else {
      tartilSedangDiputar = false;
      if (currentCfg && currentCfg->aktifAdzan) {
        dfplayer.play(currentCfg->fileAdzan);
        adzanMulaiMillis = millis();
        adzanDurasi = getDurasiAdzan(currentCfg->fileAdzan) * 1000;
        adzanSedangDiputar = true;
      } else {
        digitalWrite(RELAY_PIN, LOW); // Tidak ada adzan, matikan relay
      }
    }
  }
}*/

void cekRelayOffDelay() {
  if (relayMenungguMati && millis() - relayOffDelayMillis >= 3000) {
    digitalWrite(RELAY_PIN, LOW);
    relayMenungguMati = false;
    manualSedangDiputar = false;
  }
}
void cekSelesaiAdzan() {
  if (!adzanSedangDiputar) return;
  if (millis() - adzanMulaiMillis >= adzanDurasi) {
    adzanSedangDiputar = false;
    digitalWrite(RELAY_PIN, LOW); // Matikan relay setelah adzan selesai
  }
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
