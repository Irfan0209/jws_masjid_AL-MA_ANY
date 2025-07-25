const char * const pasar[] PROGMEM = {"WAGE", "KLIWON", "LEGI", "PAHING", "PON"}; 
const char * const Hari[] PROGMEM = {"MINGGU","SENIN","SELASA","RABU","KAMIS","JUM'AT","SABTU"};
const char * const bulanMasehi[] PROGMEM = {"JANUARI", "FEBRUARI", "MARET", "APRIL", "MEI", "JUNI", "JULI", "AGUSTUS", "SEPTEMBER", "OKTOBER", "NOVEMBER", "DESEMBER" };
const char* jadwal[] PROGMEM = {"SUBUH", "TERBIT", "DHUHA", "DZUHUR", "ASHAR", "MAGRIB", "ISYA'"};
const char* jadwalAzzan[] PROGMEM = {"SUBUH","DZUHUR", "ASHAR", "MAGRIB", "ISYA'"};
const char * const namaBulanHijriah[] PROGMEM = {
    "MUHARRAM", "SHAFAR", "RABIUL AWAL",
    "RABIUL AKHIR", "JUMADIL AWAL", 
    "JUMADIL AKHIR", "RAJAB",
    "SYA'BAN", "RAMADHAN", "SYAWAL",
    "DZULQA'DAH", "DZULHIJAH"
};


//================= tampilan animasi ==================//
/*
void drawDate() {
  if (adzan) return;

  static uint16_t x = 0;
  static uint16_t fullScroll = 0;
  static char buff_date[70];
  static char buff_time[9];
  static uint32_t lsRn = 0;
  static uint8_t lastSecond = 255;

  uint32_t Tmr = millis();
  RtcDateTime now = Rtc.GetDateTime();
  uint8_t Speed = speedDate;
  uint8_t daynow = now.DayOfWeek();
  fType(0);

  if (now.Second() != lastSecond) {
    lastSecond = now.Second();
    sprintf(buff_time, "%02d:%02d:%02d", now.Hour(), now.Minute(), now.Second());
  }

  if (fullScroll == 0) {
    snprintf(buff_date, sizeof(buff_date), "%s %s %02d %s %04d %02d %s %04dH",
      Hari[daynow],
      pasar[jumlahhari() % 5],
      now.Day(),
      bulanMasehi[now.Month() - 1],
      now.Year(),
      Hijir.getHijriyahDate,
      namaBulanHijriah[Hijir.getHijriyahMonth - 1],
      Hijir.getHijriyahYear
    );

    fullScroll = Disp.textWidth(buff_date) + 62 + 20;
  }

  if (Tmr - lsRn > Speed) {
    lsRn = Tmr;

    if (x < fullScroll) {
      ++x;
    } else {
      x = 0;
      fullScroll = 0;
      show = ANIM_NAME;
      Disp.drawLine(0, 0, 64, 0, 0);
      return;
    }

    if (x <= 6) {
      dwCtr(0, x - 6, buff_time);
    } else if (x >= (fullScroll - 6)) {
      dwCtr(0, (fullScroll - x) - 6, buff_time);
    } else {
      dwCtr(0, 0, buff_time);
    }

    drawTextWithMargin(62 - x, 9, buff_date);

  }
}

void drawTextWithMargin(int x, int y, const char* teks) {
  int textW = Disp.textWidth(teks);

  // Hanya gambar jika ada bagian teks yang masuk area 2..61
  if (x + textW <= 2 || x >= 62) return;  // Sepenuhnya di luar area tampil

  // Gambar teks
  Disp.drawText(x, y, teks);
}
*/

void drawDate() {
  if(adzan) return;
  static uint16_t x = 0;
  static uint16_t fullScroll = 0;
  static char buff_date[70];  // Cukup untuk semua teks tanggal
  static char buff_time[9];   // HH:MM:SS
  static uint32_t lsRn = 0;
  static uint8_t lastSecond = 255; // untuk deteksi perubahan waktu

  uint32_t Tmr = millis();
  RtcDateTime now = Rtc.GetDateTime();
  uint8_t Speed = speedDate;
  uint8_t daynow = now.DayOfWeek();
  fType(0);
  // Hanya update teks waktu jika detik berubah
  if (now.Second() != lastSecond) {
    lastSecond = now.Second();
    buff_time[0] = '0' + now.Hour() / 10;
    buff_time[1] = '0' + now.Hour() % 10;
    buff_time[2] = ':';
    buff_time[3] = '0' + now.Minute() / 10;
    buff_time[4] = '0' + now.Minute() % 10;
    buff_time[5] = ':';
    buff_time[6] = '0' + now.Second() / 10;
    buff_time[7] = '0' + now.Second() % 10;
    buff_time[8] = '\0';
  }
 
  // Jika belum hitung teks scrolling
  if (fullScroll == 0) {
    snprintf(buff_date, sizeof(buff_date), "%s %s %02d %s %04d %02d %s %04dH",
      Hari[daynow],
      pasar[jumlahhari() % 5],
      now.Day(),
      bulanMasehi[now.Month() - 1],
      now.Year(),
      Hijir.getHijriyahDate,
      namaBulanHijriah[Hijir.getHijriyahMonth - 1],
      Hijir.getHijriyahYear
    );

    fullScroll = Disp.textWidth(buff_date) + Disp.width() + 20;
  }

  // Waktu scroll
  if (Tmr - lsRn > Speed) {
    lsRn = Tmr;

    if (x < fullScroll) {
      ++x;
    } else {
      x = 0;
      fullScroll = 0; // Reset agar hitung ulang saat tanggal berubah
      show = ANIM_NAME;
      Disp.drawLine(0, 0, 64, 0, 0);
      return;
    }

    // Gambar waktu
    if (x <= 6) {
      Disp.drawText(6, x - 6, buff_time);
    } else if (x >= (fullScroll - 6)) {
      Disp.drawText(6, (fullScroll - x) - 6, buff_time);
    } else {
      Disp.drawText(6, 0, buff_time);
    }
   
    // Gambar tanggal berjalan
    Disp.drawText(Disp.width() - x, 9, buff_date);
  }
}


void drawName(){
  if(adzan) return;
  
  static uint16_t x;
  static uint16_t fullScroll = 0;
  RtcDateTime now = Rtc.GetDateTime();
  static uint32_t   lsRn;
  uint32_t          Tmr = millis();
   
  uint8_t Speed = speedName;
  uint8_t daynow   = now.DayOfWeek();    // load day Number

  char  Buff[20];
    
  sprintf(Buff,"%02d:%02d:%02d",now.Hour(),now.Minute(),now.Second());

  //char buff_date[100]="MASJID AL MA`ANY TANJUNGSARI"; // Pastikan ukuran buffer cukup besar

  fType(0);
  if (fullScroll == 0) { // Hitung hanya sekali
    fullScroll = Disp.textWidth(name) + Disp.width();
  }

 if (Tmr - lsRn > Speed) { 
  lsRn = Tmr;
  if (x < fullScroll) {++x; }
  else {x = 0; counterName=1;show=ANIM_TEXT1; return;}
 
    //Marquee    jam yang tampil di bawah
  Disp.drawText(Disp.width() - x, 0, name); //runing teks diatas
  //fType(0);
  if (x<=6)                     { Disp.drawText(6,16-x,Buff);}
  else if (x>=(fullScroll-6))   { Disp.drawText(6,16-(fullScroll-x),Buff); Disp.drawLine(0,15-(fullScroll-x),64,15-(fullScroll-x),0);}
  else                          { Disp.drawText(6,9,Buff);}//posisi jamnya yang bawah
        
 }
}

void drawText1(){
  if(adzan) return;
  
  static uint16_t x;
  static uint16_t fullScroll = 0;
  RtcDateTime now = Rtc.GetDateTime();
  static uint32_t   lsRn;
  uint32_t          Tmr = millis();
   
  uint8_t Speed = speedText1;
  uint8_t daynow   = now.DayOfWeek();    // load day Number

  char  Buff[20];
    
  sprintf(Buff,"%02d:%02d:%02d",now.Hour(),now.Minute(),now.Second());

    
//  char buff_date[100]; // Pastikan ukuran buffer cukup besar
//  snprintf(buff_date,sizeof(buff_date), "%s","Luruskan dan Rapatkan Shaf Sholat!");
  
  fType(0);
  if (fullScroll == 0) { // Hitung hanya sekali
    fullScroll = Disp.textWidth(text1) + Disp.width();
  }

 if (Tmr - lsRn > Speed) { 
  lsRn = Tmr;
  if (x < fullScroll) {++x; }
  else {x = 0; Disp.drawLine(0,0,64,0,0); show=ANIM_TEXT2; return;}
        
  
  if (x<=6)                     { Disp.drawText(6,x-6,Buff); }
  else if (x>=(fullScroll-6))   { Disp.drawText(6,(fullScroll-x)-6,Buff); }
  else                          { Disp.drawText(6,0,Buff); }  //posisi jam nya yang diatas
   
   //fType(0); //Marquee  running teks dibawah
   Disp.drawText(Disp.width() - x, 9 , text1);//runinng teks dibawah
 }
}

void drawText2(){
  if(adzan) return;
  
  static uint16_t x;
  static uint16_t fullScroll = 0;
  RtcDateTime now = Rtc.GetDateTime();
  static uint32_t   lsRn;
  uint32_t          Tmr = millis();
   
  uint8_t Speed = speedText2;
  uint8_t daynow   = now.DayOfWeek();    // load day Number

  char  Buff[20];
    
  sprintf(Buff,"%02d:%02d:%02d",now.Hour(),now.Minute(),now.Second());

    
//  char buff_date[100]; // Pastikan ukuran buffer cukup besar
//  snprintf(buff_date,sizeof(buff_date), "%s","Harap Matikan HP Anda!");
  
  fType(0);
  if (fullScroll == 0) { // Hitung hanya sekali
    fullScroll = Disp.textWidth(text2) + Disp.width();
  }

 if (Tmr - lsRn > Speed) { 
  lsRn = Tmr;
  if (x < fullScroll) {++x; }
  else {x = 0; show=ANIM_SHOLAT; return;}
 
    //Marquee    jam yang tampil di bawah
  Disp.drawText(Disp.width() - x, 0, text2); //runing teks diatas
  //fType(0);
  if (x<=6)                     { Disp.drawText(6,16-x,Buff);}
  else if (x>=(fullScroll-6))   { Disp.drawText(6,16-(fullScroll-x),Buff); Disp.drawLine(0,15-(fullScroll-x),64,15-(fullScroll-x),0);}
  else                          { Disp.drawText(6,9,Buff);}//posisi jamnya yang bawah
        
 }
}

void scrollText(){
  if(adzan) return;
  
   static uint16_t x;
  static uint16_t fullScroll = 0;
  static uint32_t   lsRn;
  uint32_t          Tmr = millis();
  uint8_t Speed = speedName;
  //char buff_date[]="MASJID AL MA ANY TANJUNGSARI";

  fType(4);
  if (fullScroll == 0) { // Hitung hanya sekali
    fullScroll = Disp.textWidth(name) + Disp.width();
  }

 if (Tmr - lsRn > Speed) { 
  lsRn = Tmr;
  if (x < fullScroll) {++x; }
  else {x = 0; counterName=0;show=ANIM_CLOCK_BIG; return;}
 
    //Marquee    jam yang tampil di bawah
  Disp.drawText(Disp.width() - x, 0, name); //runing teks diatas
}
}

void drawJadwalSholat() {
  if(adzan) return;
  
  RtcDateTime now = Rtc.GetDateTime();
  static int y = 0, y1 = 0;
  static uint8_t s = 0, s1 = 0;
  static bool run = false;

  static uint32_t lsRn_y1 = 0;
  static uint32_t lsRn_y = 0;
  static uint32_t tHold = 0;

  uint32_t Tmr = millis();

  // Pilih waktu sholat sesuai list
  float stime;
  switch (list) {
    case 0: stime = JWS.floatSubuh; break;
    case 1: stime = JWS.floatTerbit; break;
    case 2: stime = JWS.floatDhuha; break;
    case 3: stime = JWS.floatDzuhur; break;
    case 4: stime = JWS.floatAshar; break;
    case 5: stime = JWS.floatMaghrib; break;
    case 6: stime = JWS.floatIsya; break;
    default: stime = 0; break;
  }

  // Transisi vertikal y1 (jam muncul/hilang)
  if ((Tmr - lsRn_y1) > 55) {
    lsRn_y1 = Tmr;

    if (s1 == 0 && y1 < 17) {
      Disp.drawLine(26, y1, 26, y1, 1);//garis pemisah vertikal
      Disp.drawLine(0, y1-12, 25, y1-12, 0);//garis untuk menghapus angka jam
      y1++;
    } else if (s1 == 1 && y1 > 0) {
//      Disp.drawLine(14, 25 - y1, 25, 25 - y1, 0); // menghapus detik
//      Disp.drawLine(0, y1 - 2, 12, y1 - 2, 0);    // menghapus jam
//      Disp.drawLine(14, y1 - 9, 24, y1 - 9, 0);   // menghapus menit
      Disp.drawLine(26, y1, 26, y1, 0);//garis pemisah vertikal
      Disp.drawLine(0, y1-5, 25, y1-5, 0);//garis untuk menghapus angka jam
      y1--;
    }
  }

  // Saat y1 selesai muncul, mulai animasi jadwal
  if (y1 == 17 && s1 == 0) {
    run = true; 
    if(now.Second() % 2 ){
      Disp.drawRect(13, 6, 13, 7, 1); //posisi y = 6
      Disp.drawRect(13, 9, 13, 10, 1); //posisi y = 9
    }else{
      Disp.drawRect(13, 6, 13, 7, 0); //posisi y = 5
      Disp.drawRect(13, 9, 13, 10, 0); //posisi y = 8
    }
  }

  // Animasi gerakan teks (y)
  if (run && (Tmr - lsRn_y) > 55) {
    lsRn_y = Tmr;

    if (s == 0 && y < 9) {
      y++;
    } else if (s == 1 && y > 0) {
      y--;
      Disp.drawLine(27, 17 - y, 64, 17 - y, 0);
    }
  }

  // Delay sebelum animasi keluar (reverse)
  if (y == 9 && s == 0 && tHold == 0) {
    tHold = millis();
  }
  if (tHold > 0 && (millis() - tHold > 4000)) {
    s = 1;     // mulai keluar
    tHold = 0; // reset timer
  }

  // Setelah animasi selesai
  if (y == 0 && s == 1) {
    s = 0;
    Disp.drawLine(27, 0, 64, 0, 0);
    list = (list + 1) % 7;
    if (list == 0) {
      run = false;
      s1 = 1; // trigger keluar vertikal
      Disp.drawRect(13, 6, 13, 7, 0); //posisi y = 5
      Disp.drawRect(13, 9, 13, 10, 0); //posisi y = 8
    }
  }

//  // Tampilkan jam digital model -|
//  fType(5);
//  Disp.drawChar(0, y1 - 17, '0' + now.Hour() / 10);
//  Disp.drawChar(6, y1 - 17, '0' + now.Hour() % 10);
//
//  fType(0);
//  Disp.drawChar(14, y1 - 17, '0' + now.Minute() / 10);
//  Disp.drawChar(20, y1 - 17, '0' + now.Minute() % 10);
//  Disp.drawChar(14, 26 - y1, '0' + now.Second() / 10);
//  Disp.drawChar(20, 26 - y1, '0' + now.Second() % 10);

// Tampilkan jam digital
  fType(0);
  Disp.drawChar(1, y1 - 12, '0' + now.Hour() / 10);
  Disp.drawChar(7, y1 - 12, '0' + now.Hour() % 10);

//  Disp.drawRect(12, y1 - 12, 13, y1 - 11, 1); //posisi y = 5
//  Disp.drawRect(12, y1 - 9, 13, y1 - 8, 1); //posisi y = 8
  
  Disp.drawChar(15, y1 - 12, '0' + now.Minute() / 10);
  Disp.drawChar(21, y1 - 12, '0' + now.Minute() % 10);

  // Tampilkan teks jadwal sholat
  uint8_t shour = (uint8_t)stime;
  uint8_t sminute = (uint8_t)((stime - shour) * 60);

  char buf[6];
  buf[0] = '0' + shour / 10;
  buf[1] = '0' + shour % 10;
  buf[2] = ':';
  buf[3] = '0' + sminute / 10;
  buf[4] = '0' + sminute % 10;
  buf[5] = '\0';

  fType(0);
  dwCtr(26, y - 9, jadwal[list]);
  dwCtr(28, 18 - y, buf);

  if (y1 == 0 && s1 == 1) {
    s1 = 0;
    show = ANIM_CLOCK_BIG; // ganti mode jika perlu
  }
}

void anim_JG()
  {
    if(adzan) return;
    // check RunSelector
    //if(!dwDo(DrawAdd)) return; 
    RtcDateTime now = Rtc.GetDateTime();
    char  BuffJ[6];
    char  BuffM[6];
    char  BuffD[6];
    uint8_t daynow = now.DayOfWeek();
    
    static byte    y;
    static bool    s; // 0=in, 1=out              
    static uint32_t   lsRn;
    uint32_t          Tmr = millis();

    if((Tmr-lsRn)>75) 
      { 
        if(s==0 and y<17){lsRn=Tmr; y++;}
        if(s==1 and y>0){lsRn=Tmr; y--; Disp.clear();}
      }
    

    sprintf(BuffJ,"%02d",now.Hour());
    sprintf(BuffM,"%02d",now.Minute());
    sprintf(BuffD,"%02d",now.Second());

    fType(5);
    Disp.drawText(2,17-y,BuffJ);  //tampilkan jam 1
    Disp.drawText(25,y-17,BuffM);  //tampilkan menit
    Disp.drawText(66-y,0,BuffD);  //tampilkan detik //x=50  67
    
    if (y==17)
      {
        Disp.drawRect(20,0+3,18,0+5,1);
        Disp.drawRect(20,0+10,18,0+12,1);

         Disp.drawRect(45,0+3,43,0+5,1);
         Disp.drawRect(45,0+10,43,0+12,1);
      }

    if((Tmr-lsRn)>5000 and y ==17) {s=1;}
    if (y == 0 and s==1) {Serial.println("TIME:" + String(BuffJ) + "," + String(BuffM) + "," + String(BuffD) + "," + String(daynow)); s=0; show=ANIM_DATE;}//dwDone(DrawAdd);
    
    }

//======================= end ==========================//

/*======================= animasi memasuki waktu sholat ====================================*/
void drawAzzan()
{
    //static const char *jadwal[] = {"SUBUH", "DZUHUR", "ASHAR", "MAGRIB","ISYA'"};
    const char *sholat = jadwalAzzan[sholatNow]; 
    static uint8_t ct = 0;
    static uint32_t lsRn = 0;
    uint32_t Tmr = millis();
    const uint8_t limit = config.durasiadzan;

    if (Tmr - lsRn > 500 && ct <= limit)
    {
        lsRn = Tmr;
        if (!(ct & 1))  // Lebih cepat dibandingkan ct % 2 == 0
        {
          fType(0);
            dwCtr(1, 0, "ADZAN");
            fType(3);
            dwCtr(1, 9, sholat);
            Buzzer(1);
        }
        else
        {
            Buzzer(0);
            Disp.clear();
        }
        ct++;
    }
    
    if ((Tmr - lsRn) > 1500 && (ct > limit))
    {
        show = ANIM_IQOMAH;
        Disp.clear();
        ct = 0;
        Buzzer(0);
    }
}

void drawIqomah()  // Countdown Iqomah (9 menit)
{  
    static uint32_t lsRn = 0;
    static int ct = 0;  // Mulai dari 0 untuk menghindari error
    static int mnt, scd;
    char locBuff[10];  
    uint32_t now = millis();  // Simpan millis() di awal
    
    int cn_l = (iqomah[sholatNow] * 60);
    
    mnt = (cn_l - ct) / 60;
    scd = (cn_l - ct) % 60;
    sprintf(locBuff, "-%02d:%02d", mnt, scd);

   // if ((ct & 1) == 0) {  // Gunakan bitwise untuk optimasi modulo 2
        fType(0);
        dwCtr(0, 0, "IQOMAH");
    //}

    fType(0);
    dwCtr(0, 8, locBuff);

    if (now - lsRn > 1000) 
    {   
        lsRn = now;
        ct++;

        //Serial.println(F("test run"));  // Gunakan F() untuk hemat RAM

        if (ct > (cn_l - 5)) {
            Buzzer(ct & 1);  // Gunakan bitwise untuk menggantikan modulo 2
        }
    }

    if (ct >= cn_l)  // Pakai >= untuk memastikan countdown selesai dengan benar
    {
        ct = 0;
        Buzzer(0);
        Disp.clear();
        show = ANIM_BLINK;
        Disp.setBrightness(20);
        
    }    
}

void blinkBlock()
{
    static uint32_t lsRn;
    static int ct = 0;
    const int ct_l = displayBlink[sholatNow] * 60;  // Durasi countdown
    uint32_t now = millis();  // Simpan millis()

    // Ambil waktu dari RTC
    RtcDateTime rtcNow = Rtc.GetDateTime();

    // Hitung countdown
    int mnt = (ct_l - ct) / 60;
    int scd = (ct_l - ct) % 60;

    // Tampilkan jam besar
    char timeBuff[6];
    sprintf(timeBuff, "%02d:%02d", rtcNow.Hour(), rtcNow.Minute());
    
    fType(3);
    dwCtr(0, 9, timeBuff);

    // Update countdown setiap detik
    if (now - lsRn > 1000)
    {
        lsRn = now;
//        Serial.print(F("ct:"));
//        Serial.println(ct);
        ct++;
    }

    // Reset jika countdown selesai & kembali ke animasi utama
    if (ct > ct_l)
    {
        sholatNow = -1;
        adzan = false;
        ct = 0;
        Disp.clear();
        show = ANIM_CLOCK_BIG;
        Disp.setBrightness(brightness);
    }
}
//===================================== end =================================//

//=========================== setingan untuk tampilan text=================//
void fType(int x)
  {
    if(x==0) Disp.setFont(Font0);
    else if(x==1) Disp.setFont(Font1); 
    else if(x==2) Disp.setFont(Font2);
    else if(x==3) Disp.setFont(Font3);
    else if(x==4) Disp.setFont(Font4);
    else if(x==5) Disp.setFont(Font5);
  
  }

  void dwCtr(int x, int y, String Msg){
   uint16_t   tw = Disp.textWidth(Msg);
   uint16_t   c = int((DWidth-x-tw)/2);
   Disp.drawText(x+c,y,Msg);
}
//====================== end ==========================//
