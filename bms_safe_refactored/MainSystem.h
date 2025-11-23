#ifndef MAIN_SYSTEM_H
#define MAIN_SYSTEM_H

#include "SystemConfig.h"
#include "EEPROMStorage.h"
#include "UserManager.h"
#include "HolidayManager.h"
#include "AuthenticationManager.h"
#include "FingerprintManager.h"
#include "DoorController.h"
#include "GSMHandler.h"
#include "SMSCommandParser.h"
#include "RTCHandler.h"
#include "TemperatureMonitor.h"
#include "AlarmManager.h"
#include "BuzzerController.h"
#include "BatteryMonitor.h"

// Forward declaration
class UIStateMachine;

/**
 * Main System Coordinator Class
 * Initializes and coordinates all subsystems
 */
class MainSystem {
private:
  // Core components
  EEPROMStorage* eepromStorage;
  UserManager* userManager;
  HolidayManager* holidayManager;
  AuthenticationManager* authManager;
  FingerprintManager* fingerprintManager;
  DoorController* doorController;
  GSMHandler* gsmHandler;
  SMSCommandParser* smsParser;
  RTCHandler* rtcHandler;
  TemperatureMonitor* tempMonitor;
  AlarmManager* alarmManager;
  BuzzerController* buzzerController;
  BatteryMonitor* batteryMonitor;
  UIStateMachine* uiStateMachine;
  
  bool initialized;
  
public:
  MainSystem();
  ~MainSystem();
  
  // Initialization
  bool initialize();
  bool isInitialized();
  
  // Task functions (call in loop)
  void task();
  
  // Getters for subsystems
  EEPROMStorage* getEEPROMStorage();
  UserManager* getUserManager();
  HolidayManager* getHolidayManager();
  AuthenticationManager* getAuthManager();
  FingerprintManager* getFingerprintManager();
  DoorController* getDoorController();
  GSMHandler* getGSMHandler();
  SMSCommandParser* getSMSParser();
  RTCHandler* getRTCHandler();
  TemperatureMonitor* getTempMonitor();
  AlarmManager* getAlarmManager();
  BuzzerController* getBuzzerController();
  BatteryMonitor* getBatteryMonitor();
  UIStateMachine* getUIStateMachine();
};

#endif // MAIN_SYSTEM_H

