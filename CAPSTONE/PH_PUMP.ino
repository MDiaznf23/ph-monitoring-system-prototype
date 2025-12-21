// Main function untuk aktivasi pompa
bool activatePump(PumpType pumpType, unsigned long durationMs) {
  // Validasi input
  if(durationMs > MAX_PUMP_DURATION) {
    LOG_PRINTLN(LOG_LEVEL_ERROR, "PUMP ERROR: Duration too long");
    durationMs = MAX_PUMP_DURATION;
  }
  
  // Cari pump control
  PumpControl* pump = getPumpControl(pumpType);
  if(pump == nullptr) {
    LOG_PRINTLN(LOG_LEVEL_ERROR, "PUMP ERROR: Invalid pump type");
    return false;
  }
  
  // Check safety conditions
  if(!isSafeToActivate(pumpType)) {
    LOG_PRINTLN(LOG_LEVEL_ERROR, "PUMP ERROR: Not safe to activate");
    return false;
  }
  
  // Check cooldown
  if(pump->status == PUMP_COOLDOWN) {
    unsigned long cooldownRemaining = getCooldownRemaining(pump);
    if(cooldownRemaining > 0) {
      LOG_PRINTLN(LOG_LEVEL_ERROR, "PUMP ERROR: Cooldown active, " + String(cooldownRemaining / 1000) + "s remaining.");
      return false;
    }
  }
  
  // Activate pump
  pump->status = PUMP_ON;
  pump->startTime = millis();
  pump->duration = durationMs;
  pump->isActive = true;
  
  // In PH_PUMP.txt
  digitalWrite(pump->relayPin, LOW);
  LOG_PRINTLN(LOG_LEVEL_INFO, "PUMP ON: " + getPumpName(pumpType) + " activated for " + String(durationMs / 1000) + "s.");
  
  return true;
}

// Function untuk stop pompa manual
bool stopPump(PumpType pumpType) {
  PumpControl* pump = getPumpControl(pumpType);
  if(pump == nullptr) return false;
  
  if(pump->status == PUMP_ON) {
    // Stop pump
    updateAndSaveRunTime(pump);
    digitalWrite(pump->relayPin, HIGH);
    pump->status = PUMP_COOLDOWN;
    pump->cooldownStart = millis();
    pump->isActive = false;

    LOG_PRINTLN(LOG_LEVEL_INFO, "PUMP OFF: " + getPumpName(pumpType) + " stopped manually.");
    return true;
  }
  
  return false;
}

// Emergency stop semua pompa
void emergencyStopAllPumps() {
  LOG_PRINTLN(LOG_LEVEL_ERROR, "!!! EMERGENCY: ALL PUMPS STOPPED !!!"); 
  
  for(int i = 0; i < 3; i++) {
    digitalWrite(pumps[i].relayPin, HIGH);
    pumps[i].status = PUMP_ERROR;
    pumps[i].isActive = false;
  }
}

void clearPumpErrors() {
  for (int i = 0; i < 3; i++) {
    if (pumps[i].status == PUMP_ERROR) {
      pumps[i].status = PUMP_OFF;
    }
  }
  lockoutUntil = millis() + LOCKOUT_DURATION; // AKTIFKAN MASA TENANG
  LOG_PRINTLN(LOG_LEVEL_INFO, "SYSTEM: Pump errors reset. System locked for 5 mins.");
}

// Update status semua pompa
void updatePumpStatus() {
  for(int i = 0; i < 3; i++) {
    PumpControl* pump = &pumps[i];
// Check timeout untuk pompa yang aktif
    if(pump->status == PUMP_ON && pump->isActive) {
      if(millis() - pump->startTime >= pump->duration) {
        // Timeout - stop pump
        updateAndSaveRunTime(pump);
        digitalWrite(pump->relayPin, HIGH);
        pump->status = PUMP_COOLDOWN;
        pump->cooldownStart = millis();
        pump->isActive = false;

        LOG_PRINTLN(LOG_LEVEL_INFO, "PUMP OFF: " + getPumpName(pump->type) + " timeout.");
        if (pump->type == PUMP_LIME || pump->type == PUMP_ACID) {
          LOG_PRINTLN(LOG_LEVEL_INFO, "AUTO: Activating water pump for circulation.");
          activatePump(PUMP_WATER, 15000); // Nyalakan pompa air selama 15 detik
        }
      }
    }
    
    // Check cooldown selesai
    if(pump->status == PUMP_COOLDOWN) {
      // <-- GUNAKAN VARIABEL COOLDOWN GLOBAL -->
      unsigned long currentCooldownDuration;
      switch(pump->type) {
        case PUMP_LIME: currentCooldownDuration = cooldownLime; break;
        case PUMP_ACID: currentCooldownDuration = cooldownAcid; break;
        case PUMP_WATER: currentCooldownDuration = cooldownWater; break;
        default: currentCooldownDuration = 60000; // Fallback
      }
      
      if(millis() - pump->cooldownStart >= currentCooldownDuration) {
        pump->status = PUMP_OFF;
        LOG_PRINTLN(LOG_LEVEL_DEBUG, "PUMP INFO: " + getPumpName(pump->type) + " cooldown finished.");
      }
    }
  }
}

// Safety checks
bool isSafeToActivate(PumpType pumpType) {
  float currentPH = getCurrentAvgPH();
  
  // Check pH emergency levels
  if(currentPH <= EMERGENCY_SHUTDOWN_PH || currentPH >= EMERGENCY_SHUTDOWN_PH_HIGH) {
    return false;
  }
  
  // Check konflik pompa (jangan aktifkan lime dan acid bersamaan)
  if(pumpType == PUMP_LIME && isPumpActive(PUMP_ACID)) {
    return false;
  }
  
  if(pumpType == PUMP_ACID && isPumpActive(PUMP_LIME)) {
    return false;
  }
  
  return true;
}

// Helper functions
PumpControl* getPumpControl(PumpType pumpType) {
  for(int i = 0; i < 3; i++) {
    if(pumps[i].type == pumpType) {
      return &pumps[i];
    }
  }
  return nullptr;
}

String getPumpName(PumpType pumpType) {
  switch(pumpType) {
    case PUMP_LIME: return "LIME";
    case PUMP_ACID: return "ACID";
    case PUMP_WATER: return "WATER";
    default: return "UNKNOWN";
  }
}

bool isPumpActive(PumpType pumpType) {
  PumpControl* pump = getPumpControl(pumpType);
  return (pump != nullptr && pump->isActive);
}

unsigned long getCooldownRemaining(PumpControl* pump) {
  if(pump->status != PUMP_COOLDOWN) return 0;
  
  // <-- GUNAKAN VARIABEL COOLDOWN GLOBAL -->
  unsigned long currentCooldownDuration;
  switch(pump->type) {
    case PUMP_LIME: currentCooldownDuration = cooldownLime; break;
    case PUMP_ACID: currentCooldownDuration = cooldownAcid; break;
    case PUMP_WATER: currentCooldownDuration = cooldownWater; break;
    default: currentCooldownDuration = 60000; // Fallback
  }

  unsigned long elapsed = millis() - pump->cooldownStart;
    if(elapsed >= currentCooldownDuration) return 0;
  
  return currentCooldownDuration - elapsed;
}

PumpStatus getPumpStatus(PumpType pumpType) {
  PumpControl* pump = getPumpControl(pumpType);
  return (pump != nullptr) ? pump->status : PUMP_ERROR;
}

unsigned long getTotalRunTime(PumpType pumpType) {
  unsigned long totalRunTime = 0;
  uint16_t address;

  if (pumpType == PUMP_LIME) {
    address = ADDR_LIME_RUNTIME;
  } else if (pumpType == PUMP_ACID) {
    address = ADDR_ACID_RUNTIME;
  } else if (pumpType == PUMP_WATER) {
    address = ADDR_WATER_RUNTIME;
  } else {
    return 0; // Kembalikan 0 jika tipe tidak valid
  }

  // Baca nilai total runtime dari alamat yang sesuai
  EEPROM.get(address, totalRunTime);
  return totalRunTime;
}