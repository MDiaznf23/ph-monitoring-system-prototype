// ========== SYSTEM STATUS FUNCTIONS ==========

bool isAnyPumpInError() {
  for (int i = 0; i < 3; i++) { // Pastikan loop sampai 3
    if (pumps[i].status == PUMP_ERROR) {
      return true;
    }
  }
  return false;
}

String getSystemStatus() {
  // Check emergency conditions first
  float currentPH = getCurrentAvgPH();
  
  if(currentPH <= EMERGENCY_SHUTDOWN_PH || currentPH >= EMERGENCY_SHUTDOWN_PH_HIGH) {
    return "EMERGENCY";
  }
  
  // Check if any pump is active
  bool anyPumpActive = false;
  for(int i = 0; i < 3; i++) {
    if(pumps[i].isActive) {
      anyPumpActive = true;
      break;
    }
  }
  
  if(anyPumpActive) {
    return "ADJUSTING";
  }
  
  // Check if buffer is ready
  if(!bufferReady) {
    return "SAMPLING";
  }
  
  // Check pH status
  if(abs(currentPH - targetPH) <= 0.1) {
    return "OPTIMAL";
  } else if(abs(currentPH - targetPH) <= 0.6) {
    return "NORMAL";
  } else {
    return "ADJUST REQ";
  }
}

// Fungsi baru untuk kalibrasi cepat 1-titik
void performQuickCalibration() {
  // 1. Baca voltase saat ini dari sensor
  int rawPH = analogRead(PH_PIN);

  // 2. Hitung offset baru agar hasilnya menjadi 7.0
  // Rumus: 7.0 = (-0.0139 * voltage) + newOffset
  // Maka: newOffset = 7.0 - (-0.0139 * voltage)
  calibrationOffset = 7.0 + (0.0139 * rawPH);

  // 3. Simpan offset baru ke EEPROM
  EEPROM.writeFloat(ADDR_CALIBRATION_OFFSET, calibrationOffset);
  EEPROM.commit();

  LOG_PRINTLN(LOG_LEVEL_INFO, "QUICK CALIBRATION: New offset saved: " + String(calibrationOffset, 4));
}

// ========== SENSOR FUNCTIONS ==========

int getRawSensorValue() {
  int value = analogRead(PH_PIN);
  return value;
}

// ========== HELPERS ==========

void executeMainAction() {
  switch(currentMode) {
    case MODE_CALIBRATION:
      // Panggil fungsi kalibrasi cepat yang sudah kita buat
      performQuickCalibration();

      // Opsional: Tampilkan pesan sukses selama 2 detik
      lcd.clear();
      lcd.setCursor(0, 1);
      lcd.print("   KALIBRASI SUKSES   ");
      delay(2000);

      // Setelah selesai, otomatis kembali ke mode AUTO dan layar utama
      currentMode = MODE_AUTO;
      displayState = DISPLAY_MAIN;
      needsRedraw = true;
      break;

    case MODE_SETTINGS:
      // Should not reach here in settings mode
      break;
  }
}

// ========== DISPLAY HELPERS ==========

// Helper function untuk update dan simpan runtime pompa ke EEPROM
void updateAndSaveRunTime(PumpControl* pump) {
  if (pump == nullptr || pump->startTime == 0) return;

  unsigned long duration = millis() - pump->startTime;
  unsigned long totalRunTime = 0;
  uint16_t address;

  if (pump->type == PUMP_LIME) {
    address = ADDR_LIME_RUNTIME; 
  } else if (pump->type == PUMP_ACID) { // <-- Dibuat lebih eksplisit
    address = ADDR_ACID_RUNTIME;
  } else if (pump->type == PUMP_WATER) { // <-- BLOK BARU UNTUK POMPA AIR
    address = ADDR_WATER_RUNTIME;
  } else {
    // Jika tipe pompa tidak dikenali, jangan lakukan apa-apa
    return;
  }

  // Baca total runtime sebelumnya dari EEPROM
  EEPROM.get(address, totalRunTime);

  // Tambahkan durasi sekarang dan simpan kembali
  totalRunTime += duration;
  EEPROM.put(address, totalRunTime);
  EEPROM.commit();

  LOG_PRINTLN(LOG_LEVEL_DEBUG, "MAINTENANCE: Runtime for pump " + getPumpName(pump->type) + " updated to " + String(totalRunTime / 1000) + "s.");
}

void resetPumpRuntimes() {
  EEPROM.put(ADDR_LIME_RUNTIME, (unsigned long)0);
  EEPROM.put(ADDR_ACID_RUNTIME, (unsigned long)0);
  EEPROM.put(ADDR_WATER_RUNTIME, (unsigned long)0);
  EEPROM.commit();
  LOG_PRINTLN(LOG_LEVEL_INFO, "MAINTENANCE: All pump runtimes have been reset.");
}

// ========== SAFETY FUNCTIONS ==========

bool isSystemHealthy() {
  // Check pH sensor
  if(!bufferReady) return false;
  
  float currentPH = getCurrentAvgPH();
  if(currentPH <= 0 || currentPH >= 14) return false;
  
  // Check temperature sensor
  if(currentHumidity < 0 || currentHumidity > 100) return false;
  
  // Check pump status
  for(int i = 0; i < 3; i++) {
    if(pumps[i].status == PUMP_ERROR) return false;
  }
  
  return true;
}

void performSafetyCheck() {
  // Comprehensive safety check
  if (millis() < lockoutUntil) {
    return;
  }
  
  // 1. Check pH sensor readings
  if(!bufferReady) {
    LOG_PRINTLN(LOG_LEVEL_ERROR, "SAFETY WARNING: pH buffer not ready.");
    return;
  }
  
  float currentPH = getCurrentAvgPH();
  
  // 2. Check emergency pH levels
  if(currentPH <= EMERGENCY_SHUTDOWN_PH || currentPH >= EMERGENCY_SHUTDOWN_PH_HIGH) {
    LOG_PRINTLN(LOG_LEVEL_ERROR, "!!! SAFETY CRITICAL: Emergency pH levels detected!");
    emergencySystemReset();
    return;
  }
  
  // 3. Check temperature sensor
  if(currentHumidity < 0 || currentHumidity > 100) {
    LOG_PRINTLN(LOG_LEVEL_ERROR, "SAFETY WARNING: Temperature sensor out of range.");
    emergencySystemReset();
    return;
  }
  
  // 4. Check pump status untuk error conditions
  for(int i = 0; i < 3; i++) {
    if(pumps[i].status == PUMP_ERROR) {
      LOG_PRINTLN(LOG_LEVEL_ERROR, "SAFETY WARNING: Pump " + getPumpName(pumps[i].type) + " is in error state.");
    }
    
    // Check untuk pump yang stuck ON
    if(pumps[i].isActive && (millis() - pumps[i].startTime > MAX_PUMP_DURATION + 5000)) {
      LOG_PRINTLN(LOG_LEVEL_ERROR, "!!! SAFETY CRITICAL: Pump " + getPumpName(pumps[i].type) + " stuck ON!");
      emergencyStopAllPumps();
      return;
    }
  }
  
  // 5. Check system health
  if(!isSystemHealthy()) {
    LOG_PRINTLN(LOG_LEVEL_ERROR, "SAFETY WARNING: System health check failed.");
    emergencyStopAllPumps();
    return;
  }
  
  // 6. Check memory/EEPROM integrity
  if(!validateEEPROMData()) {
    LOG_PRINTLN(LOG_LEVEL_ERROR, "SAFETY WARNING: EEPROM data corrupted.");
    emergencySystemReset();
    // Don't stop pumps for this, just log
  }
  
}

// ========== EEPROM VALIDATION ==========

bool validateEEPROMData() {
  // Check settings validity marker
  if(EEPROM.readByte(ADDR_SETTINGS_VALID) != 0xAA) {
    return false;
  }
  
  // Validate target pH
  float testTargetPH = EEPROM.readFloat(ADDR_TARGET_PH);
  if(testTargetPH < 4.0 || testTargetPH > 10.0) {
    return false;
  }
  
  // Validate pump duration
  int testPumpLimeDuration = EEPROM.readInt(ADDR_PUMP_DUR_LIME);
  if(testPumpLimeDuration < 1000 || testPumpLimeDuration > 30000) {
    return false;
  }

  int testPumpAcidDuration = EEPROM.readInt(ADDR_PUMP_DUR_ACID);
  if(testPumpAcidDuration < 1000 || testPumpAcidDuration > 30000) {
    return false;
  }

  if(EEPROM.readULong(ADDR_COOLDOWN_LIME) > 900000) return false;
  if(EEPROM.readULong(ADDR_COOLDOWN_ACID) > 900000) return false;
  if(EEPROM.readULong(ADDR_COOLDOWN_WATER) > 900000) return false;
  
  return true;
}

void resetToDefaults() {
  // Reset all settings to defaults
  targetPH = 6.4;
  pumpDurationLime = 5000; // 
  pumpDurationAcid = 5000; // <-- BARU
  cooldownLime = 300000;  // 5 menit
  cooldownAcid = 300000;  // 5 menit
  cooldownWater = 60000;   // 1 menit
  LOG_PRINTLN(LOG_LEVEL_INFO, "SYSTEM: All settings have been reset to defaults.");
}

// ========== INITIALIZATION HELPERS ==========

// In HELPER_FUNCTIONS.txt
void initializeRelays() {
  pinMode(RELAY_LIME_PUMP, OUTPUT);
  pinMode(RELAY_ACID_PUMP, OUTPUT);
  pinMode(RELAY_WATER_PUMP, OUTPUT);
  
  // Atur ke HIGH untuk memastikan relay (active-LOW) mati
  digitalWrite(RELAY_LIME_PUMP, HIGH);
  digitalWrite(RELAY_ACID_PUMP, HIGH);
  digitalWrite(RELAY_WATER_PUMP, HIGH);

  LOG_PRINTLN(LOG_LEVEL_INFO, "INIT: Relays initialized.");
}

void initializeSensors() {
  // Initialize ADC for pH sensor
  analogReadResolution(12);  // 12-bit ADC for ESP32
  analogSetAttenuation(ADC_11db);  // 3.3V range
  
  LOG_PRINTLN(LOG_LEVEL_INFO, "INIT: Sensors initialized.");
}

// Function untuk reset system dalam kondisi emergency
void emergencySystemReset() {
  Serial.println("EMERGENCY: Performing system reset");
  
  // Stop all pumps
  emergencyStopAllPumps();
  
  // Reset buffer
  for(int i = 0; i < BUFFER_SIZE; i++) {
    phBuffer[i] = -1.0;
  }
  bufferIndex = 0;
  bufferReady = false;
  
  // Reset display
  displayState = DISPLAY_MAIN;
  needsRedraw = true;
  
  // Save current settings
  saveSettingsToEEPROM();
  
  Serial.println("Emergency reset completed");
}

// Function untuk watchdog timer 
void feedWatchdog() {
  // Reset timer watchdog (tanpa argumen)
  esp_task_wdt_reset();
}
