void cekPotensiometerVolume() {
  static uint32_t lastCheck = 0;

  if (millis() - lastCheck >= 1000) {  // Cek tiap 1 detik
    lastCheck = millis();

    uint32_t sum = 0;
    // 50 sampel tanpa delay di dalam loop for
    for (uint8_t i = 0; i < 80; i++) {
        sum += analogRead(PIN_POT);
    }
    uint16_t averageRaw = sum / 80;

    uint8_t vol = map(averageRaw, 0, 1023, 0, 30);
     
//    uint16_t analogValue = analogRead(PIN_POT);
//    uint8_t vol = map(analogValue, 0, 1023, 0, MAX_VOLUME);

    if (vol != lastVolumeRead) {
      lastVolumeRead = vol;
      volumeDFPlayer = vol;
      dfplayer.volume(volumeDFPlayer);
      //saveToEEPROM();
      Serial.print(F("volume:")); Serial.println(volumeDFPlayer);
    }
  }
}

uint16_t getDurasiTartil(uint8_t folder, uint8_t file) {
  if (folder == 0 || folder > MAX_FOLDER || file >= MAX_FILE) return 0;
  return durasiTartil[folder - 1][file];
}

uint16_t getDurasiAdzan(uint8_t file) {
  if (file == 0 || file >= MAX_FILE) return 0;
  return durasiAdzan[file];
}


void cekDanPutarSholatNonBlocking() {
  if (tartilSedangDiputar || adzanSedangDiputar || sudahEksekusi) return;

  uint32_t detikSekarang = hour() * 3600UL + minute() * 60UL + second();  // cukup pakai uint16_t

  static bool stateJadwal = false;

  // Cetak hanya sekali pada menit tertentu
  if ((minute() == 0 || minute() == 15 || minute() == 30 || minute() == 45) && second() == 0 && !stateJadwal) {
    stateJadwal = true;
    Serial.println(F("jadwal"));
  } else if (second() != 0) {
  stateJadwal = false;
  } 

  if(hour() == 1 && minute() == 0 && second() == 0){
    restartArduino();
  }


  for (byte w = 0; w < WAKTU_TOTAL; w++) { 
    
    WaktuConfig &cfg = jadwal[currentDay][w];
    if (!cfg.aktif) continue;
    if (jamSholat[w] == 0 && menitSholat[w] == 0) continue;  // Lewati jadwal tidak valid
    
    uint16_t totalDurasi = 0;
    
    // Hitung total durasi dari file tartil
    for (byte i = 0; i < 5; i++) {
      byte f = cfg.list[i];
      if (f) {
        uint16_t d = getDurasiTartil(cfg.folder, f);
        if (d) totalDurasi += d;
      }
    }

    uint32_t jadwalDetik = jamSholat[w] * 3600UL + menitSholat[w] * 60UL;
    uint32_t triggerDetik = cfg.tartilDulu ? (jadwalDetik - totalDurasi) : jadwalDetik;
    
    
    if (triggerDetik > 86400) continue;  // Lewati jika melebihi 1 hari
   
    if (detikSekarang == triggerDetik) {
      /*/============ DEBUG =============//
      Serial.println("TRIGGER MATCH!");
      Serial.println("jam: " + String(hour()) + " " + "menit: " + String(minute()) + "detik: " + String(second()));
      Serial.println("jamSholat[w]: " + String(jamSholat[w]));
      Serial.println("menitSholat[w]: " + String(menitSholat[w]));
      Serial.println("jadwalDetik: " + String(jadwalDetik));
      Serial.println("totalDurasi: " + String(totalDurasi));
      Serial.println("triggerDetik: " + String(triggerDetik));
      Serial.println("detikSekarang: " + String(detikSekarang));
      //================================/*/
      
      digitalWrite(RELAY_PIN, LOW);//relay NYALA
      currentCfg = &cfg;
      lastTriggerMillis = millis();
      sudahEksekusi = true;

      if (cfg.tartilDulu && totalDurasi > 0) {
        tartilIndex = 0;
        tartilFolder = cfg.folder;
        tartilCounter = 0;
        tartilSedangDiputar = true;
       // manualSedangDiputar = false;

        byte f = cfg.list[tartilIndex];
        targetDurasi = getDurasiTartil(tartilFolder, f);
        lastTick = millis();
        dfplayer.playFolder(tartilFolder, f);

#if DEBUG
        Serial.print("Tartil dimulai: ");
        Serial.println(f);
#endif

      } else if (cfg.aktifAdzan) {
        targetDurasiAdzan = getDurasiAdzan(cfg.fileAdzan);
        adzanCounter = 0;
        lastAdzanTick = millis();
        adzanSedangDiputar = true;
        dfplayer.playFolder(11, cfg.fileAdzan);

#if DEBUG
        Serial.print("Adzan langsung diputar: ");
        Serial.println(cfg.fileAdzan);
#endif
      }
    }
  }
}

void cekSelesaiTartil() {
  if (!tartilSedangDiputar) return;

  // Jeda antar file tartil
  if (jedaAktif) {
    if (millis() - jedaMulaiMillis >= JEDA_ANTAR_TARTIL) {
      jedaAktif = false;

      if (tartilIndex < 5) {
        byte f = currentCfg->list[tartilIndex];
        if (f) {
          targetDurasi = getDurasiTartil(tartilFolder, f);
          tartilCounter = 0;
          lastTick = millis();
          dfplayer.playFolder(tartilFolder, f);
#if DEBUG
          Serial.print("Memutar tartil selanjutnya: ");
          Serial.println(f);
#endif
        } else {
          tartilIndex = 5; // skip ke akhir
        }
      } else {
        tartilSedangDiputar = false;
      }
    }
    return;
  }

  // Counter tartil per detik
  if (millis() - lastTick >= 1000) {
    lastTick = millis();
    if (++tartilCounter >= targetDurasi) {
      tartilIndex++;
      if (tartilIndex < 5) {
        if (currentCfg->list[tartilIndex]) {
          jedaAktif = true;
          jedaMulaiMillis = millis();
#if DEBUG
          Serial.println("Menunggu jeda antar file tartil...");
#endif
        } else {
          tartilIndex = 5;
        }
      } else {
        // Tartil selesai
        tartilSedangDiputar = false;
        if (currentCfg->aktifAdzan) {
          adzanCounter = 0;
          targetDurasiAdzan = getDurasiAdzan(currentCfg->fileAdzan);
          lastAdzanTick = millis();
          adzanSedangDiputar = true;
          dfplayer.playFolder(11, currentCfg->fileAdzan);
#if DEBUG
          Serial.println("Tartil selesai, memutar adzan.");
#endif
        } else {
          matikanSemuaAudio();
          //digitalWrite(RELAY_PIN, LOW);
#if DEBUG
          Serial.println("Tartil selesai, relay dimatikan.");
#endif
        }
      }
    }
  }
}


void matikanSemuaAudio() {
  dfplayer.stop();
  digitalWrite(RELAY_PIN, HIGH);//relay mati
  tartilSedangDiputar = false;
  adzanSedangDiputar = false;
//  manualSedangDiputar = false;
}

void cekSelesaiAdzan() {
  if (!adzanSedangDiputar) return;

  if (millis() - lastAdzanTick >= 1000) {
    lastAdzanTick = millis();
    adzanCounter++;

    if (adzanCounter >= targetDurasiAdzan) {
      dfplayer.stop();
      digitalWrite(RELAY_PIN, HIGH);//relay mati
      adzanSedangDiputar = false;
     // Serial.println("Adzan selesai. Relay dimatikan.");
    }
  }
}
