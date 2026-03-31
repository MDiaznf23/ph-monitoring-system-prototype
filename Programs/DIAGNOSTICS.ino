// ========== DATA LOGGING FOR GRAPHING ==========

void logDataForGraph() {
  // Dapatkan timestamp dalam detik
  unsigned long currentTimeSeconds = millis() / 1000;

  // Dapatkan nilai kelembapan saat ini
  float humidity = getCurrentHumidity();

  // Cek apakah pompa air sedang aktif (1 untuk aktif, 0 untuk tidak aktif)
  int waterPumpStatus = isPumpActive(PUMP_WATER) ? 1 : 0;

  // Cetak dalam format CSV: Waktu,Kelembapan,StatusPompaAir
  Serial.print(currentTimeSeconds);
  Serial.print(",");
  Serial.print(humidity, 2); // Cetak dengan 2 angka desimal
  Serial.print(",");
  Serial.println(waterPumpStatus);
}

void logpHDataForGraph() {
  // Dapatkan timestamp dalam detik
  unsigned long currentTimeSeconds = millis() / 1000;

  float currentPH = getCurrentAvgPH();

  float phTarget = targetPH;

  int acidPumpStatus = isPumpActive(PUMP_ACID) ? 1 : 0;

  int limePumpStatus = isPumpActive(PUMP_LIME) ? 1 : 0;

  // Cetak semua data dalam satu baris format CSV
  Serial.print(currentTimeSeconds);
  Serial.print(",");
  Serial.print(currentPH, 2); // pH dengan 2 desimal
  Serial.print(",");
  Serial.print(phTarget, 2); // Target pH dengan 2 desimal
  Serial.print(",");
  Serial.print(acidPumpStatus);
  Serial.print(",");
  Serial.println(limePumpStatus);
}