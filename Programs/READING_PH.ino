void readSensors() {
  // Baca sensor pH
  int rawPH = analogRead(PH_PIN);
  float calibratedPH = calibratePH(rawPH);
  
  // Baca sensor suhu (untuk kompensasi)
  int rawHumidity = analogRead(HUMIDITY_PIN); // Gunakan pin dan variabel baru
  currentHumidity = calibrateHumidity(rawHumidity); // Panggil fungsi baru
  
  needsRedraw = true;
  samplingProgress = 0; 

  // Validasi data
  if(isValidPH(calibratedPH)) {
    // Masukkan ke circular buffer
    phBuffer[bufferIndex] = calibratedPH;
    bufferIndex = (bufferIndex + 1) % BUFFER_SIZE;
    
    // Update moving average
    calculateMovingAverage();

    // Debug output
    LOG_PRINT(LOG_LEVEL_DEBUG, "Sensor: Raw=");
    LOG_PRINT(LOG_LEVEL_DEBUG, rawPH);
    LOG_PRINT(LOG_LEVEL_DEBUG, ", pH=");
    LOG_PRINT(LOG_LEVEL_DEBUG, calibratedPH, 2);
    LOG_PRINT(LOG_LEVEL_DEBUG, ", Avg=");
    LOG_PRINT(LOG_LEVEL_DEBUG, currentAvgPH, 2);
    LOG_PRINT(LOG_LEVEL_DEBUG, ", Humidity=");
    LOG_PRINTLN(LOG_LEVEL_DEBUG, currentHumidity, 1);
    } else {
      // Pesan ini juga hanya untuk debug
      LOG_PRINTLN(LOG_LEVEL_DEBUG, "Invalid pH reading - skipped");
    }
}

void calculateMovingAverage() {
  float sum = 0.0;
  int validCount = 0;
  
  // Hitung rata-rata dari data valid
  for(int i = 0; i < BUFFER_SIZE; i++) {
    if(phBuffer[i] != -1.0) {  // Data valid
      sum += phBuffer[i];
      validCount++;
    }
  }
  
  if(validCount >= 3) {  // Minimal 3 data untuk rata-rata
    currentAvgPH = sum / validCount;
    bufferReady = true;
  } else {
    bufferReady = false;
  }
}

// Ganti fungsi calibratePH() yang lama dengan ini
float calibratePH(float value) {
  // Selalu gunakan rumus dari datasheet, tapi dengan offset yang dinamis
  return (-0.0139 * value) + calibrationOffset;
}

// Ganti fungsi calibrateTemp 
float calibrateHumidity(int rawHumidity) {
  // Sesuaikan dengan sensor yang dipakai (misal: HR202, SY-HS-220)
  float humidity = map(rawHumidity, 2500, 1000, 0, 100);

  // Pastikan nilai berada dalam rentang 0-100%
  if (humidity < 0) humidity = 0;
  if (humidity > 100) humidity = 100;

  return humidity;
}

bool isValidPH(float ph) {
  // Validasi range pH yang masuk akal
  if(ph < 0.0 || ph > 14.0) return false;
  if(isnan(ph) || isinf(ph)) return false;
  
  return true;
}

// Getter functions untuk dipakai di bagian lain
float getCurrentAvgPH() {
  return currentAvgPH;
}

float getCurrentHumidity() {
  return currentHumidity;
}

bool isBufferReady() {
  return bufferReady;
}