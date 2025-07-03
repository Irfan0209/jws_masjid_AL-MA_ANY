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

#define RELAY_PIN         13
#define NORMAL_STATUS_LED 12
#define NORMAL_LED        11
#define SETTING_LED       10
#define RUN_LED           9

#define NORMAL_BUTTON   A0
#define SETTING_BUTTON  A1
#define RESTART_BUTTON  A2

#define EEPROM_MAGIC 0x42 // Tanda bahwa EEPROM sudah pernah diinisialisasi
#define EEPROM_ADDR_MAGIC 1000  // Alamat terakhir (disesuaikan agar tidak tabrakan)

OneButton butt_normal(NORMAL_BUTTON, true);
OneButton butt_setting(SETTING_BUTTON, true);
OneButton butt_restart(RESTART_BUTTON, true);

#define HARI_TOTAL  8 // 7 hari + SemuaHari (index ke-7)
#define WAKTU_TOTAL 5
#define MAX_FILE    25
#define MAX_FOLDER  3

struct WaktuConfig {
  byte aktif;
  byte aktifAdzan;
  byte fileAdzan;
  byte tartilDulu;
  byte folder;
  byte list[3];
};

WaktuConfig jadwal[HARI_TOTAL][WAKTU_TOTAL];
uint8_t durasiAdzan[MAX_FILE];
uint8_t durasiTartil[MAX_FOLDER][MAX_FILE];
byte volumeDFPlayer;
uint8_t jamSholat[WAKTU_TOTAL] = {4, 12, 15, 18, 19};
uint8_t menitSholat[WAKTU_TOTAL] = {30, 0, 30, 0, 30};

bool tartilSedangDiputar = false;
uint32_t tartilMulaiMillis = 0;
uint16_t tartilDurasi = 0;
byte tartilFolder = 0;
byte tartilIndex = 0;
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

//variabel untuk led status system
static uint8_t m_Counter = 0;
static uint16_t waveStepDelay = 20;  // Delay antar frame LED breathing (ms)
static uint32_t lastWaveMillis = 0;
bool STATUS_MODE = false;
bool lastStatusMode = !STATUS_MODE;     // agar langsung update saat pertama kali
bool lastNormalStatus = false;
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
 
  if (!dfplayer.begin(dfSerial)) {
    Serial.println("DFPlayer tidak terdeteksi!");
    while (1);
  }
  
  Serial.println("Sistem Auto Tartil Siap.");
  loadFromEEPROM();
  delay(2000);
  dfplayer.volume(volumeDFPlayer);
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
    //Serial.print(c); // DEBUG: tampilkan semua karakter yang diterima
    if (c == '\n') {
      //Serial.println("\n>> Memanggil parseData()");
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
  uint8_t jam    = getIntPart(data, idx);
  uint8_t menit  = getIntPart(data, idx);
  uint8_t detik  = getIntPart(data, idx);
  uint8_t hari   = getIntPart(data, idx);

  if (jam < 24 && menit < 60 && detik < 60 && hari < 7) {
    setTime(jam, menit, detik, 1, 1, 2024);
    currentDay = hari;
    lastTimeReceived = millis(); // perbarui waktu koneksi
    Serial.print(F("Waktu diatur ke: "));
    Serial.print(jam); Serial.print(":");
    Serial.print(menit); Serial.print(":");
    Serial.print(detik); Serial.print(" | Hari ke-");
    Serial.println(hari);
  } else {
    Serial.println(F("Format TIME tidak valid."));
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

//---- Program baru------//
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
    cfg.folder       = getIntPart(data, pos);

    // Lebih aman dan memastikan semua list[i] terisi
for (int i = 0; i < 3; i++) {
  int dash = data.indexOf('-', pos);
  if (dash != -1) {
    cfg.list[i] = data.substring(pos, dash).toInt();
    pos = dash + 1;
  } else {
    cfg.list[i] = data.substring(pos).toInt(); // pastikan tetap terisi jika dash tidak ada
    break;
  }
}

 for (int i = 0; i < 3; i++) {
  Serial.print("list["); Serial.print(i); Serial.print("] = ");
  Serial.println(cfg.list[i]);
}

  }

  saveToEEPROM();
  return;
}
//----------------------------//

//-----------------------//*/
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
/*
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
*/
if (data.startsWith("JWS:")) {
  String sisa = data.substring(4); // Hilangkan "JWS:"
  for (int i = 0; i < WAKTU_TOTAL; i++) {
    int komaIdx = sisa.indexOf(',');
    int pemisahIdx = sisa.indexOf('|');

    if (komaIdx == -1) break;
    jamSholat[i] = sisa.substring(0, komaIdx).toInt();

    if (pemisahIdx == -1) {
      // Tidak ada | berarti ini adalah elemen terakhir
      menitSholat[i] = sisa.substring(komaIdx + 1).toInt();
      break;
    } else {
      menitSholat[i] = sisa.substring(komaIdx + 1, pemisahIdx).toInt();
      sisa = sisa.substring(pemisahIdx + 1); // lanjut ke data berikutnya
    }
  }

  saveToEEPROM();
  Serial.println("Jadwal Sholat diperbarui:");
  for (int i = 0; i < WAKTU_TOTAL; i++) {
    Serial.print(" - Waktu "); Serial.print(i);
    Serial.print(": "); Serial.print(jamSholat[i]);
    Serial.print(":"); Serial.println(menitSholat[i]);
  }
  return;
}



}

//
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

//---- Program Baru------//
void cekDanPutarSholatNonBlocking() {
  if (tartilSedangDiputar || adzanSedangDiputar) return;

  uint32_t now = hour() * 3600UL + minute() * 60UL + second();

  for (byte w = 0; w < WAKTU_TOTAL; w++) {
    WaktuConfig &cfg = jadwal[currentDay][w];
    if (!cfg.aktif || sudahEksekusi) continue;

    uint16_t totalDurasi = 0;
    byte jumlahTartil = 0;

    // Hitung total durasi tartil langsung dari cfg.list
    for (byte i = 0; i < 3; i++) {
      byte f = cfg.list[i];
      if (f) {
        uint16_t d = getDurasiTartil(cfg.folder, f);
        if (d > 0) {
          totalDurasi += d;
          jumlahTartil++;
        }
      }
    }

     uint32_t jadwalDetik = (uint32_t)jamSholat[w] * 3600UL + (uint32_t)menitSholat[w] * 60UL;
     uint32_t triggerDetik = cfg.tartilDulu ? (jadwalDetik - totalDurasi) : jadwalDetik;

     if (now == triggerDetik && !sudahEksekusi) {

      digitalWrite(RELAY_PIN, HIGH);
      currentCfg = &cfg;

      if (cfg.tartilDulu && jumlahTartil > 0) {
        tartilIndex = 0;
        tartilFolder = cfg.folder;
        tartilSedangDiputar = true;

        byte f = cfg.list[tartilIndex];
        tartilDurasi = getDurasiTartil(tartilFolder, f) * 1000UL;
        tartilMulaiMillis = millis();
        dfplayer.playFolder(tartilFolder, f);

        Serial.println(F("Tartil dimulai."));
        Serial.print(F("Folder: ")); Serial.print(tartilFolder);
        Serial.print(F(" File: ")); Serial.println(f);
      } else if (cfg.aktifAdzan) {
        adzanDurasi = getDurasiAdzan(cfg.fileAdzan) * 1000UL;
        adzanMulaiMillis = millis();
        adzanSedangDiputar = true;
        dfplayer.playFolder(11, cfg.fileAdzan);
        Serial.println(F("Memutar adzan (tanpa tartil)."));
      }

      lastTriggerMillis = millis();
      sudahEksekusi = true;
    }
  }
}

void cekSelesaiTartil() {
  if (!tartilSedangDiputar || !currentCfg) return;

  if (millis() - tartilMulaiMillis >= tartilDurasi) {
    tartilIndex++;
    while (tartilIndex < 3) {
      byte nextFile = currentCfg->list[tartilIndex];
      if (nextFile > 0) {
        uint16_t durasi = getDurasiTartil(tartilFolder, nextFile);
        if (durasi > 0) {
          dfplayer.playFolder(tartilFolder, nextFile);
          tartilDurasi = durasi * 1000UL;
          tartilMulaiMillis = millis();
          Serial.print("Memutar tartil ke-");
          Serial.print(tartilIndex + 1);
          Serial.print(": folder "); Serial.print(tartilFolder);
          Serial.print(", file "); Serial.print(nextFile);
          Serial.print(", durasi "); Serial.println(durasi);
          return;
        }
      }
      tartilIndex++;
    }

    // Semua list selesai
    tartilSedangDiputar = false;

    if (currentCfg->aktifAdzan) {
      dfplayer.playFolder(11, currentCfg->fileAdzan);
      adzanDurasi = getDurasiAdzan(currentCfg->fileAdzan) * 1000UL;
      adzanMulaiMillis = millis();
      adzanSedangDiputar = true;
      Serial.println("Tartil selesai, memutar adzan.");
    } else {
      matikanSemuaAudio();
      Serial.println("Tartil selesai, tidak ada adzan. Relay akan dimatikan.");
    }
  }
}

void matikanSemuaAudio() {
  dfplayer.stop();
  digitalWrite(RELAY_PIN, LOW);
  relayMenungguMati = false;
  tartilSedangDiputar = false;
  adzanSedangDiputar = false;
  manualSedangDiputar = false;
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
    EEPROM.put(addr, durasiAdzan[i]);
    addr += sizeof(uint16_t);
  }
  for (int f = 0; f < MAX_FOLDER; f++) {
    for (int i = 0; i < MAX_FILE; i++) {
      EEPROM.put(addr, durasiTartil[f][i]);
      addr += sizeof(uint16_t);
    }
  }
  EEPROM.put(addr, volumeDFPlayer);
 addr += sizeof(volumeDFPlayer);


  for (int i = 0; i < WAKTU_TOTAL; i++) {
    EEPROM.write(addr++, jamSholat[i]);
    EEPROM.write(addr++, menitSholat[i]);
  }

#if defined(ESP8266) || defined(ESP32)
  EEPROM.commit(); // penting untuk board ESP
#endif

 

  EEPROM.write(EEPROM_ADDR_MAGIC, EEPROM_MAGIC);
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
    Serial.print("adzan["); Serial.print(i);
    Serial.print("] = "); Serial.println(durasiAdzan[i]);
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
  addr += sizeof(volumeDFPlayer);
  Serial.print("Volume: "); Serial.println(volumeDFPlayer);

  for (int i = 0; i < WAKTU_TOTAL; i++) {
  EEPROM.get(addr, jamSholat[i]); addr += sizeof(uint8_t);
  EEPROM.get(addr, menitSholat[i]); addr += sizeof(uint8_t);
  Serial.print("jamSholat["); Serial.print(i);
  Serial.print("] = "); Serial.println(jamSholat[i]);
  Serial.print("menitSholat["); Serial.print(i);
  Serial.print("] = "); Serial.println(menitSholat[i]);
}
}
