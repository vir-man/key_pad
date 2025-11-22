#include "MainSystem.h"
#include <HardwareSerial.h>

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
  
  // Initialize Fingerprint Manager
  fingerprintManager = new FingerprintManager(&Serial3);
  if (!fingerprintManager->initialize()) {
    // Non-critical - continue
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
  
  // Initialize UI State Machine (to be created)
  // uiStateMachine = new UIStateMachine(...);
  // if (!uiStateMachine->initialize()) {
  //   return false;
  // }
  
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
  
  // UI State Machine task (to be implemented)
  // if (uiStateMachine != nullptr) {
  //   uiStateMachine->task();
  // }
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

