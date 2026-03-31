void handleButtons() {
  // Baca status semua tombol
  bool btnModeState = !digitalRead(BTN_MODE);
  bool btnSelectState = !digitalRead(BTN_SELECT);
  bool btnUpState = !digitalRead(BTN_UP);
  bool btnDownState = !digitalRead(BTN_DOWN);

  // Logika debounce untuk 4 tombol
  if (btnUpState && !btnUpPressed && (millis() - lastButtonPress > 200)) {
    btnUpPressed = true; lastButtonPress = millis(); handleUpButton();
  }
  if (btnDownState && !btnDownPressed && (millis() - lastButtonPress > 200)) {
    btnDownPressed = true; lastButtonPress = millis(); handleDownButton();
  }
  if (btnModeState && !btnModePressed && (millis() - lastButtonPress > 200)) {
    btnModePressed = true; lastButtonPress = millis(); handleModeButton();
  }
  if (btnSelectState && !btnSelectPressed && (millis() - lastButtonPress > 200)) {
    btnSelectPressed = true; lastButtonPress = millis(); handleSelectButton();
  }

  // Reset state
  if (!btnUpState) btnUpPressed = false;
  if (!btnDownState) btnDownPressed = false;
  if (!btnModeState) btnModePressed = false;
  if (!btnSelectState) btnSelectPressed = false;
}

void handleUpButton() {
  switch(displayState) {
    case DISPLAY_MENU: // Navigasi menu ke atas
      currentMenuItem = (MenuItem)((currentMenuItem - 1 + MENU_COUNT) % MENU_COUNT);
      needsRedraw = true;
      break;
    case DISPLAY_MODE_SELECT: // 
      pendingMode = (SystemMode)((pendingMode - 1 + MODE_COUNT) % MODE_COUNT);
      needsRedraw = true;
      break;
    case DISPLAY_VALUE_ADJUST: // Menambah nilai
      adjustValue(true);
      needsRedraw = true;
      break;
    case DISPLAY_MODE_CONFIRM: 
    case DISPLAY_YES_NO_CONFIRM:
      // Toggle pilihan: jika sedang YES, jadi NO, dan sebaliknya
      confirmSelection = (confirmSelection == CHOICE_YES) ? CHOICE_NO : CHOICE_YES;
      needsRedraw = true;
      break;
  }
}

void handleDownButton() {
  switch(displayState) {
    case DISPLAY_MENU: // Navigasi menu ke bawah
      currentMenuItem = (MenuItem)((currentMenuItem + 1) % MENU_COUNT);
      needsRedraw = true;
      break;
    case DISPLAY_MODE_SELECT: // 
      pendingMode = (SystemMode)((pendingMode + 1) % MODE_COUNT);
      needsRedraw = true;
      break;
    case DISPLAY_VALUE_ADJUST: // Mengurangi nilai
      adjustValue(false);
      needsRedraw = true;
      break;
    case DISPLAY_MODE_CONFIRM: 
    case DISPLAY_YES_NO_CONFIRM:
      // Beri fungsi yang sama persis dengan tombol NAIK
      confirmSelection = (confirmSelection == CHOICE_YES) ? CHOICE_NO : CHOICE_YES;
      needsRedraw = true;
      break;
  }
}

void handleModeButton() {
  switch(displayState) {
    case DISPLAY_MAIN:
      // Dari layar utama, masuk ke layar pemilihan mode
      pendingMode = currentMode; // Atur pilihan awal ke mode saat ini
      displayState = DISPLAY_MODE_SELECT;
      needsRedraw = true;
      break;

    default: // Dari layar lain, berfungsi sebagai tombol KEMBALI
      displayState = DISPLAY_MAIN;
      needsRedraw = true;
      break;
  }
}

void displayModeSelect() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("PILIH MODE");

  for (int i = 0; i < MODE_COUNT; i++) {
    lcd.setCursor(0, i + 1);
    if ((SystemMode)i == pendingMode) {
      lcd.write(byte(0)); // Tampilkan panah kanan
    } else {
      lcd.print(" ");
    }
    lcd.print(getModeString((SystemMode)i));
  }
}

void handleSelectButton() {
  if (isAnyPumpInError()) {
    clearPumpErrors();
    displayState = DISPLAY_MAIN; // Kembali ke layar utama
    needsRedraw = true;          // Gambar ulang layar agar kembali normal
    return;                      // Keluar dari fungsi, jangan proses yang lain
  }

  switch(displayState) {
    case DISPLAY_MAIN:
      // Masuk ke menu
      if(currentMode == MODE_SETTINGS) {
        displayState = DISPLAY_MENU;
        currentMenuItem = MENU_PH_TARGET;
      } else {
        executeMainAction();
      }
      needsRedraw = true;
      break;

    case DISPLAY_MODE_SELECT:
      // Pindah ke layar konfirmasi untuk mode yang dipilih
      displayState = DISPLAY_MODE_CONFIRM;
      confirmSelection = CHOICE_NO; // Default ke "No" untuk keamanan
      needsRedraw = true;
      break;
      
    case DISPLAY_MODE_CONFIRM:
      if (confirmSelection == CHOICE_YES) {
          currentMode = pendingMode; // Terapkan mode baru
          LOG_PRINTLN(LOG_LEVEL_INFO, "UI: Mode changed to " + getModeString(currentMode));
      }
      displayState = DISPLAY_MAIN; // Kembali ke layar utama
      needsRedraw = true;
      break;
      
    case DISPLAY_MENU:
      // Execute menu action
      executeMenuAction();
      break;

    case DISPLAY_VALUE_ADJUST:
        // PERIKSA: Apakah ini item pengaturan yang bisa disimpan?
        if (currentMenuItem == MENU_PH_TARGET || currentMenuItem == MENU_PUMP_DUR_LIME || currentMenuItem == MENU_PUMP_DUR_ACID ||
            currentMenuItem == MENU_COOLDOWN_LIME || currentMenuItem == MENU_COOLDOWN_ACID ||
            currentMenuItem == MENU_COOLDOWN_WATER) {
            displayState = DISPLAY_YES_NO_CONFIRM;
            confirmSelection = CHOICE_YES;
            needsRedraw = true;
        } 
        // JIKA BUKAN PENGATURAN, PERIKSA: Apakah ini tes pompa manual?
        else if (currentMenuItem == MENU_MANUAL_PUMP_LIME || 
                   currentMenuItem == MENU_MANUAL_PUMP_ACID || 
                   currentMenuItem == MENU_MANUAL_PUMP_WATER) {
            // Jika YA, jalankan logika asli untuk memulai tes pompa.
            pumpToTest = (currentMenuItem == MENU_MANUAL_PUMP_LIME) ? PUMP_LIME : 
                        (currentMenuItem == MENU_MANUAL_PUMP_ACID) ? PUMP_ACID : 
                        PUMP_WATER; 
            pumpTestStartTime = millis(); 
            displayState = DISPLAY_PUMP_TESTING;
            
            // Nyalakan relay pompa yang akan dites
            digitalWrite(getPumpControl(pumpToTest)->relayPin, LOW); 
            LOG_PRINTLN(LOG_LEVEL_INFO, "PUMP TEST: Started for " + String(pumpTestDuration) + "ms"); 
            needsRedraw = true; 
        }
        break;

    case DISPLAY_RUNTIME_INFO: 
      // Pindah ke layar konfirmasi untuk aksi reset
      currentMenuItem = MENU_RESET_RUNTIME; // Atur item menu agar konfirmasi tahu apa yang harus dilakukan
      displayState = DISPLAY_YES_NO_CONFIRM;
      confirmSelection = CHOICE_NO; // Default ke "No" untuk keamanan
      needsRedraw = true;
      break;

    case DISPLAY_YES_NO_CONFIRM:
      if (confirmSelection == CHOICE_YES) {
        executeConfirmAction();
      }
      displayState = DISPLAY_MENU;
      needsRedraw = true;
      break;
    }
}

void adjustValue(bool increase) {
  float increment = 0.1;
  
  switch(currentMenuItem) {
    case MENU_PH_TARGET:
      targetPH += increase ? increment : -increment;
      targetPH = constrain(targetPH, 4.0, 10.0);
      break;

    case MENU_PUMP_DUR_LIME:  // <-- DIUBAH
      pumpDurationLime += increase ? 500 : -500;
      pumpDurationLime = constrain(pumpDurationLime, 1000, 30000);
      break;

    case MENU_PUMP_DUR_ACID:  // <-- BARU
      pumpDurationAcid += increase ? 500 : -500;
      pumpDurationAcid = constrain(pumpDurationAcid, 1000, 30000);
      break;

    case MENU_COOLDOWN_LIME:
      cooldownLime += increase ? 30000 : -30000;
      cooldownLime = constrain(cooldownLime, 30000, 900000); // Batasi 30 detik - 15 menit
      break;

    case MENU_COOLDOWN_ACID:
      cooldownAcid += increase ? 30000 : -30000;
      cooldownAcid = constrain(cooldownAcid, 30000, 900000); // Batasi 30 detik - 15 menit
      break;

    case MENU_COOLDOWN_WATER:
      cooldownWater += increase ? 10000 : -10000;
      cooldownWater = constrain(cooldownWater, 0, 300000); // Batasi 0 detik - 5 menit
      break;

    case MENU_MANUAL_PUMP_LIME:
    case MENU_MANUAL_PUMP_ACID:
    case MENU_MANUAL_PUMP_WATER:
      pumpTestDuration += increase ? 1000 : -1000; // Tambah/kurang 1 detik
      pumpTestDuration = constrain(pumpTestDuration, 1000, 10000); // Batasi 1-10 detik
      break;
  }
}

void updateDisplay() {
    // Cek kondisi error sebagai prioritas utama
  if (isAnyPumpInError() && displayState == DISPLAY_MAIN) {
    displayErrorScreen();
    return; // Hentikan proses, jangan tampilkan layar lain
  }

  switch(displayState) {
    case DISPLAY_MAIN:
      displayMainScreen();
      break;
    case DISPLAY_MODE_SELECT: 
      displayModeSelect();
      break;
    case DISPLAY_MODE_CONFIRM:
      displayModeConfirm();
      break;
    case DISPLAY_MENU:
      displayMenu();
      break;
    case DISPLAY_VALUE_ADJUST:
      displayValueAdjust();
      break;
    case DISPLAY_YES_NO_CONFIRM:
      displayYesNoConfirm();
      break;
    case DISPLAY_PUMP_TESTING:
      displayPumpTesting();
      break;
    case DISPLAY_RUNTIME_INFO: 
      displayRuntimeInfo();
      break;
  }
}

void displayMainScreen() {
  lcd.clear();
  
  switch(currentMode) {
    case MODE_AUTO:
      displayAutoMode();
      break;
    case MODE_CALIBRATION:
      displayCalibrationMode();
      break;
    case MODE_SETTINGS:
      displaySettingsMode();
      break;
  }
}

void displayCooldownScreen(PumpType type, unsigned long remaining, unsigned long total) {
  lcd.clear();

  // Baris 1: Header Status
  lcd.setCursor(0, 0);
  lcd.print("CIRCULATING");
  
  lcd.setCursor(13, 0); // Pindah ke tengah untuk kelembapan
  lcd.print("H:" + String((int)currentHumidity) + "%"); // Tampilkan kelembapan sebagai integer

  // Baris 2: Info Pompa
  lcd.setCursor(0, 1);
  String pumpName = getPumpName(type);
  lcd.print(pumpName);
  lcd.print("   Cooldown");

  // Baris 3: Tampilkan pH dan Kelembapan di sebelah kiri
  lcd.setCursor(0, 2);
  lcd.print("pH:" + String(currentAvgPH, 2)); // Tampilkan pH dengan 2 desimal

  // Lanjutkan dengan menampilkan waktu cooldown di sebelah kanan pada baris yang sama
  unsigned long remainingSeconds = remaining / 1000;
  unsigned long minutes = remainingSeconds / 60;
  unsigned long seconds = remainingSeconds % 60;
  String timeStr = String(minutes) + "m" + String(seconds) + "s";

  // Atur posisi agar rata kanan
  lcd.setCursor(20 - timeStr.length(), 2);
  lcd.print(timeStr);

  // Baris 4: Progress Bar
  lcd.setCursor(0, 3);
  lcd.print("[");
  unsigned long elapsed = total - remaining;
  int progress = (elapsed * 18) / total;
  for (int i = 0; i < 18; i++) {
    if (i < progress) {
      lcd.write(255); // Karakter blok penuh
    } else {
      lcd.print("."); // Bar kosong
    }
  }
  lcd.print("]");
}

void displayPumpTesting() {
  static unsigned long lastUpdate = 0;
  static int animationFrame = 0;
  
  if (millis() - lastUpdate < 250) return;
  lastUpdate = millis();
  
  lcd.clear();
  
  // Hitung waktu dan progress awal
  unsigned long elapsedTime = millis() - pumpTestStartTime;
  unsigned long remainingTime = (pumpTestDuration > elapsedTime) ? 
                                (pumpTestDuration - elapsedTime) / 1000 : 0;
  int progress = (elapsedTime * 100L) / pumpTestDuration;
  
  if (remainingTime == 0) {
    progress = 100;
  }
  // Pastikan progress tidak lebih dari 100
  if (progress > 100) progress = 100;
  
  // Baris 1: Header
  lcd.setCursor(0, 0);
  lcd.print("    TES POMPA     ");
  lcd.setCursor(19, 0);
  lcd.write(4 + (animationFrame % 4));
  
  // Baris 2: Nama Pompa dan Status
  lcd.setCursor(0, 1);
  String pumpName = getPumpName(pumpToTest);
  if (pumpName.length() > 14) {
    pumpName = pumpName.substring(0, 14);
  }
  lcd.print(char(0)); 
  lcd.print(" ");
  lcd.print(pumpName);
  lcd.setCursor(18, 1);
  if (remainingTime > 0) {
    lcd.print(animationFrame % 2 == 0 ? "ON" : "  ");
  } else {
    lcd.print("OK");
  }
  
  // Baris 3: Progress Bar (Selalu digambar)
  lcd.setCursor(0, 2);
  lcd.print("[");
  int filledBars = (progress * 18) / 100;
  for (int i = 0; i < 18; i++) {
    if (i < filledBars) {
      lcd.print(char(255)); // Blok penuh
    } else if (i == filledBars && remainingTime > 0) {
      lcd.write(4 + (animationFrame % 4)); // Animasi loading
    } else {
      lcd.print("."); // Bar kosong/selesai
    }
  }
  lcd.print("]");
  
  // Baris 4: Countdown atau Pesan Selesai
  lcd.setCursor(0, 3);
  if (remainingTime > 0) {
    // Tampilkan countdown timer
    String timeStr = (remainingTime < 10) ? "0" + String(remainingTime) : String(remainingTime);
    lcd.print("   Sisa: ");
    lcd.print(timeStr);
    lcd.print("s ");
    lcd.setCursor(15, 3);
    lcd.print(progress);
    lcd.print("%");
  } else {
    // Tampilkan pesan tes selesai
    lcd.print("     TES SELESAI!     ");
  }
  
  // Update frame animasi
  animationFrame++;
  if (animationFrame >= 100) animationFrame = 0;
}

void displayErrorScreen() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("   SYSTEM ERROR   "); // 
  lcd.setCursor(0, 1);
  lcd.print("   Pump Failure   ");
  lcd.setCursor(0, 3);
  lcd.print("SELECT --> Reset"); // Arahkan pengguna untuk menekan SELECT
}

void displayAutoMode() {
  // Cek apakah ada pompa yang sedang cooldown sebagai prioritas
  for (int i = 0; i < 3; i++) { // Cek semua pompa
    if (getPumpStatus(pumps[i].type) == PUMP_COOLDOWN) {
      unsigned long remaining = getCooldownRemaining(&pumps[i]);
      unsigned long totalCooldown;
      switch(pumps[i].type) {
        case PUMP_LIME: totalCooldown = cooldownLime; break;
        case PUMP_ACID: totalCooldown = cooldownAcid; break;
        case PUMP_WATER: totalCooldown = cooldownWater; break;
      }
      if (remaining > 0) {
        displayCooldownScreen(pumps[i].type, remaining, totalCooldown);
        return; // Tampilkan layar cooldown dan hentikan eksekusi fungsi ini
      }
    }
  }
  // Cek apakah sedang dalam masa tenang
  if (millis() < lockoutUntil) {
    unsigned long remainingSeconds = (lockoutUntil - millis()) / 1000;
    unsigned long minutes = remainingSeconds / 60;
    unsigned long seconds = remainingSeconds % 60;

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("   STABILIZING    ");
    lcd.setCursor(0, 1);
    lcd.print("  Please Wait...  ");
    lcd.setCursor(0, 2);
    String timeStr = "Time Left: " + String(minutes) + "m " + String(seconds) + "s";
    lcd.print(timeStr);

  } else {
    lcd.setCursor(0, 0);
    lcd.print("AUTO Mode  pH:");
    lcd.print(getCurrentAvgPH(), 1);
    
    lcd.setCursor(0, 1);
    lcd.print("Target: ");
    lcd.print(targetPH, 1);
    lcd.print(" H:"); // "H" untuk Humidity
    lcd.print(getCurrentHumidity(), 0); // Panggil fungsi getter yang sudah diubah namanya
    lcd.print("%");

    lcd.setCursor(0, 2);
    lcd.print("Status: ");
    lcd.print(getSystemStatus());

      // Baris 2: Progress Bar Sampling
    lcd.setCursor(0, 3);
    lcd.print("Sample [");
    for (int i = 0; i < 10; i++) {
      if (i < samplingProgress) {
        lcd.write(255); // Karakter blok penuh
      } else {
        lcd.print("-");
      }
    }
    lcd.print("]");
  }
}

// Kode untuk menampilkan instruksi Kalibrasi Cepat 1-Titik
void displayCalibrationMode() {
  // Ambil nilai sensor terbaru
  int rawPH = getRawSensorValue();      // Ambil nilai ADC mentah
  float calPH = calibratePH(rawPH);     // Konversi ke nilai pH

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("   KALIBRASI CEPAT  ");
  
  lcd.setCursor(0, 1);
  lcd.print("Celupkan ke pH 7.0");
  
  // Tampilkan ADC dan pH secara berdampingan
  lcd.setCursor(0, 2);
  // Format: "ADC:1234 pH:7.00"
  lcd.print("ADC:");
  lcd.print(rawPH);
  lcd.setCursor(9, 2); // Geser kursor agar rapi
  lcd.print("pH:");
  lcd.print(calPH, 2);

  lcd.setCursor(0, 3);
  lcd.print("SELECT----->Simpan");
}

void displaySettingsMode() {
  lcd.setCursor(0, 0);
  lcd.print("SETTINGS Mode");
  
  lcd.setCursor(0, 1);
  lcd.print("pH Target: ");
  lcd.print(targetPH, 1);
  
  lcd.setCursor(0, 2);
  lcd.print("Pump Time: ");
  lcd.print(pumpDurationLime/1000);
  lcd.print("s");
  
  lcd.setCursor(0, 3);
  lcd.print("MODE");
  lcd.write(2);
  lcd.print(" SELECT:Menu");
}

void displayModeConfirm() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Ganti ke mode");
  lcd.setCursor(0, 1);
  lcd.print(getModeString(pendingMode));
  lcd.print("?");

  // Menampilkan pilihan Yes/No dengan panah kustom di baris ke-3
  if (confirmSelection == CHOICE_YES) {
    // Panah menunjuk ke "Yes"
    lcd.setCursor(0, 3);
    lcd.write(byte(0)); // Cetak karakter panah kanan
    lcd.print("Yes");
    lcd.setCursor(10, 3);
    lcd.print(" No "); // Spasi untuk menimpa panah jika sebelumnya ada di sini
  } else { // confirmSelection == CHOICE_NO
    // Panah menunjuk ke "No"
    lcd.setCursor(0, 3);
    lcd.print(" Yes"); // Spasi untuk menimpa panah jika sebelumnya ada di sini
    lcd.setCursor(10, 3);
    lcd.write(byte(0)); // Cetak karakter panah kanan
    lcd.print("No");
  }
}

void displayMenu() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("MENU");
  
  // Show current menu item
  lcd.setCursor(0, 1);
  lcd.write(0);  // Arrow right
  lcd.print(getMenuItemName(currentMenuItem));
  
  // Show value if applicable
  lcd.setCursor(0, 2);
  lcd.print("Value: ");
  lcd.print(getMenuItemValue());
  
  lcd.setCursor(0, 3);
  lcd.print("MODE");
  lcd.write(2);
  lcd.write(3);  // Up/Down arrows
  lcd.print(" SELECT:Edit");
}

void displayValueAdjust() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("ADJUST VALUE");
  
  lcd.setCursor(0, 1);
  lcd.print(getMenuItemName(currentMenuItem));
  
  lcd.setCursor(0, 2);
  lcd.write(1);  // Left arrow
  lcd.print(" ");
  lcd.print(getMenuItemValue());
  lcd.print(" ");
  lcd.write(0);  // Right arrow
  
  lcd.setCursor(0, 3);
  lcd.print("MODE");
  lcd.write(2);
  lcd.write(3);
  lcd.print(" SELECT:Save");
}

void displayYesNoConfirm() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("CONFIRM ACTION");
  
  lcd.setCursor(0, 1);
  lcd.print(getMenuItemName(currentMenuItem));
  
  lcd.setCursor(0, 2);
  lcd.print("Are you sure?");
  
  lcd.setCursor(0, 3);
  if (confirmSelection == CHOICE_YES) {
    lcd.setCursor(0, 3); lcd.write(byte(0)); lcd.print("YES");
    lcd.setCursor(10, 3); lcd.print(" NO");
  } else {
    lcd.setCursor(0, 3); lcd.print(" YES");
    lcd.setCursor(10, 3); lcd.write(byte(0)); lcd.print("NO");
  }
}

void showQuickIntro() {
  lcd.clear();
  // Quick wave
  waveAnimation();
  
  lcd.clear();
  typewriterPrint("pH Monitor", 1, 5); // Tulis "pH Monitor" di baris 1, kolom 4
  typewriterPrint("ESP32 System", 2, 4); // Tulis "ESP32 System" di baris 2, kolom 3
  
  // Quick border
  lcd.setCursor(0, 0);
  lcd.print("====================");
  lcd.setCursor(0, 3);
  lcd.print("====================");
  
  delay(1500);
}

void waveAnimation() {
  lcd.clear();
  for (int pos = 0; pos < 20; pos++) {
    for (int wave = 4; wave < 7; wave++) {
      lcd.setCursor(pos, 1);
      lcd.write(byte(wave));
      delay(50);
      if (pos > 0) {
        lcd.setCursor(pos - 1, 1);
        lcd.print(" ");
      }
    }
  }
  lcd.clear();
}

// Fungsi untuk menampilkan teks dengan efek mesin tik
void typewriterPrint(String text, int row, int col) {
  for (int i = 0; i < text.length(); i++) {
    lcd.setCursor(col + i, row);
    lcd.print(text[i]);
    delay(100); // Atur kecepatan ketikan di sini (ms)
  }
}

// Menu dan helper functions
String getModeString(SystemMode mode) {
  switch(mode) {
    case MODE_AUTO: return "AUTO";
    case MODE_CALIBRATION: return "CALIBRATION";
    case MODE_SETTINGS: return "SETTINGS";
    default: return "UNKNOWN";
  }
}

String getMenuItemName(MenuItem item) {
  switch(item) {
    case MENU_PH_TARGET: return "pH Target";
    case MENU_PUMP_DUR_LIME: return "Durasi Pompa Kapur"; 
    case MENU_PUMP_DUR_ACID: return "Durasi Pompa Asam";  
    case MENU_COOLDOWN_LIME:
      return "Cooldown Kapur";
    case MENU_COOLDOWN_ACID:
      return "Cooldown Asam";
    case MENU_COOLDOWN_WATER:
      return "Cooldown Air";
    case MENU_RESET_CALIBRATION: return "Reset Kalibrasi";
    case MENU_RESET_DEFAULTS: return "Reset Defaults";
    case MENU_MANUAL_PUMP_LIME: return "Tes Pompa Kapur";
    case MENU_MANUAL_PUMP_ACID: return "Tes Pompa Asam";
    case MENU_MANUAL_PUMP_WATER: return "Tes Pompa Air";
    case MENU_SHOW_RUNTIME: return "Show Runtime Pompa"; 
    case MENU_RESET_RUNTIME: return "Reset Runtime";
    case MENU_BACK: return "Back";
    default: return "Unknown";
  }
}

String getMenuItemValue() {
  switch(currentMenuItem) {
    case MENU_PH_TARGET: return String(targetPH, 1);
    case MENU_PUMP_DUR_LIME: return String(pumpDurationLime / 1000.0, 1) + "s"; 
    case MENU_PUMP_DUR_ACID: return String(pumpDurationAcid / 1000.0, 1) + "s"; 
    case MENU_COOLDOWN_LIME: {
      unsigned long totalSeconds = cooldownLime / 1000;
      return String(totalSeconds / 60) + "m " + String(totalSeconds % 60) + "s";
    }
    case MENU_COOLDOWN_ACID: {
      unsigned long totalSeconds = cooldownAcid / 1000;
      return String(totalSeconds / 60) + "m " + String(totalSeconds % 60) + "s";
    }
    case MENU_COOLDOWN_WATER: {
      unsigned long totalSeconds = cooldownWater / 1000;
      return String(totalSeconds / 60) + "m " + String(totalSeconds % 60) + "s";
    }
    case MENU_RESET_CALIBRATION:
      return String(calibrationOffset, 4); 
    case MENU_MANUAL_PUMP_LIME:
    case MENU_MANUAL_PUMP_ACID:
    case MENU_MANUAL_PUMP_WATER:
      return String(pumpTestDuration / 1000) + "s";
    default: return "N/A";
  }
}

void executeMenuAction() {
  switch(currentMenuItem) {
    case MENU_PH_TARGET:
    case MENU_PUMP_DUR_LIME:
    case MENU_PUMP_DUR_ACID:
      displayState = DISPLAY_VALUE_ADJUST;
      break;
    case MENU_COOLDOWN_LIME:
    case MENU_COOLDOWN_ACID:
    case MENU_COOLDOWN_WATER:
      displayState = DISPLAY_VALUE_ADJUST;
      break;
    case MENU_RESET_CALIBRATION:
      // Kembalikan offset ke nilai default dari datasheet
      calibrationOffset = 7.7851;

      // Simpan nilai default ini kembali ke EEPROM
      EEPROM.writeFloat(ADDR_CALIBRATION_OFFSET, calibrationOffset);
      EEPROM.commit();

      // Beri pesan konfirmasi ke pengguna
      lcd.clear();
      lcd.setCursor(0, 1);
      lcd.print("  Reset Berhasil!  ");
      delay(2000);
      break;

    case MENU_RESET_DEFAULTS:
      displayState = DISPLAY_YES_NO_CONFIRM;
      confirmSelection = CHOICE_NO; // Default "No" untuk keamanan
      needsRedraw = true;
      break;

    case MENU_MANUAL_PUMP_LIME:
    case MENU_MANUAL_PUMP_ACID:
    case MENU_MANUAL_PUMP_WATER:
      displayState = DISPLAY_VALUE_ADJUST; // Pindah ke layar atur nilai
      break;

    case MENU_SHOW_RUNTIME: 
      displayState = DISPLAY_RUNTIME_INFO;
      break;
      
    case MENU_BACK:
      displayState = DISPLAY_MAIN;
      break;
  }
  needsRedraw = true;
}

// Tambahkan fungsi baru ini
void displayRuntimeInfo() {
  lcd.clear();

  // Ambil dan format waktu untuk setiap pompa
  unsigned long limeSeconds = getTotalRunTime(PUMP_LIME) / 1000;
  unsigned long acidSeconds = getTotalRunTime(PUMP_ACID) / 1000;
  unsigned long waterSeconds = getTotalRunTime(PUMP_WATER) / 1000;

  String limeStr = String(limeSeconds / 60) + "m " + String(limeSeconds % 60) + "s";
  String acidStr = String(acidSeconds / 60) + "m " + String(acidSeconds % 60) + "s";
  String waterStr = String(waterSeconds / 60) + "m " + String(waterSeconds % 60) + "s";

  lcd.setCursor(0, 0);
  lcd.print("LIME : " + limeStr);
  lcd.setCursor(0, 1);
  lcd.print("ACID : " + acidStr);
  lcd.setCursor(0, 2);
  lcd.print("WATER: " + waterStr);
  lcd.setCursor(0, 3);

  lcd.print("SELECT---->Reset");
}

void executeConfirmAction() {
  switch(currentMenuItem) {
    case MENU_PH_TARGET:
      EEPROM.writeFloat(ADDR_TARGET_PH, targetPH); 
      EEPROM.writeByte(ADDR_SETTINGS_VALID, 0xAA);
      EEPROM.commit(); 
      LOG_PRINTLN(LOG_LEVEL_INFO, "UI: pH Target saved via confirm: " + String(targetPH, 1)); 
      break;

    case MENU_PUMP_DUR_LIME: // <-- DIUBAH
      EEPROM.writeInt(ADDR_PUMP_DUR_LIME, pumpDurationLime);
      EEPROM.writeByte(ADDR_SETTINGS_VALID, 0xAA);
      EEPROM.commit();
      LOG_PRINTLN(LOG_LEVEL_INFO, "UI: Lime Pump Duration saved: " + String(pumpDurationLime));
      break;

    case MENU_PUMP_DUR_ACID: // <-- BARU
      EEPROM.writeInt(ADDR_PUMP_DUR_ACID, pumpDurationAcid);
      EEPROM.writeByte(ADDR_SETTINGS_VALID, 0xAA);
      EEPROM.commit();
      LOG_PRINTLN(LOG_LEVEL_INFO, "UI: Acid Pump Duration saved: " + String(pumpDurationAcid));
      break;

    case MENU_COOLDOWN_LIME:
      EEPROM.put(ADDR_COOLDOWN_LIME, cooldownLime);
      EEPROM.writeByte(ADDR_SETTINGS_VALID, 0xAA);
      EEPROM.commit();
      LOG_PRINTLN(LOG_LEVEL_INFO, "UI: Lime Cooldown saved: " + String(cooldownLime));
      break;

    case MENU_COOLDOWN_ACID:
      EEPROM.put(ADDR_COOLDOWN_ACID, cooldownAcid);
      EEPROM.writeByte(ADDR_SETTINGS_VALID, 0xAA);
      EEPROM.commit();
      LOG_PRINTLN(LOG_LEVEL_INFO, "UI: Acid Cooldown saved: " + String(cooldownAcid));
      break;

    case MENU_COOLDOWN_WATER:
      EEPROM.put(ADDR_COOLDOWN_WATER, cooldownWater);
      EEPROM.writeByte(ADDR_SETTINGS_VALID, 0xAA);
      EEPROM.commit();
      LOG_PRINTLN(LOG_LEVEL_INFO, "UI: Water Cooldown saved: " + String(cooldownWater));
      break;

    case MENU_RESET_CALIBRATION:
      // Kembalikan offset ke nilai default dari datasheet
      calibrationOffset = 7.7851;

      // Simpan nilai default ini kembali ke EEPROM
      EEPROM.writeFloat(ADDR_CALIBRATION_OFFSET, calibrationOffset);
      EEPROM.writeByte(ADDR_SETTINGS_VALID, 0xAA);
      EEPROM.commit();

      // Beri pesan konfirmasi ke pengguna
      lcd.clear();
      lcd.setCursor(0, 1);
      lcd.print("  Reset Berhasil!  ");
      delay(2000);
      break;

    case MENU_RESET_RUNTIME: // <-- TAMBAHKAN BLOK INI
      resetPumpRuntimes(); // Panggil fungsi reset yang sudah dibuat
      // Tampilkan pesan sukses
      lcd.clear();
      lcd.setCursor(0, 1);
      lcd.print("  Runtime Direset!  ");
      delay(2000);
      // Setelah direset, kembali ke layar info runtime
      displayState = DISPLAY_RUNTIME_INFO;
      break;
      
    case MENU_RESET_DEFAULTS: // Tambahkan blok case ini
      resetToDefaults(); // Panggil fungsi reset di sini
      break;
  }
}

void handleAutoMode() {
  if (millis() < lockoutUntil) {
    return; // Jangan lakukan apa-apa, masih dalam masa tenang
  }

  if(!bufferReady) return;
  float currentPH = getCurrentAvgPH(); 
  float currentHum = getCurrentHumidity();

  // Cek apakah pompa sudah aktif atau dalam masa cooldown untuk mencegah panggilan berulang
  bool isLimePumpAvailable = (getPumpStatus(PUMP_LIME) == PUMP_OFF);
  bool isAcidPumpAvailable = (getPumpStatus(PUMP_ACID) == PUMP_OFF);
  bool isWaterPumpAvailable = (getPumpStatus(PUMP_WATER) == PUMP_OFF);

  // Hysteresis logic
  if(currentPH < (targetPH - 0.6) && isLimePumpAvailable) {
    // pH terlalu rendah, aktifkan pompa lime
    activatePump(PUMP_LIME, pumpDurationLime); 
  }
  else if(currentPH > (targetPH + 0.6) && isAcidPumpAvailable) {
    // pH terlalu tinggi, aktifkan pompa acid
    activatePump(PUMP_ACID, pumpDurationAcid);
  }
  else if (currentHum < MINIMUM_HUMIDITY && isWaterPumpAvailable) {
    // Pemeriksaan keamanan: Pastikan tidak ada pompa lain yang aktif
    if (!isPumpActive(PUMP_LIME) && !isPumpActive(PUMP_ACID)) {
      LOG_PRINTLN(LOG_LEVEL_INFO, "AUTO: Humidity low, activating water pump.");
      activatePump(PUMP_WATER, 10000); // Nyalakan pompa air selama 10 detik
    }
  }
  // Dead zone: targetPH ± 0.6
}

void handleCalibrationMode() {
  // Fungsi ini hanya untuk me-refresh layar kalibrasi secara berkala
  // agar pengguna bisa melihat pembacaan pH live yang stabil.
  
  static unsigned long lastCalibrationUpdate = 0;
  
  if(millis() - lastCalibrationUpdate > 1000) { // Update setiap 1 detik
    needsRedraw = true;
    lastCalibrationUpdate = millis();
  }
}

// Function untuk save semua settings
void saveSettingsToEEPROM() {
  EEPROM.writeFloat(ADDR_TARGET_PH, targetPH);
  EEPROM.writeInt(ADDR_PUMP_DUR_ACID, pumpDurationAcid);
  EEPROM.writeInt(ADDR_PUMP_DUR_LIME, pumpDurationLime);

  EEPROM.writeULong(ADDR_COOLDOWN_LIME, cooldownLime);
  EEPROM.writeULong(ADDR_COOLDOWN_ACID, cooldownAcid);
  EEPROM.writeULong(ADDR_COOLDOWN_WATER, cooldownWater);
  
  // Mark settings as valid
  EEPROM.writeByte(ADDR_SETTINGS_VALID, 0xAA);
  
  EEPROM.commit();
  LOG_PRINTLN(LOG_LEVEL_INFO, "EEPROM: All settings saved.");
}

// Function untuk load settings dari EEPROM
void loadSettingsFromEEPROM() {
  // Check if settings valid
  if(EEPROM.readByte(ADDR_SETTINGS_VALID) != 0xAA) {
    LOG_PRINTLN(LOG_LEVEL_INFO, "EEPROM: No valid settings found, using defaults.");
    
    // Inisialisasi runtime ke 0 jika belum ada data valid
    EEPROM.put(ADDR_LIME_RUNTIME, (unsigned long)0);
    EEPROM.put(ADDR_ACID_RUNTIME, (unsigned long)0);
    EEPROM.put(ADDR_WATER_RUNTIME, (unsigned long)0);
    EEPROM.commit();

    return;
  }
  
  targetPH = EEPROM.readFloat(ADDR_TARGET_PH);
  pumpDurationLime = EEPROM.readInt(ADDR_PUMP_DUR_LIME);   // <-- DIUBAH
  pumpDurationAcid = EEPROM.readInt(ADDR_PUMP_DUR_ACID);   // <-- BARU
  cooldownLime = EEPROM.readULong(ADDR_COOLDOWN_LIME);
  cooldownAcid = EEPROM.readULong(ADDR_COOLDOWN_ACID);
  cooldownWater = EEPROM.readULong(ADDR_COOLDOWN_WATER);
  
  LOG_PRINTLN(LOG_LEVEL_INFO, "EEPROM: Settings loaded (Target pH: " + String(targetPH, 1) + ", Pump ACID: " + String(pumpDurationAcid) + "ms)." + ", Pump Lime: " + String(pumpDurationLime) + "ms).");
}