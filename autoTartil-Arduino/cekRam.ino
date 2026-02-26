extern unsigned int __heap_start;
extern void *__brkval;

int freeRam() {
  int free_memory;

  if ((int)__brkval == 0) {
    free_memory = ((int)&free_memory) - ((int)&__heap_start);
  } else {
    free_memory = ((int)&free_memory) - ((int)__brkval);
  }

  return free_memory;
}


void restartArduino() {
  wdt_enable(WDTO_15MS);  // timeout tercepat
  while (1) {
    // tunggu watchdog reset
  }
}

void checkRamWarning() {
  static unsigned long lastCheck = 0;

  if (millis() - lastCheck < RAM_CHECK_INTERVAL) return;
  lastCheck = millis();

  int ram = freeRam();

  if (ram <= RAM_WARNING_THRESHOLD) {
    if (!ramWarningActive) {
      ramWarningActive = true;
      Serial.print(F("⚠ WARNING: RAM LOW = "));
      Serial.print(ram);
      Serial.println(F(" bytes"));
    }
  } else {
    // RAM sudah aman lagi → reset warning
    ramWarningActive = false;
  }
  digitalWrite(RAM_WARNING,ramWarningActive);
}
