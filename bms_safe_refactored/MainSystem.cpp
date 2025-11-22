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
  
  // Call task functions for all subsystems
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
  
  if (smsParser != nullptr) {
    smsParser->task();
  }
  
  if (authManager != nullptr) {
    authManager->checkSMSMasterTimeout();
  }
  
  // UI State Machine task
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

