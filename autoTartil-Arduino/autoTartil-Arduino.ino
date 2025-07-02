/*
  Auto Tart.tick();m with Arduino Micro + ESP8266 + DFPlayer Mini
  --------------------------------------------------------------
  - ESP8266: menerima konfigurasi dari App Inventor via AP + HTTP GET
  - Arduino micro: membaca konfigurasi via Serial, menyimpan konfigurasi dalam array
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
#define NORMAL_STATUS_LED 12
#define NORMAL_LED 11
#define SETTING_LED 10
#define RUN_LED   9

#define NORMAL_BUTTON 8
#define SETTING_BUTTON 7
#define RESTART_BUTTON  6

#define EEPROM_MAGIC 0x42 // Tanda bahwa EEPROM sudah pernah diinisialisasi
#define EEPROM_ADDR_MAGIC 1000  // Alamat terakhir (disesuaikan agar tidak tabrakan)

OneButton butt_normal(NORMAL_BUTTON, true);
OneButton butt_setting(SETTING_BUTTON, true);
OneButton butt_restart(RESTART_BUTTON, true);

#define HARI_TOTAL 8 // 7 hari + SemuaHari (index ke-7)
#define WAKTU_TOTAL 5
#define MAX_FILE 10
#define MAX_FOLDER 5

struct WaktuConfig {
  byte aktif;
  byte aktifAdzan;
  byte fileAdzan;
  byte tartilDulu;
  uint8_t durasiTartil;
  byte folder;
  byte list[3];
};

WaktuConfig jadwal[HARI_TOTAL][WAKTU_TOTAL];
uint8_t durasiAdzan[MAX_FILE];
uint8_t durasiTartil[MAX_FOLDER][MAX_FILE];
byte volumeDFPlayer = 20;

uint8_t jamSholat[WAKTU_TOTAL] = {4, 12, 15, 18, 19};
uint8_t menitSholat[WAKTU_TOTAL] = {30, 0, 30, 0, 30};

bool tartilSedangDiputar = false;
uint32_t tartilMulaiMillis = 0;
uint16_t tartilDurasi = 0;
byte tartilFolder = 0;
byte tartilIndex = 0;
byte tartilList[3];
byte tartilCount = 0;
WaktuConfig *currentCfg = nullptr;

uint32_t lastTriggerMillis = 0;
bool sudahEksekusi = false;
bool adzanSedangDiputar = false;
uint32_t adzanMulaiMillis = 0;
uint16_t adzanDurasi = 0;

byte currentDay = 0;

// Tambahan untuk relay delay dan manual
uint32_t relayOffDelayMillis = 0;
bool relayMenungguMati = false;
bool manualSedangDiputar = false;

//variabel untuk led staus run
//#define BOARD_LED_BRIGHTNESS 255  // Kecerahan maksimum 244 dari 255
//#define DIMM(x) ((uint32_t)(x) * (BOARD_LED_BRIGHTNESS) / 255)
//uint8_t m_Counter = 0;   // Penghitung 8-bit untuk efek breathe
// Variabel global yang dibutuhkan
static uint8_t m_Counter = 0;
static uint16_t waveStepDelay = 20;  // Delay antar frame LED breathing (ms)
static uint32_t lastWaveMillis = 0;

//variabel mode 
bool STATUS_MODE = false;

bool lastStatusMode = !STATUS_MODE;     // agar langsung update saat pertama kali
bool lastNormalStatus = false;

//variabel untuk mengecek status normal
uint32_t lastTimeReceived = 0;
const uint16_t TIMEOUT_INTERVAL = 70000; // 70 detik, lebih dari 1 menit


void setup() {
  pinMode(RUN_LED, OUTPUT);
  pinMode(NORMAL_LED, OUTPUT);
  pinMode(SETTING_LED, OUTPUT);
  pinMode(NORMAL_STATUS_LED, OUTPUT);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW); // Awal mati

  butt_normal.attachClick(NORMAL);
  butt_setting.attachClick(SETTING);
  butt_restart.attachClick(RESTART);
  
  Serial.begin(9600);
  dfSerial.begin(9600);
 // delay(2000);
  
  

  if (!dfplayer.begin(dfSerial)) {
    Serial.println("DFPlayer tidak terdeteksi!");
    while (1);
  }
  dfplayer.volume(volumeDFPlayer);
  Serial.println("Sistem Auto Tartil Siap.");
  loadFromEEPROM();
  delay(2000);
//  durasiTartil[0][0] = 0; 
//  durasiTartil[0][1] = 20;// Folder 1, File 1 = 20 detik
//  durasiTartil[0][2] = 40;
//  durasiTartil[0][3] = 100;
//  durasiAdzan[1]     = 16;
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
  cekStatusSystem();

  getStatusRun();
  butt_normal.tick();
  butt_setting.tick();
  butt_restart.tick();
}

void bacaDataSerial() {
  static String buffer = "";
  while (Serial.available()) {
    char c = Serial.read();
    Serial.print(c); // DEBUG: tampilkan semua karakter yang diterima
    if (c == '\n') {
      Serial.println("\n>> Memanggil parseData()");
      parseData(buffer);
      buffer = "";
    } else {
      buffer += c;
    }
  }
}

void cekStatusSystem() {
  // Handle status mode (SETTING vs NORMAL)
  if (STATUS_MODE != lastStatusMode) {
    digitalWrite(SETTING_LED, STATUS_MODE);
    digitalWrite(NORMAL_LED, !STATUS_MODE);
    lastStatusMode = STATUS_MODE;
  }

  // Hanya cek koneksi waktu jika bukan mode setting
  if (!STATUS_MODE) {
    bool status = (millis() - lastTimeReceived <= TIMEOUT_INTERVAL);
    if (status != lastNormalStatus) {
      digitalWrite(NORMAL_STATUS_LED, status);
      lastNormalStatus = status;
    }
  }
}

void parseData(String data) {
  Serial.println("DATA: " + data);

  // --- Parsing TIME ---
  if (data.startsWith("TIME:")) {
    int idx = 5;
    uint8_t jam = getIntPart(data, idx);
    uint8_t menit = getIntPart(data, idx);
    uint8_t hari = getIntPart(data, idx);
    if (jam < 24 && menit < 60 && hari < 7) {
      setTime(jam, menit, 0, 1, 1, 2024);
      currentDay = hari;
      lastTimeReceived = millis(); // perbarui waktu koneksi
    }
    return;
  }

  // --- Parsing VOL ---
  if (data.startsWith("VOL:")) {
    volumeDFPlayer = data.substring(4).toInt();
    dfplayer.volume(volumeDFPlayer);
    saveToEEPROM();
    return;
  }

  // --- Parsing HR (jadwal harian) ---
  if (data.startsWith("HR:")) {
    int hariEnd = data.indexOf('|');
    if (hariEnd == -1) return;

    int hari = data.substring(3, hariEnd).toInt();
    if (hari < 0 || hari >= HARI_TOTAL) return;

    for (int w = 0; w < WAKTU_TOTAL; w++) {
      String tag = "|W" + String(w) + ":";
      int idxW = data.indexOf(tag);
      if (idxW == -1) continue;

      int pos = idxW + tag.length();
      WaktuConfig &cfg = jadwal[hari][w];

      cfg.aktif        = getIntPart(data, pos);
      cfg.aktifAdzan   = getIntPart(data, pos);
      cfg.fileAdzan    = getIntPart(data, pos);
      cfg.tartilDulu   = getIntPart(data, pos);
      cfg.durasiTartil = getIntPart(data, pos);
      cfg.folder       = getIntPart(data, pos);

      // Parsing 3 file list tartil (dipisah '-')
      for (int i = 0; i < 3; i++) {
        int dash = data.indexOf('-', pos);
        if (dash == -1) {
          cfg.list[i] = data.substring(pos).toInt();
          break;
        }
        cfg.list[i] = data.substring(pos, dash).toInt();
        pos = dash + 1;
      }
    }

    saveToEEPROM();
    return;
  }

  // --- Perintah PLAY ---
  if (data.startsWith("PLAY:")) {
    int idx = 5;
    byte folder = getIntPart(data, idx);
    byte file   = getIntPart(data, idx);
    if (folder >= 1 && folder <= MAX_FOLDER && file >= 1 && file < MAX_FILE) {
      uint16_t durasi = getDurasiTartil(folder, file);
      if (durasi > 0) {
        dfplayer.playFolder(folder, file);
        Serial.print("Memutar manual: folder "); Serial.print(folder);
        Serial.print(", file "); Serial.print(file);
        Serial.print(", durasi "); Serial.print(durasi); Serial.println(" detik");

        digitalWrite(RELAY_PIN, HIGH);
        tartilMulaiMillis    = millis();
        tartilDurasi         = durasi * 1000;
        tartilSedangDiputar  = false;
        adzanSedangDiputar   = false;
        manualSedangDiputar  = true;
        relayMenungguMati    = false;
      } else {
        Serial.println("Durasi file 0, tidak diputar.");
      }
    }
    return;
  }

  // --- Perintah STOP ---
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

  if (data.startsWith("NAMAFILE:")) {
  int idx = 9;
  String nama = data.substring(idx, data.indexOf(':', idx));
  idx = data.indexOf(':', idx) + 1;
  byte folder = getIntPart(data, idx);
  byte list = getIntPart(data, idx);
  byte durasi = getIntPart(data, idx);

  if (folder < MAX_FOLDER && list < MAX_FILE) {
    durasiTartil[folder][list] = durasi;
    Serial.print("Disimpan durasi tartil: ");
    Serial.print(nama); Serial.print(" => Folder ");
    Serial.print(folder); Serial.print(", List ");
    Serial.print(list); Serial.print(", Durasi ");
    Serial.print(durasi); Serial.println(" detik");
    saveToEEPROM();
  } else {
    Serial.println("Folder atau List melebihi batas.");
  }
  return;
}

if (data.startsWith("ADZAN:")) {
  int idx = 6;
  byte file = getIntPart(data, idx);
  byte durasi = getIntPart(data, idx);
  if (file < MAX_FILE) {
    durasiAdzan[file] = durasi;
    Serial.print("Disimpan durasi adzan file ");
    Serial.print(file); Serial.print(" = ");
    Serial.print(durasi); Serial.println(" detik");
    saveToEEPROM();
  }
  return;
}

}

//void parseData(String data) {
//  /*if (data.startsWith("TIME:")) {
//    int idx = 5;
//    uint8_t jam = getIntPart(data, idx);
//    uint8_t menit = getIntPart(data, idx);
//    uint8_t hari = getIntPart(data, idx);
//    if (jam < 24 && menit < 60 && hari < 7) {
//      setTime(jam, menit, 0, 1, 1, 2024);
//      currentDay = hari;
//    }
//    return;
//  }*/
//  Serial.println("DATA: " + String(data));
//  if (data.startsWith("TIME:")) {
//  int idx = 5;
//  uint8_t jam = getIntPart(data, idx);
//  uint8_t menit = getIntPart(data, idx);
//  uint8_t hari = getIntPart(data, idx);
//  if (jam < 24 && menit < 60 && hari < 7) {
//    setTime(jam, menit, 0, 1, 1, 2024);
//    currentDay = hari;
//    lastTimeReceived = millis(); // Tambahkan baris ini
//  }
//  return;
//}
//
//
//  if (data.startsWith("VOL:")) {
//    volumeDFPlayer = data.substring(4).toInt();
//    dfplayer.volume(volumeDFPlayer);
//    saveToEEPROM();
//    return;
//  }
// if (data.startsWith("HR:")) {
//    int idxHR = data.indexOf("HR:");
//    //Serial.println("idxHR"+String(idxHR));
//    if (idxHR == -1) return;
//
//    int hari = data.substring(idxHR + 3, data.indexOf('|')).toInt();
//
//    // Versi fleksibel untuk |W0: dan |W:
//    for (int w = 0; w < WAKTU_TOTAL; w++) {
//      String tag1 = "|W" + String(w) + ":";
//      String tag2 = "|W:";
//      int idxW = data.indexOf(tag1);
//      int waktuIdx = w;
//      if (idxW == -1) {
//        idxW = data.indexOf(tag2);
//        if (idxW != -1) {
//          int temp = idxW + tag2.length();
//          waktuIdx = getIntPart(data, temp);
//          idxW = data.indexOf(tag2);
//        }
//      }
//      if (idxW == -1) continue;
//
//      int koma = idxW + ((data.indexOf(tag1) != -1) ? tag1.length() : tag2.length());
//
//      WaktuConfig &cfg = jadwal[hari][waktuIdx];
//      cfg.aktif       = getIntPart(data, koma);
//      cfg.aktifAdzan  = getIntPart(data, koma);
//      cfg.fileAdzan   = getIntPart(data, koma);
//      cfg.tartilDulu  = getIntPart(data, koma);
//      cfg.durasiTartil= getIntPart(data, koma);
//      cfg.folder      = getIntPart(data, koma);
//
//      String listStr = data.substring(koma);
//      for (int i = 0; i < 3; i++) {
//        int dash = listStr.indexOf('-');
//        if (dash == -1) {
//          cfg.list[i] = listStr.length() > 0 ? listStr.toInt() : 0;
//          listStr = "";
//        } else {
//          cfg.list[i] = listStr.substring(0, dash).toInt();
//          listStr = listStr.substring(dash + 1);
//        }
//      }
//    }
//    saveToEEPROM();
//    return;
//  }
//  /*if (data.startsWith("HR:")) {
//    int idxHR = data.indexOf("HR:");
//    Serial.println("Data HR diterima: " + idxHR); // Tambahan
//    if (idxHR == -1) return;
//
//    int hari = data.substring(idxHR + 3, data.indexOf('|')).toInt();
//    for (int w = 0; w < WAKTU_TOTAL; w++) {
//      String tag = "|W" + String(w) + ":";
//      int idxW = data.indexOf(tag);
//      if (idxW == -1) continue;
//      int koma = idxW + tag.length();
//
//      WaktuConfig &cfg = jadwal[hari][w];
//      cfg.aktif       = getIntPart(data, koma);
//      cfg.aktifAdzan  = getIntPart(data, koma);
//      cfg.fileAdzan   = getIntPart(data, koma);
//      cfg.tartilDulu  = getIntPart(data, koma);
//      cfg.durasiTartil= getIntPart(data, koma);
//      cfg.folder      = getIntPart(data, koma);
//
//      String listStr = data.substring(koma);
//      for (int i = 0; i < 3; i++) {
//        int dash = listStr.indexOf('-');
//        if (dash == -1) {
//          cfg.list[i] = listStr.length() > 0 ? listStr.toInt() : 0;
//          listStr = "";
//        } else {
//          cfg.list[i] = listStr.substring(0, dash).toInt();
//          listStr = listStr.substring(dash + 1);
//        }
//      }
//    }
//    saveToEEPROM();
//    return;
//  }*/
///*
//  if (data.startsWith("VOL:")) {
//    volumeDFPlayer = data.substring(4).toInt();
//    dfplayer.volume(volumeDFPlayer);
//    saveToEEPROM();
//    return;
//  }
//
//  if (data.startsWith("HR:")) {
//    int idxHR = data.indexOf("HR:");
//    int idxW = data.indexOf("|W:");
//    if (idxHR == -1 || idxW == -1) return;
//
//    int hari = data.substring(idxHR + 3, idxW).toInt();
//    String segmen = data.substring(idxW + 3);
//
//    int koma = 0;
//    byte waktuIdx = getIntPart(segmen, koma);
//    if (waktuIdx >= WAKTU_TOTAL) return;
//
//    WaktuConfig &cfg = jadwal[hari][waktuIdx];
//    cfg.aktif       = getIntPart(segmen, koma);
//    cfg.aktifAdzan  = getIntPart(segmen, koma);
//    cfg.fileAdzan   = getIntPart(segmen, koma);
//    cfg.tartilDulu  = getIntPart(segmen, koma);
//    cfg.durasiTartil= getIntPart(segmen, koma);
//    cfg.folder      = getIntPart(segmen, koma);
//
//    String listStr = segmen.substring(koma);
//    for (int i = 0; i < 3; i++) {
//      int dash = listStr.indexOf('-');
//      if (dash == -1 && listStr.length() > 0) {
//        cfg.list[i] = listStr.toInt();
//        break;
//      } else if (dash != -1) {
//        cfg.list[i] = listStr.substring(0, dash).toInt();
//        listStr = listStr.substring(dash + 1);
//      } else {
//        cfg.list[i] = 0;
//      }
//    }
//    saveToEEPROM();
//  }
//  */
// if (data.startsWith("PLAY:")) {
//  int idx = 5;
//  byte folder = getIntPart(data, idx);
//  byte file = getIntPart(data, idx);
//  if (folder >= 1 && folder <= MAX_FOLDER && file >= 1 && file < MAX_FILE) {
//    uint16_t durasi = getDurasiTartil(folder, file);
//    if (durasi > 0) {
//      dfplayer.playFolder(folder, file);
//      Serial.print("Memutar manual: folder "); Serial.print(folder);
//      Serial.print(", file "); Serial.print(file);
//      Serial.print(", durasi "); Serial.print(durasi); Serial.println(" detik");
//      digitalWrite(RELAY_PIN, HIGH);
//      tartilMulaiMillis = millis();
//      tartilDurasi = durasi * 1000;
//      tartilSedangDiputar = false;
//      adzanSedangDiputar = false;
//      manualSedangDiputar = true;
//      relayMenungguMati = false; // Jangan matikan relay langsung, tunggu durasi selesai
//    } else {
//      Serial.println("Durasi file 0, tidak diputar.");
//    }
//  }
//  return;
//}
//
//if (data.startsWith("STOP")) {
//  dfplayer.stop();
//  digitalWrite(RELAY_PIN, LOW);
//  relayMenungguMati = false;
//  tartilSedangDiputar = false;
//  adzanSedangDiputar = false;
//  manualSedangDiputar = false;
//  Serial.println("STOP: DFPlayer dan relay dimatikan");
//  return;
//}
//}
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

/*
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
*/
void cekDanPutarSholatNonBlocking() {
  if (tartilSedangDiputar || adzanSedangDiputar) return;

  uint32_t now = hour() * 3600UL + minute() * 60UL + second();

  for (byte w = 0; w < WAKTU_TOTAL; w++) {
    WaktuConfig &cfg = jadwal[currentDay][w];
    if (!cfg.aktif) continue;

    tartilCount = 0;
    uint16_t totalDurasi = 0;

    for (byte i = 0; i < 3; i++) {
      byte f = cfg.list[i];
      if (f) {
        uint16_t d = getDurasiTartil(cfg.folder, f);
        if (d) {
          tartilList[tartilCount++] = f;
          totalDurasi += d;
        }
      }
    }

    uint32_t jadwalDetik = (uint32_t)jamSholat[w] * 3600UL + (uint32_t)menitSholat[w] * 60UL;
    if (now == jadwalDetik - totalDurasi && !sudahEksekusi) {
      digitalWrite(RELAY_PIN, HIGH);

      if (cfg.tartilDulu && tartilCount > 0) {
        tartilIndex = 0;
        tartilFolder = cfg.folder;
        currentCfg = &cfg;

        byte f = tartilList[0];
        tartilDurasi = getDurasiTartil(tartilFolder, f) * 1000UL;
        tartilMulaiMillis = millis();
        tartilSedangDiputar = true;
        dfplayer.playFolder(tartilFolder, f);
      } else if (cfg.aktifAdzan) {
        adzanDurasi = getDurasiAdzan(cfg.fileAdzan) * 1000UL;
        adzanMulaiMillis = millis();
        adzanSedangDiputar = true;
        dfplayer.playFolder(11, cfg.fileAdzan);
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
      byte f = tartilList[tartilIndex];
      tartilDurasi = getDurasiTartil(tartilFolder, f) * 1000UL;
      tartilMulaiMillis = millis();
      dfplayer.playFolder(tartilFolder, f);
    } else {
      tartilSedangDiputar = false;

      if (currentCfg && currentCfg->aktifAdzan) {
        adzanDurasi = getDurasiAdzan(currentCfg->fileAdzan) * 1000UL;
        adzanMulaiMillis = millis();
        adzanSedangDiputar = true;
        dfplayer.playFolder(11, currentCfg->fileAdzan);
      } else {
        // Tidak ada adzan, artinya bisa matikan relay atau lanjut ke idle
        // (aksi selanjutnya bisa kamu tambahkan sendiri jika perlu)
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
    dfplayer.stop();
    adzanSedangDiputar = false;
    digitalWrite(RELAY_PIN, LOW); // Matikan relay setelah adzan selesai
    Serial.println("selesai adzan aktif");
  }
}

void getStatusRun() {
  uint32_t now = millis();
  if (now - lastWaveMillis >= waveStepDelay) {
    lastWaveMillis = now;
    updateWaveLED();
  }
}

void updateWaveLED() {
  // brightness naik turun dari 0 - 255 - 0
  uint8_t brightness = (m_Counter < 128) ? m_Counter * 2 : (255 - m_Counter) * 2;
  setLED(brightness);

  m_Counter = (m_Counter + 1) % 256;  // loop kembali ke 0 setelah 255
}

void setLED(uint8_t brightness) {
  analogWrite(RUN_LED, brightness);
}

/*
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
}*/

void NORMAL(){
  STATUS_MODE = false;
  Serial.println("SETTING=0");
}

void SETTING(){
  STATUS_MODE = true;
  Serial.println("SETTING=1");
}

void RESTART(){
  Serial.println("restart=1");
}
/*
void loadFromEEPROM() {
  int addr = 0;
  EEPROM.get(addr, jadwal); addr += sizeof(jadwal);
  EEPROM.get(addr, durasiAdzan); addr += sizeof(durasiAdzan);
  EEPROM.get(addr, durasiTartil); addr += sizeof(durasiTartil);
  EEPROM.get(addr, volumeDFPlayer); addr += sizeof(volumeDFPlayer);
  Serial.println(jadwal));
   Serial.println("durasiAdzan:" + String(durasiAdzan));
    Serial.println("durasiTartil:" + String(durasiTartil));
     Serial.println("volumeDFPlayer:" + String(volumeDFPlayer));
}

void saveToEEPROM() {
  int addr = 0;
  EEPROM.put(addr, jadwal); addr += sizeof(jadwal);
  EEPROM.put(addr, durasiAdzan); addr += sizeof(durasiAdzan);
  EEPROM.put(addr, durasiTartil); addr += sizeof(durasiTartil);
  EEPROM.put(addr, volumeDFPlayer); addr += sizeof(volumeDFPlayer);
}
*/
/*
// Tambahan fungsi untuk menyimpan dan membaca konfigurasi dari EEPROM
void saveToEEPROM() {
  Serial.println("Data berhasil disimpan ke EEPROM!");
  int addr = 0;
  for (int h = 0; h < HARI_TOTAL; h++) {
    for (int w = 0; w < WAKTU_TOTAL; w++) {
      EEPROM.put(addr, jadwal[h][w]);
      addr += sizeof(WaktuConfig);
    }
  }
  for (int i = 0; i < MAX_FILE; i++) {
    EEPROM.write(addr, durasiAdzan[i]);
    addr += sizeof(uint16_t);
  }
  for (int f = 0; f < MAX_FOLDER; f++) {
    for (int i = 0; i < MAX_FILE; i++) {
      EEPROM.write(addr, durasiTartil[f][i]);
      addr += sizeof(uint16_t);
    }
  }
  EEPROM.write(addr, volumeDFPlayer);
   
}

void loadFromEEPROM() {
  int addr = 0;
  Serial.println("Memuat konfigurasi dari EEPROM...");

  for (int h = 0; h < HARI_TOTAL; h++) {
    for (int w = 0; w < WAKTU_TOTAL; w++) {
      EEPROM.get(addr, jadwal[h][w]);
      addr += sizeof(WaktuConfig);

      // Tampilkan konfigurasi waktu
      Serial.print("HR:"); Serial.print(h);
      Serial.print(" W"); Serial.print(w);
      Serial.print(" Aktif:"); Serial.print(jadwal[h][w].aktif);
      Serial.print(" Adzan:"); Serial.print(jadwal[h][w].aktifAdzan);
      Serial.print(" FileAdzan:"); Serial.print(jadwal[h][w].fileAdzan);
      Serial.print(" TartilDulu:"); Serial.print(jadwal[h][w].tartilDulu);
      Serial.print(" DurasiTartil:"); Serial.print(jadwal[h][w].durasiTartil);
      Serial.print(" Folder:"); Serial.print(jadwal[h][w].folder);
      Serial.print(" List:");
      for (int i = 0; i < 3; i++) {
        Serial.print(jadwal[h][w].list[i]);
        if (i < 2) Serial.print("-");
      }
      Serial.println();
    }
  }

  Serial.println("Durasi Adzan:");
  for (int i = 0; i < MAX_FILE; i++) {
    EEPROM.get(addr, durasiAdzan[i]);
    Serial.print("Adzan["); Serial.print(i); Serial.print("] = ");
    Serial.println(durasiAdzan[i]);
    addr += sizeof(uint16_t);
  }

  Serial.println("Durasi Tartil:");
  for (int f = 0; f < MAX_FOLDER; f++) {
    for (int i = 0; i < MAX_FILE; i++) {
      EEPROM.get(addr, durasiTartil[f][i]);
      Serial.print("Tartil["); Serial.print(f); Serial.print("]["); Serial.print(i); Serial.print("] = ");
      Serial.println(durasiTartil[f][i]);
      addr += sizeof(uint16_t);
    }
  }

  EEPROM.get(addr, volumeDFPlayer);
  Serial.print("Volume DFPlayer = ");
  Serial.println(volumeDFPlayer);

  Serial.println("Selesai memuat konfigurasi dari EEPROM.");
}*/


void saveToEEPROM() {
  Serial.println("Data berhasil disimpan ke EEPROM!");
  int addr = 0;
  for (int h = 0; h < HARI_TOTAL; h++) {
    for (int w = 0; w < WAKTU_TOTAL; w++) {
      EEPROM.put(addr, jadwal[h][w]);
      addr += sizeof(WaktuConfig);
    }
  }
  for (int i = 0; i < MAX_FILE; i++) {
    EEPROM.write(addr, durasiAdzan[i]);
    addr += sizeof(uint16_t);
  }
  for (int f = 0; f < MAX_FOLDER; f++) {
    for (int i = 0; i < MAX_FILE; i++) {
      EEPROM.write(addr, durasiTartil[f][i]);
      addr += sizeof(uint16_t);
    }
  }
  EEPROM.write(addr, volumeDFPlayer);
  addr += sizeof(volumeDFPlayer);

 EEPROM.write(EEPROM_ADDR_MAGIC, EEPROM_MAGIC); // Simpan magic number

}

void loadFromEEPROM() {
  if (EEPROM.read(EEPROM_ADDR_MAGIC) != EEPROM_MAGIC) {
    Serial.println("EEPROM belum diinisialisasi. Lewati load.");
    return;
  }

  Serial.println("Memuat konfigurasi dari EEPROM...");
  int addr = 0;
  for (int h = 0; h < HARI_TOTAL; h++) {
    for (int w = 0; w < WAKTU_TOTAL; w++) {
      EEPROM.get(addr, jadwal[h][w]);
      addr += sizeof(WaktuConfig);
      Serial.print("HR:"); Serial.print(h);
      Serial.print(" W"); Serial.print(w);
      Serial.print(" Aktif:"); Serial.print(jadwal[h][w].aktif);
      Serial.print(" Adzan:"); Serial.print(jadwal[h][w].aktifAdzan);
      Serial.print(" FileAdzan:"); Serial.print(jadwal[h][w].fileAdzan);
      Serial.print(" TartilDulu:"); Serial.print(jadwal[h][w].tartilDulu);
      Serial.print(" DurasiTartil:"); Serial.print(jadwal[h][w].durasiTartil);
      Serial.print(" Folder:"); Serial.print(jadwal[h][w].folder);
      Serial.print(" List:");
      Serial.print(jadwal[h][w].list[0]); Serial.print("-");
      Serial.print(jadwal[h][w].list[1]); Serial.print("-");
      Serial.println(jadwal[h][w].list[2]);
    }
  }
  for (int i = 0; i < MAX_FILE; i++) {
    EEPROM.get(addr, durasiAdzan[i]);
    addr += sizeof(uint16_t);
  }
  for (int f = 0; f < MAX_FOLDER; f++) {
    for (int i = 0; i < MAX_FILE; i++) {
      EEPROM.get(addr, durasiTartil[f][i]);
      addr += sizeof(uint16_t);
      Serial.print("Tartil["); Serial.print(f); Serial.print("]["); Serial.print(i);
      Serial.print("] = "); Serial.println(durasiTartil[f][i]);
    }
  }
  EEPROM.get(addr, volumeDFPlayer);
  Serial.print("Volume: "); Serial.println(volumeDFPlayer);
}
