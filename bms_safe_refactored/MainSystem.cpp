#include "MainSystem.h"
#include "UIStateMachine.h"
#include <HardwareSerial.h>
#include <LiquidCrystal.h>
#include <Adafruit_Keypad.h>

// LCD instance (static to persist for program lifetime)
static LiquidCrystal* lcdInstance = nullptr;

// Keypad instance (static to persist for program lifetime)
static Adafruit_Keypad* keypadInstance = nullptr;

MainSystem::MainSystem() 
  : eepromStorage(nullptr), userManager(nullptr), holidayManager(nullptr),
    authManager(nullptr), fingerprintManager(nullptr), doorController(nullptr),
    gsmHandler(nullptr), smsParser(nullptr), rtcHandler(nullptr),
    tempMonitor(nullptr), alarmManager(nullptr), buzzerController(nullptr),
    batteryMonitor(nullptr), uiStateMachine(nullptr), initialized(false) {
}

MainSystem::~MainSystem() {
  // Cleanup if needed
}

bool MainSystem::initialize() {
  // Initialize EEPROM Storage
  eepromStorage = new EEPROMStorage();
  if (!eepromStorage->initialize()) {
    return false;
  }
  
  // Initialize User Manager
  userManager = new UserManager(eepromStorage);
  if (!userManager->initialize()) {
    return false;
  }
  
  // Initialize Holiday Manager
  holidayManager = new HolidayManager(eepromStorage);
  if (!holidayManager->initialize()) {
    return false;
  }
  
  // Initialize RTC Handler
  rtcHandler = new RTCHandler();
  if (!rtcHandler->initialize()) {
    return false;
  }
  
  // Initialize Door Controller
  doorController = new DoorController(eepromStorage);
  if (!doorController->initialize()) {
    return false;
  }
  
  // Initialize Authentication Manager
  authManager = new AuthenticationManager(userManager, holidayManager);
  if (!authManager->initialize(doorController, rtcHandler)) {
    return false;
  }
  
  // Initialize LCD (must be before FingerprintManager to set LCD pointer)
  if (lcdInstance == nullptr) {
    lcdInstance = new LiquidCrystal(SystemConfig::LCD_RS_PIN, SystemConfig::LCD_EN_PIN,
                                    SystemConfig::LCD_D4_PIN, SystemConfig::LCD_D5_PIN,
                                    SystemConfig::LCD_D6_PIN, SystemConfig::LCD_D7_PIN);
  }
  
  // Initialize Fingerprint Manager
  fingerprintManager = new FingerprintManager(&Serial3);
  if (!fingerprintManager->initialize()) {
    // Non-critical - continue
  }
  
  // Set LCD pointer for FingerprintManager (for enrollment display)
  if (lcdInstance != nullptr && fingerprintManager != nullptr) {
    fingerprintManager->setLCD(lcdInstance);
  }
  
  // Initialize Temperature Monitor
  tempMonitor = new TemperatureMonitor();
  if (!tempMonitor->initialize()) {
    // Non-critical - continue
  }
  
  // Initialize Buzzer Controller
  buzzerController = new BuzzerController();
  uint16_t buzzer_timeout = eepromStorage->readBuzzerTimeout();
  if (!buzzerController->initialize(buzzer_timeout)) {
    // Non-critical - continue
  }
  
  // Initialize Battery Monitor
  batteryMonitor = new BatteryMonitor();
  if (!batteryMonitor->initialize()) {
    // Non-critical - continue
  }
  
  // Initialize GSM Handler
  gsmHandler = new GSMHandler(&Serial1, userManager);
  if (!gsmHandler->initialize()) {
    // Non-critical - continue
  }
  
  // Initialize SMS Command Parser
  smsParser = new SMSCommandParser(&Serial1, userManager, authManager, 
                                    doorController, holidayManager, gsmHandler);
  if (!smsParser->initialize()) {
    // Non-critical - continue
  }
  
  // Initialize Alarm Manager
  alarmManager = new AlarmManager(tempMonitor, gsmHandler, buzzerController, userManager);
  if (!alarmManager->initialize()) {
    // Non-critical - continue
  }
  
  // Initialize Keypad (only once)
  if (keypadInstance == nullptr) {
    Serial.println(F("[MainSystem] Creating keypad instance..."));
    static char keys[4][4] = {
      {'1', '2', '3', 'x'},
      {'4', '5', '6', '^'},
      {'7', '8', '9', '&'},
      {'@', '0', '#', '*'}
    };
    static byte rowPins[4] = {
      SystemConfig::KEYPAD_ROW_0,
      SystemConfig::KEYPAD_ROW_1,
      SystemConfig::KEYPAD_ROW_2,
      SystemConfig::KEYPAD_ROW_3
    };
    static byte colPins[4] = {
      SystemConfig::KEYPAD_COL_0,
      SystemConfig::KEYPAD_COL_1,
      SystemConfig::KEYPAD_COL_2,
      SystemConfig::KEYPAD_COL_3
    };
    Serial.print(F("[MainSystem] Row pins: "));
    for (int i = 0; i < 4; i++) {
      Serial.print(rowPins[i]);
      if (i < 3) Serial.print(F(", "));
    }
    Serial.println();
    Serial.print(F("[MainSystem] Col pins: "));
    for (int i = 0; i < 4; i++) {
      Serial.print(colPins[i]);
      if (i < 3) Serial.print(F(", "));
    }
    Serial.println();
    keypadInstance = new Adafruit_Keypad(makeKeymap(keys), rowPins, colPins, 4, 4);
    Serial.println(F("[MainSystem] Keypad instance created"));
  } else {
    Serial.println(F("[MainSystem] Keypad instance already exists"));
  }
  
  // Initialize UI State Machine
  uiStateMachine = new UIStateMachine(lcdInstance, keypadInstance, this);
  if (!uiStateMachine->initialize()) {
    return false;
  }
  
  // Print EEPROM stored data on startup
  Serial.println(F("\n=== EEPROM STORED DATA ==="));
  
  // Print user data
  Serial.println(F("\n--- User Data ---"));
  for (uint8_t i = 0; i < SystemConfig::MAX_USER_TO_BE_STORED; i++) {
    uint8_t user_id = i + 1;
    if (userManager->userExists(user_id)) {
      Serial.print(F("User "));
      Serial.print(user_id);
      Serial.print(F(": Mobile="));
      char mobile[SystemConfig::MOBILE_NUMBER_LENGTH + 1];
      if (userManager->getMobileNumber(user_id, mobile) == ErrorCode::SUCCESS) {
        Serial.print(mobile);
      } else {
        Serial.print(F("ERROR"));
      }
      Serial.print(F(", Password Length="));
      Serial.println(userManager->getPasswordLength(user_id));
      
      // Print time slot if configured
      if (userManager->isTimeSlotConfigured(user_id)) {
        // Time slot details would need getter methods
        Serial.print(F("  Time Slot: Configured"));
        Serial.println();
      }
    }
  }
  
  // Print door open count
  Serial.print(F("\n--- Door Open Count: "));
  Serial.print(doorController->getDoorOpenCount());
  Serial.println(F(" ---"));
  
  // Print buzzer timeout
  Serial.print(F("--- Buzzer Timeout: "));
  Serial.print(eepromStorage->readBuzzerTimeout());
  Serial.println(F(" minutes ---"));
  
  // Print holiday count
  Serial.print(F("--- Holiday Count: "));
  Serial.print(holidayManager->getHolidayCount());
  Serial.println(F(" ---"));
  
  // Print holidays
  uint8_t holiday_count = holidayManager->getHolidayCount();
  if (holiday_count > 0) {
    Serial.println(F("--- Holidays ---"));
    for (uint8_t i = 0; i < holiday_count; i++) {
      uint8_t date, month, year;
      if (holidayManager->getHoliday(i, &date, &month, &year) == ErrorCode::SUCCESS) {
        Serial.print(F("  Holiday "));
        Serial.print(i + 1);
        Serial.print(F(": "));
        if (date < 10) Serial.print('0');
        Serial.print(date);
        Serial.print(F("/"));
        if (month < 10) Serial.print('0');
        Serial.print(month);
        Serial.print(F("/"));
        if (year < 10) Serial.print('0');
        Serial.println(year);
      }
    }
  }
  
  Serial.println(F("=== END EEPROM DATA ===\n"));
  
  initialized = true;
  return true;
}

bool MainSystem::isInitialized() {
  return initialized;
}

void MainSystem::task() {
  if (!initialized) {
    return;
  }
  
  // CRITICAL: Process keypad input FIRST (like original gpio_task() which calls tick() first)
  // This ensures immediate keypad response, matching the original behavior
  if (uiStateMachine != nullptr) {
    // Only call handleKeypadInput() directly - don't call full task() yet
    // The keypad needs to be processed immediately, before any delays from other tasks
    uiStateMachine->handleKeypadInput();
  }
  
  // Now process other subsystems
  if (rtcHandler != nullptr) {
    rtcHandler->task();
  }
  
  if (doorController != nullptr) {
    doorController->task();
  }
  
  if (tempMonitor != nullptr) {
    tempMonitor->task();
  }
  
  if (alarmManager != nullptr) {
    alarmManager->task();
  }
  
  if (buzzerController != nullptr) {
    buzzerController->task();
  }
  
  if (batteryMonitor != nullptr) {
    batteryMonitor->task();
  }
  
  if (gsmHandler != nullptr) {
    gsmHandler->task();
    gsmHandler->housekeepingTask();
  }
  
  // SMS Parser task (now non-blocking - only runs when data is available)
  if (smsParser != nullptr) {
    smsParser->task();
  }
  
  if (authManager != nullptr) {
    authManager->checkSMSMasterTimeout();
  }
  
  // UI State Machine task (now handles display updates, etc.)
  // Keypad input was already processed above for immediate response
  if (uiStateMachine != nullptr) {
    uiStateMachine->task();
  }
}

EEPROMStorage* MainSystem::getEEPROMStorage() {
  return eepromStorage;
}

UserManager* MainSystem::getUserManager() {
  return userManager;
}

HolidayManager* MainSystem::getHolidayManager() {
  return holidayManager;
}

AuthenticationManager* MainSystem::getAuthManager() {
  return authManager;
}

FingerprintManager* MainSystem::getFingerprintManager() {
  return fingerprintManager;
}

DoorController* MainSystem::getDoorController() {
  return doorController;
}

GSMHandler* MainSystem::getGSMHandler() {
  return gsmHandler;
}

SMSCommandParser* MainSystem::getSMSParser() {
  return smsParser;
}

RTCHandler* MainSystem::getRTCHandler() {
  return rtcHandler;
}

TemperatureMonitor* MainSystem::getTempMonitor() {
  return tempMonitor;
}

AlarmManager* MainSystem::getAlarmManager() {
  return alarmManager;
}

BuzzerController* MainSystem::getBuzzerController() {
  return buzzerController;
}

BatteryMonitor* MainSystem::getBatteryMonitor() {
  return batteryMonitor;
}

UIStateMachine* MainSystem::getUIStateMachine() {
  return uiStateMachine;
}

