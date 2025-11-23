#ifndef ALARM_MANAGER_H
#define ALARM_MANAGER_H

#include "SystemConfig.h"
#include "ErrorCodes.h"
#include "TemperatureMonitor.h"
#include "GSMHandler.h"
#include "BuzzerController.h"
#include "UserManager.h"

/**
 * Alarm Manager Class
 * Handles all alarm systems (temperature, vibration, gun point)
 * Manages OTP generation and deactivation
 */
class AlarmManager {
private:
  TemperatureMonitor* tempMonitor;
  GSMHandler* gsmHandler;
  BuzzerController* buzzer;
  UserManager* userManager;
  
  // Alarm states
  bool temperature_alarm_triggered;
  bool vibration_alarm_triggered;
  bool gun_point_triggered;
  
  // OTP system
  uint8_t generated_otp[6];
  char char_generated_otp[7];
  uint8_t input_otp[6];
  uint8_t otp_length;
  bool otp_matched;
  
  // Gun point state machine
  enum class GunPointState {
    DO_NOTHING = 0,
    SEND_MESSAGE = 1,
    CALL = 2
  };
  
  GunPointState gpa_state;
  unsigned long call_start_time;
  
  // Helper functions
  void generateRandomOTP();
  void activateAlarms();
  void deactivateAlarms();
  void sendAlarmMessages(uint8_t alarm_type);
  void makeAlarmCalls();
  
public:
  AlarmManager(TemperatureMonitor* temp, GSMHandler* gsm, BuzzerController* buz, UserManager* userMgr);
  bool initialize();
  
  // Alarm operations
  void checkTemperatureAlarm();
  void triggerVibrationAlarm();
  void triggerGunPoint();
  
  // OTP operations
  void resetOTP();
  ErrorCode inputOTPDigit(uint8_t digit);
  bool isOTPMatched();
  void generateOTP();
  
  // Task function (call in loop)
  void task();
  
  // State queries
  bool isAlarmActive();
  bool isTemperatureAlarm();
  bool isVibrationAlarm();
  bool isGunPointActive();
};

#endif // ALARM_MANAGER_H

