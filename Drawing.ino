//const char msg[] PROGMEM = "MUSHOLLAH HIDAYATULLAH RT19/RW03,DODOKAN,TANJUNGSARI";
const char * const pasar[] PROGMEM = {"WAGE", "KLIWON", "LEGI", "PAHING", "PON"}; 
const char * const Hari[] PROGMEM = {"MINGGU","SENIN","SELASA","RABU","KAMIS","JUM'AT","SABTU"};
const char * const bulanMasehi[] PROGMEM = {"JANUARI", "FEBRUARI", "MARET", "APRIL", "MEI", "JUNI", "JULI", "AGUSTUS", "SEPTEMBER", "OKTOBER", "NOVEMBER", "DESEMBER" };
//const char msg1[] PROGMEM ="SAIPUL,SANIYAH,MUSY'IROH,ACHMAD";
const char * const namaBulanHijriah[] PROGMEM = {
    "MUHARRAM", "SHAFAR", "RABIUL AWAL",
    "RABIUL AKHIR", "JUMADIL AWAL", 
    "JUMADIL AKHIR", "RAJAB",
    "SYA'BAN", "RAMADHAN", "SYAWAL",
    "DZULQA'DAH", "DZULHIJAH"
};
//const char jadwal[][8] PROGMEM = {
//    "SUBUH ", "TERBIT ", "DZUHUR ", "ASHAR ", 
//    "TRBNM ", "MAGRIB ", "ISYA' "
//  };


//================= tampilan animasi ==================//

void runAnimasiJam(){
  
  RtcDateTime now = Rtc.GetDateTime();
  static int    y=0;
  static bool    s; // 0=in, 1=out              
  static uint32_t   lsRn;
  uint32_t          Tmr = millis();
  uint8_t dot    = now.Second();
  char buff_jam[20];
  
  if(dot & 1){sprintf(buff_jam,"%02d:%02d",now.Hour(),now.Minute());}
  else{sprintf(buff_jam,"%02d %02d",now.Hour(),now.Minute());}
  
  if((Tmr-lsRn)>75) 
      { 
        if(s==0 and y<9 ){lsRn=Tmr;y++; }
        if(s==1 and y>0){lsRn=Tmr;y--; if(y == 1){ Disp.drawText(0,0, "          "); }}
      }
  
   if(y ==9 and flagAnim == true) {s=1;}

   if (y == 0 and s==1) {y=0; s=0; flagAnim = false; show = ANIM_SHOLAT;}
  
  fType(5); 
  dwCtr(0,y-9, buff_jam); 

}

void drawDate(){
  static uint16_t x;
  static uint16_t fullScroll = 0;
  RtcDateTime now = Rtc.GetDateTime();
  static uint32_t   lsRn;
  uint32_t          Tmr = millis();
   
  uint8_t Speed = 50;//speedDate;
  uint8_t daynow   = now.DayOfWeek();    // load day Number

  char  Buff[20];
    
  sprintf(Buff,"%02d:%02d:%02d",now.Hour(),now.Minute(),now.Second());

    
  char buff_date[100]; // Pastikan ukuran buffer cukup besar
    snprintf(buff_date,sizeof(buff_date), "%s %s %02d %s %04d %02d %s %04dH",
    Hari[daynow], pasar[jumlahhari() % 5], now.Day(), bulanMasehi[now.Month()-1], now.Year(),
    Hijir.getHijriyahDate, namaBulanHijriah[Hijir.getHijriyahMonth - 1], Hijir.getHijriyahYear);

  fType(0);
  if (fullScroll == 0) { // Hitung hanya sekali
    fullScroll = Disp.textWidth(buff_date) + Disp.width();
  }

 if (Tmr - lsRn > Speed) { 
  lsRn = Tmr;
  if (x < fullScroll) {++x; }
  else {x = 0; return;}
        
  
  if (x<=6)                     { dwCtr(0,x-6,Buff); }
  else if (x>=(fullScroll-6))   { dwCtr(0,(fullScroll-x)-6,Buff); }
  else                          { dwCtr(0,0,Buff); }  //posisi jam nya yang diatas
   
   //fType(0); //Marquee  running teks dibawah
   Disp.drawText(Disp.width() - x, 9 , buff_date);//runinng teks dibawah
 }
}

void drawName(){
  static uint16_t x;
  static uint16_t fullScroll = 0;
  RtcDateTime now = Rtc.GetDateTime();
  static uint32_t   lsRn;
  uint32_t          Tmr = millis();
   
  uint8_t Speed = 50;//speedDate;
  uint8_t daynow   = now.DayOfWeek();    // load day Number

  char  Buff[20];
    
  sprintf(Buff,"%02d:%02d:%02d",now.Hour(),now.Minute(),now.Second());
//String nama = ;
    
  char buff_date[100]; // Pastikan ukuran buffer cukup besar
//    snprintf(buff_date,sizeof(buff_date), "%s %s %02d %s %04d %02d %s %04dH",
//    Hari[daynow], pasar[jumlahhari() % 5], now.Day(), bulanMasehi[now.Month()-1], now.Year(),
//    Hijir.getHijriyahDate, namaBulanHijriah[Hijir.getHijriyahMonth - 1], Hijir.getHijriyahYear);
snprintf(buff_date,sizeof(buff_date), "%s","MASJID AL MA`ANY ,NGAMPEL RT14 RW02,TANJUNGSARI.SIDOARJO,JAWA TIMUR");
  fType(0);
  if (fullScroll == 0) { // Hitung hanya sekali
    fullScroll = Disp.textWidth(buff_date) + Disp.width();
  }

 if (Tmr - lsRn > Speed) { 
  lsRn = Tmr;
  if (x < fullScroll) {++x; }
  else {x = 0; return;}
 
    //Marquee    jam yang tampil di bawah
  Disp.drawText(Disp.width() - x, 0, buff_date); //runing teks diatas
  //fType(0);
  if (x<=6)                     { dwCtr(0,16-x,Buff);}
  else if (x>=(fullScroll-6))   { dwCtr(0,16-(fullScroll-x),Buff); }
  else                          { dwCtr(0,9,Buff);}//posisi jamnya yang bawah
        
 }
}

void runningTextInfo() {
  static uint16_t x = 0;
  static uint32_t lsRn;
  uint32_t Tmr = millis();
  uint8_t Speed = speedText1;
  
//  char msg_buffer[50]; // Pastikan cukup besar untuk teks
//  strcpy_P(msg_buffer, msg1); // Ambil teks dari Flash
  //String msg_buffer = text;
  // Hitung panjang teks hanya sekali
  static uint16_t fullScroll = 0;
  if (fullScroll == 0) { 
    fullScroll = Disp.textWidth(text1) + Disp.width() + 250;
  }

  // Jalankan animasi scrolling berdasarkan millis()
  if (Tmr - lsRn > Speed && flagAnim == false) { 
    lsRn = Tmr;
    fType(0);
    
    int posX = Disp.width() - x;
    if (posX < -Disp.textWidth(text1)) { // Cegah teks keluar layar
      x = 0;
      flagAnim = true;
      fullScroll=0;
      Disp.clear();
      return;
    }

    Disp.drawText(posX, 9, text1);
    x++; // Geser teks ke kiri
  }
}

void anim_JG()
  {
    // check RunSelector
    //if(!dwDo(DrawAdd)) return; 

    char  BuffJ[6];
    char  BuffM[6];
    char  BuffD[6];
    
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
    Disp.drawText(1,17-y,BuffJ);  //tampilkan jam
    Disp.drawText(25,y-17,BuffM);  //tampilkan menit
    Disp.drawText(67-y,0,BuffD);  //tampilkan detik //x=50
    
    if (y==17)
      {
        Disp.drawRect(20,0+3,18,0+5,1);
        Disp.drawRect(20,0+10,18,0+12,1);

         Disp.drawRect(45,0+3,43,0+5,1);
         Disp.drawRect(45,0+10,43,0+12,1);
      }

    if((Tmr-lsRn)>3000 and y ==17) {s=1;}
    if (y == 0 and s==1) { s=0;}//dwDone(DrawAdd);
    
    }







//======================= end ==========================//

//==================== tampilkan jadwal sholat ====================//
void animasiJadwalSholat(){
 
  RtcDateTime now = Rtc.GetDateTime();
  static int        y=0;
  static int        x=0;
  static uint8_t    s=0; // 0=in, 1=out   
  static uint8_t    s1=0;
  
  float sholatT[]={JWS.floatSubuh,JWS.floatTerbit,JWS.floatDhuha,JWS.floatDzuhur,JWS.floatAshar,JWS.floatMaghrib,JWS.floatIsya};
  if(list != lastList){s=0; s1=0; x=0; y=0;lastList = list; }

  static uint32_t   lsRn;
  uint32_t          Tmr = millis(); 
  
  const char *jadwal[] = {"SUBUH","TERBIT","DHUHA", "DZUHUR", "ASHAR", "MAGRIB","ISYA'"};
  char buff_jam[10];

  if((Tmr-lsRn)>55) 
  { 
    if(s1==0 and y<9){lsRn=Tmr; y++; }
    if(s==1 and x<33){lsRn=Tmr; x++; }
  }

  if((Tmr-lsRn)>4000 and y == 9) {s1=1; s=1;}

  if (x == 33 and s==1 and s1 == 1) { 
    s=0;
    s1=0;
    x=0;
    y=0;
    list++; 
    //Serial.println(config.latitude,6);
    if(list==7){list=0; Disp.clear(); show=ANIM_CLOCK_BIG; }
  }

  float stime = sholatT[list];
  uint8_t shour = floor(stime);
  uint8_t sminute = floor((stime - (float)shour) * 60);
  uint8_t ssecond = floor((stime - (float)shour - (float)sminute / 60) * 3600);

  sprintf(buff_jam, "%02d:%02d", shour, sminute);

  if(s1==0){
    fType(3);
    dwCtr(0,y-9, jadwal[list]);
    fType(0);
    dwCtr(0,18-y, buff_jam);
  }
  else{
    Disp.drawLine((list<6)?x-1:x,-1,(list<6)?x-1:x,16,1);
    Disp.drawLine((list<6)?x-2:x-1,-1,(list<6)?x-2:x-1,16,0);
  }
}

//=========================================================================//
 
/*======================= animasi memasuki waktu sholat ====================================*/
void drawAzzan()
{
    static const char *jadwal[] = {"SUBUH", "DZUHUR", "ASHAR", "MAGRIB","ISYA'"};
    const char *sholat = jadwal[sholatNow]; 
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
        show = ANIM_CLOCK_BIG;
        Disp.clear();
        ct = 0;
        Buzzer(0);
    }
}
/*
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

    if ((ct & 1) == 0) {  // Gunakan bitwise untuk optimasi modulo 2
        fType(0);
        dwCtr(0, 8, "IQOMAH");
    }

    fType(0);
    dwCtr(0, 16, locBuff);

    if (now - lsRn > 1000) 
    {   
        lsRn = now;
        ct++;

        Serial.println(F("test run"));  // Gunakan F() untuk hemat RAM

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

//    char locBuff[6];
//    sprintf(locBuff, " %d:%02d ", mnt, scd);

//    fType(2);
//    Disp.drawText(10, 8, locBuff);

    // Tampilkan jam besar
    char timeBuff[9];
    sprintf(timeBuff, "%02d:%02d:%02d", rtcNow.Hour(), rtcNow.Minute(),rtcNow.Second());
    
    fType(3);
    dwCtr(0, 16, timeBuff);

    // Update countdown setiap detik
    if (now - lsRn > 1000)
    {
        lsRn = now;
        Serial.print(F("ct:"));
        Serial.println(ct);
        ct++;
    }

    // Reset jika countdown selesai & kembali ke animasi utama
    if (ct > ct_l)
    {
        sholatNow = -1;
        adzan = false;
        ct = 0;
        Disp.clear();
        show = ANIM_JAM;
    }
}*/
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
