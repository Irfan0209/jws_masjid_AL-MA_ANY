/*
  Auto Tartil System with Arduino UNO + ESP8266 + DFPlayer Mini
  --------------------------------------------------------------
  - ESP8266: menerima konfigurasi dari App Inventor via AP + HTTP GET
  - Arduino UNO: membaca konfigurasi via Serial, menyimpan konfigurasi dalam array
  - Arduino mengatur jadwal berdasarkan waktu dan memutar rekaman tartil dan adzan
  - DFPlayer Mini: memainkan file tartil & adzan
*/

#include <SoftwareSerial.h>
#include <DFRobotDFPlayerMini.h>
#include <TimeLib.h>  //<RTClib.h>
#include <EEPROM.h>

SoftwareSerial dfSerial(10, 11); // RX, TX ke DFPlayer
DFRobotDFPlayerMini dfplayer;
//RTC_DS3231 rtc;

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
byte volumeDFPlayer = 25;

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

void setup() {
  Serial.begin(9600);
  dfSerial.begin(9600);
  //rtc.begin();
  loadFromEEPROM();

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
  if (data.startsWith("VOL:")) {
    volumeDFPlayer = data.substring(4).toInt();
    dfplayer.volume(volumeDFPlayer);
    saveToEEPROM();
    return;
  }

  // Format: HR:<hari>|W:<waktu>,aktif,aktifAdzan,fileAdzan,tartilDulu,durasiTartil,folder,list1-list2-list3
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
  //DateTime now = rtc.now();
  int hariIdx = weekday();//now.dayOfTheWeek();

  for (int w = 0; w < WAKTU_TOTAL; w++) {
    if (hour() == jamSholat[w] && minute() == menitSholat[w] && !sudahEksekusi) {
      WaktuConfig &cfg = jadwal[hariIdx][w];
      if (!cfg.aktif || tartilSedangDiputar) return;

      if (cfg.tartilDulu) {
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
      }
    }
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
