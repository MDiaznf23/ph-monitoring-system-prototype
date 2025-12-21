#include <LiquidCrystal_I2C.h>
#include "EEPROM.h"
#include "esp_task_wdt.h"

// Definisikan Level Logging
#define LOG_LEVEL_NONE    0 // Tidak ada log sama sekali
#define LOG_LEVEL_ERROR   1 // Hanya error kritis dan safety
#define LOG_LEVEL_INFO    2 // Info penting (perubahan mode, aksi pompa)
#define LOG_LEVEL_DEBUG   3 // Semua informasi, termasuk data sensor mentah

// Atur level logging yang aktif di sini
#define ACTIVE_LOG_LEVEL LOG_LEVEL_DEBUG

#if (ACTIVE_LOG_LEVEL > 0)
  #define LOG_PRINT(level, ...) if(level <= ACTIVE_LOG_LEVEL) { Serial.print(__VA_ARGS__); }
  #define LOG_PRINTLN(level, ...) if(level <= ACTIVE_LOG_LEVEL) { Serial.println(__VA_ARGS__); }
#else
  #define LOG_PRINT(level, ...) // Jika tidak aktif, kode ini akan kosong
  #define LOG_PRINTLN(level, ...)
#endif

LiquidCrystal_I2C lcd(0x27,20,4);  // set the LCD address 

// EEPROM (MEMORY)
#define EEPROM_SIZE 512
#define ADDR_TARGET_PH 0
#define ADDR_COOLDOWN_LIME 8   
#define ADDR_COOLDOWN_ACID 12  
#define ADDR_COOLDOWN_WATER 16 
#define ADDR_PUMP_DUR_ACID 20 
#define ADDR_PUMP_DUR_LIME 24
#define ADDR_WATER_RUNTIME 28   
#define ADDR_SETTINGS_VALID 32
#define ADDR_LIME_RUNTIME 36 
#define ADDR_ACID_RUNTIME 40 // Alamat untuk total runtime pompa asam (unsigned long = 4 bytes)
#define ADDR_CALIBRATION_OFFSET 44 

// Initialize pin for sensors
#define PH_PIN 35
#define HUMIDITY_PIN 34 

#define RELAY_LIME_PUMP 13      // Relay untuk pompa kapur/dolomit
#define RELAY_ACID_PUMP 14      // Relay untuk pompa asam
#define RELAY_WATER_PUMP 15     // Pin baru untuk pompa air

// Variabel untuk progress bar
int samplingProgress = 0;
unsigned long lastSafetyCheck = 0;

// Pump types
enum PumpType {
  PUMP_LIME,      // Pompa larutan kapur (pH naik)
  PUMP_ACID,       // Pompa larutan asam (pH turun)
  PUMP_WATER      // Pompa air baru
};

// Pump status
enum PumpStatus {
  PUMP_OFF,
  PUMP_ON,
  PUMP_COOLDOWN,
  PUMP_ERROR
};

// Pump control structure
struct PumpControl {
  PumpType type;
  int relayPin;
  PumpStatus status;
  unsigned long startTime;
  unsigned long duration;
  unsigned long cooldownStart;
  bool isActive;
};

// Global pump controls
PumpControl pumps[3] = {
  {PUMP_LIME, RELAY_LIME_PUMP, PUMP_OFF, 0, 0, 0, false},
  {PUMP_ACID, RELAY_ACID_PUMP, PUMP_OFF, 0, 0, 0, false},
  {PUMP_WATER, RELAY_WATER_PUMP, PUMP_OFF, 0, 0, 0, false}
};

// Safety settings
#define MAX_PUMP_DURATION 30000     // Max 30 detik per aktivasi
#define MINIMUM_HUMIDITY 30.0
#define EMERGENCY_SHUTDOWN_PH 3.0   // pH darurat shutdown
#define EMERGENCY_SHUTDOWN_PH_HIGH 9.0

// Initialize button for interface
#define BTN_MODE 19       // Button untuk navigasi atas/bawah atau ganti mode
#define BTN_SELECT 18     // Button untuk select/confirm atau navigasi kiri/kanan
#define BTN_UP 17      // Tombol Atas
#define BTN_DOWN 16    // Tombol Bawah

// Di bagian variabel global
unsigned long lockoutUntil = 0;
const unsigned long LOCKOUT_DURATION = 300000; // 5 menit (300,000 ms)

// System modes
enum SystemMode {
  MODE_AUTO,
  MODE_CALIBRATION,
  MODE_SETTINGS,
  MODE_COUNT
};

// Display states untuk navigasi
enum DisplayState {
  DISPLAY_MAIN,
  DISPLAY_MODE_CONFIRM,     // Konfirmasi ganti mode
  DISPLAY_MODE_SELECT,
  DISPLAY_MENU,             // Menu navigasi
  DISPLAY_VALUE_ADJUST,     // Adjust nilai dengan panah
  DISPLAY_YES_NO_CONFIRM, 
  DISPLAY_RUNTIME_INFO,    // Konfirmasi Yes/No
  DISPLAY_PUMP_TESTING,
};

// Menu items untuk navigasi
enum MenuItem {
  MENU_PH_TARGET,
  MENU_PUMP_DUR_LIME,   
  MENU_PUMP_DUR_ACID,   
  MENU_COOLDOWN_LIME,   
  MENU_COOLDOWN_ACID, 
  MENU_COOLDOWN_WATER, 
  MENU_RESET_CALIBRATION, 
  MENU_RESET_DEFAULTS, 
  MENU_MANUAL_PUMP_LIME, 
  MENU_MANUAL_PUMP_ACID, 
  MENU_MANUAL_PUMP_WATER,
  MENU_SHOW_RUNTIME,
  MENU_BACK,
  MENU_COUNT,
  MENU_RESET_RUNTIME,
};

byte wave1[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x1C, 0x1F};
byte wave2[8] = {0x00, 0x00, 0x00, 0x08, 0x1C, 0x1F, 0x1F, 0x1F};
byte wave3[8] = {0x00, 0x08, 0x1C, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F};
byte wave4[8] = {0x08, 0x1C, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F};

byte droplet[8] = {
  B00100,
  B00100,
  B01110,
  B11111,
  B11111,
  B11111,
  B01110,
  B00000
};

// Custom characters untuk panah dan indikator
byte arrowRight[8] = {
  0b00000,
  0b01000,
  0b01100,
  0b01110,
  0b01110,
  0b01100,
  0b01000,
  0b00000
};

byte arrowLeft[8] = {
  0b00000,
  0b00010,
  0b00110,
  0b01110,
  0b01110,
  0b00110,
  0b00010,
  0b00000
};

byte arrowUp[8] = {
  0b00100,
  0b01110,
  0b11111,
  0b00100,
  0b00100,
  0b00100,
  0b00100,
  0b00000
};

byte arrowDown[8] = {
  0b00100,
  0b00100,
  0b00100,
  0b00100,
  0b11111,
  0b01110,
  0b00100,
  0b00000
};

// Global variables
SystemMode currentMode = MODE_AUTO;
SystemMode pendingMode = MODE_AUTO;
DisplayState displayState = DISPLAY_MAIN;
MenuItem currentMenuItem = MENU_PH_TARGET;
int menuCursor = 0;
int valueCursor = 0;
bool confirmYes = true;  // Default Yes pada konfirmasi

// Settings variables
float targetPH = 6.4;
int pumpDurationLime = 5000;  // <--: Durasi untuk pompa kapur
int pumpDurationAcid = 5000;  // <--: Durasi untuk pompa asam
unsigned long cooldownLime = 300000;  
unsigned long cooldownAcid = 300000;  
unsigned long cooldownWater = 60000;   
float calibrationOffset = 7.7851;
unsigned long pumpTestStartTime = 0;
PumpType pumpToTest;

// Timing variables
unsigned long lastButtonPress = 0;
bool needsRedraw = true;

// Button states
bool btnModePressed = false;
bool btnSelectPressed = false;
bool btnUpPressed = false;
bool btnDownPressed = false;

enum ConfirmChoice { CHOICE_YES, CHOICE_NO };
ConfirmChoice confirmSelection = CHOICE_YES; // Default memilih "Yes"

#define DEBOUNCE_DELAY 200

// Buffer settings
#define BUFFER_SIZE 6
#define SAMPLING_INTERVAL 10000  // 10 detik dalam ms

// Global variables
float phBuffer[BUFFER_SIZE];
int bufferIndex = 0;
bool bufferReady = false;
unsigned long lastSampleTime = 0;
int pumpTestDuration = 3000; // Default 3 detik (dalam ms)
float currentAvgPH = 0.0;
float currentHumidity = 50.0; // Nilai kelembapan saat ini

hw_timer_t *sensorTimer = NULL;
volatile bool readSensorFlag = false;

// Timer interrupt handler
void IRAM_ATTR onSensorTimer() {
  readSensorFlag = true;
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);

  // Initialize EEPROM
  if (!EEPROM.begin(EEPROM_SIZE)) {
    Serial.println("Failed to initialize EEPROM");
    delay(1000);
    ESP.restart();
  }
  
  // Load settings dari EEPROM
  loadSettingsFromEEPROM();

  // ----- Konfigurasi Watchdog Timer -----
  Serial.println("Initializing Watchdog Timer...");

  esp_task_wdt_config_t wdt_config = {
      .timeout_ms = 15000,
      .idle_core_mask = (1 << portNUM_PROCESSORS) - 1,
      .trigger_panic = true
  };

  if (esp_task_wdt_init(&wdt_config) != ESP_OK) {
      Serial.println("Failed to initialize watchdog");
  } else {
      Serial.println("Watchdog initialized successfully");
      // Pendaftaran task akan dilakukan di dalam loop()
  }

  // Initialize pin button
  pinMode(BTN_MODE, INPUT_PULLUP);
  pinMode(BTN_SELECT, INPUT_PULLUP);
  pinMode(BTN_UP, INPUT_PULLUP);      
  pinMode(BTN_DOWN, INPUT_PULLUP);    

  // Initialize Component
  initializeRelays();
  initializeSensors();

  // Initialize LCD
  lcd.init();

  // turn on LCD backlight                      
  lcd.backlight();

  lcd.createChar(0, arrowRight);
  lcd.createChar(1, arrowLeft);
  lcd.createChar(2, arrowUp);
  lcd.createChar(3, arrowDown);
  lcd.createChar(4, wave1);
  lcd.createChar(5, wave2);
  lcd.createChar(6, wave3);
  lcd.createChar(7, wave4);

  showQuickIntro();
  delay(2000);

  // Initialize display
  updateDisplay();

  // Initialize buffer dengan nilai invalid
  for(int i = 0; i < BUFFER_SIZE; i++) {
    phBuffer[i] = -1.0;
  }

  // Setup hardware timer - 10 detik
  sensorTimer = timerBegin(1000000);  // Timer 0, prescaler 80 (1MHz)
  timerAttachInterrupt(sensorTimer, &onSensorTimer);
  timerAlarm(sensorTimer, 10000000, true, 0);

  // Serial.println("Timestamp,Event,Avg_pH,Target_pH,Humidity,System_Status,Active_Pump,Mode");
}

void loop() {
  // put your main code here, to run repeatedly:
  static bool wdt_task_added = false;
  if (!wdt_task_added) {
    if (esp_task_wdt_add(NULL) == ESP_OK) {
      Serial.println("Watchdog task added.");
      wdt_task_added = true;
    }
  }
  feedWatchdog();

  if (displayState == DISPLAY_PUMP_TESTING) {
      if (millis() - pumpTestStartTime >= pumpTestDuration) {
        // Waktu habis, matikan pompa dan kembali ke menu
        digitalWrite(getPumpControl(pumpToTest)->relayPin, HIGH);

        // Dapatkan pump control yang sesuai
        PumpControl* pump = getPumpControl(pumpToTest);
        if (pump != nullptr) {
          // Simulasikan pump->startTime agar perhitungan durasi benar
          pump->startTime = pumpTestStartTime;
          // Panggil fungsi untuk update dan simpan runtime
          updateAndSaveRunTime(pump);
        }
        // --- AKHIR PENAMBAHAN KODE ---

        displayState = DISPLAY_MENU;
        LOG_PRINTLN(LOG_LEVEL_INFO, "PUMP TEST: Finished.");
      }
      // Picu redraw setiap detik untuk update countdown
      needsRedraw = true; 
  }

  // Variabel untuk melacak waktu per detik
  static unsigned long lastSecondUpdate = 0;

  // Cek jika sudah satu detik berlalu
  if (millis() - lastSecondUpdate >= 1000) {
    samplingProgress++; // Tambah progress
    if (samplingProgress > 10) {
      // Biarkan kosong, akan di-reset oleh readSensors()
    }
    needsRedraw = true; // Picu redraw setiap detik
    lastSecondUpdate = millis();
  }

  if(readSensorFlag) {
    readSensorFlag = false;
    readSensors();
    // logDataForGraph("BACA_SENSOR", PUMP_LIME, "");
  }

  // Update pump status setiap loop
  updatePumpStatus();
  
  // Handle Interface
  handleButtons();
  
  // Update display jika perlu
  if(needsRedraw) {
    updateDisplay();
    needsRedraw = false; // Reset flag setelah layar diperbarui
  }

  // Check Safety
  if(millis() - lastSafetyCheck > 5000) {
    performSafetyCheck();
    lastSafetyCheck = millis();
  }

  // Task berdasarkan mode
  switch(currentMode) {
    case MODE_AUTO:
      // Mode otomasi - panggil fungsi monitoring
      handleAutoMode();
      break;
      
    case MODE_CALIBRATION:
      // Mode kalibrasi - panggil fungsi kalibrasi
      handleCalibrationMode();
      break;
  }
  
  delay(50);
}