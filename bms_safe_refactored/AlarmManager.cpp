#include "AlarmManager.h"
#include <string.h>
#include <stdlib.h>

AlarmManager::AlarmManager(TemperatureMonitor* temp, GSMHandler* gsm, BuzzerController* buz, UserManager* userMgr)
  : tempMonitor(temp), gsmHandler(gsm), buzzer(buz), userManager(userMgr),
    temperature_alarm_triggered(false), vibration_alarm_triggered(false),
    gun_point_triggered(false), otp_length(0), otp_matched(false),
    gpa_state(GunPointState::DO_NOTHING), call_start_time(0) {
  memset(generated_otp, 0, sizeof(generated_otp));
  memset(char_generated_otp, 0, sizeof(char_generated_otp));
  memset(input_otp, 0, sizeof(input_otp));
}

bool AlarmManager::initialize() {
  if (tempMonitor == nullptr || gsmHandler == nullptr || buzzer == nullptr || userManager == nullptr) {
    return false;
  }
  
  resetOTP();
  return true;
}

void AlarmManager::generateRandomOTP() {
  // Generate based on current time (simplified)
  uint16_t seed = (millis() % 1000);
  randomSeed(seed);
  
  for (uint8_t i = 0; i < 6; i++) {
    generated_otp[i] = random(0, 9);
    char_generated_otp[i] = generated_otp[i] + '0';
  }
  char_generated_otp[6] = '\0';
}

void AlarmManager::generateOTP() {
  generateRandomOTP();
}

void AlarmManager::resetOTP() {
  otp_length = 0;
  otp_matched = false;
  memset(input_otp, 0, sizeof(input_otp));
}

ErrorCode AlarmManager::inputOTPDigit(uint8_t digit) {
  if (otp_length >= 6) {
    return ErrorCode::SMS_INVALID_PARAMETERS;
  }
  
  if (digit > 9) {
    return ErrorCode::SMS_INVALID_PARAMETERS;
  }
  
  input_otp[otp_length++] = digit;
  
  // Check if complete
  if (otp_length == 6) {
    // Check against generated OTP or master OTP
    bool match = true;
    for (uint8_t i = 0; i < 6; i++) {
      if (input_otp[i] != generated_otp[i] && input_otp[i] != SystemConfig::MASTER_OTP[i]) {
        match = false;
        break;
      }
    }
    
    if (match) {
      otp_matched = true;
      deactivateAlarms();
      return ErrorCode::SUCCESS;
    } else {
      resetOTP();
      return ErrorCode::AUTH_INVALID_PASSWORD;
    }
  }
  
  return ErrorCode::SUCCESS;
}

bool AlarmManager::isOTPMatched() {
  return otp_matched;
}

void AlarmManager::activateAlarms() {
  // Turn on sirens
  digitalWrite(SystemConfig::SIREN_PIN_0, HIGH);
  digitalWrite(SystemConfig::SIREN_PIN_1, HIGH);
  
  // Turn on buzzer
  if (buzzer != nullptr) {
    buzzer->turnOn();
  }
}

void AlarmManager::deactivateAlarms() {
  // Turn off sirens
  digitalWrite(SystemConfig::SIREN_PIN_0, LOW);
  digitalWrite(SystemConfig::SIREN_PIN_1, LOW);
  
  // Turn off buzzer
  if (buzzer != nullptr) {
    buzzer->turnOff();
  }
  
  // Reset alarm states
  temperature_alarm_triggered = false;
  vibration_alarm_triggered = false;
  gun_point_triggered = false;
  resetOTP();
}

void AlarmManager::sendAlarmMessages(uint8_t alarm_type) {
  if (gsmHandler == nullptr || userManager == nullptr) {
    return;
  }
  
  // Send to all configured users
  for (uint8_t i = 0; i < SystemConfig::MAX_USER_TO_BE_STORED; i++) {
    if (userManager->isPasswordConfigured(i + 1)) {
      gsmHandler->addToQueue(alarm_type, i + 1);
    }
  }
}

void AlarmManager::makeAlarmCalls() {
  if (gsmHandler == nullptr || userManager == nullptr) {
    return;
  }
  
  // Make calls to all configured users
  for (uint8_t i = 0; i < SystemConfig::MAX_USER_TO_BE_STORED; i++) {
    if (userManager->isPasswordConfigured(i + 1)) {
      gsmHandler->makeCallDirect(i + 1);
      delay(2000);  // Delay between calls
    }
  }
}

void AlarmManager::checkTemperatureAlarm() {
  if (tempMonitor == nullptr) {
    return;
  }
  
  if (tempMonitor->isAlarmTriggered() && !temperature_alarm_triggered) {
    temperature_alarm_triggered = true;
    generateOTP();
    activateAlarms();
    sendAlarmMessages(SystemConfig::TEMP_ALARM_MSG);
    gpa_state = GunPointState::SEND_MESSAGE;
  }
}

void AlarmManager::triggerVibrationAlarm() {
  if (vibration_alarm_triggered) {
    return;  // Already triggered
  }
  
  vibration_alarm_triggered = true;
  generateOTP();
  activateAlarms();
  sendAlarmMessages(SystemConfig::VIBRATION_ALARM_MSG);
  gpa_state = GunPointState::SEND_MESSAGE;
}

void AlarmManager::triggerGunPoint() {
  if (gun_point_triggered) {
    return;  // Already triggered
  }
  
  gun_point_triggered = true;
  generateOTP();
  activateAlarms();
  sendAlarmMessages(SystemConfig::GUN_POINT_MSG);
  gpa_state = GunPointState::SEND_MESSAGE;
}

void AlarmManager::task() {
  // Check temperature alarm
  checkTemperatureAlarm();
  
  // Handle gun point state machine
  switch(gpa_state) {
    case GunPointState::SEND_MESSAGE:
      if (gsmHandler->getQueueSize() < 1) {
        gpa_state = GunPointState::CALL;
        call_start_time = millis();  // Initialize timer when entering CALL state
      }
      break;
      
    case GunPointState::CALL:
      if (millis() - call_start_time > 20000) {
        makeAlarmCalls();
        call_start_time = millis();
        gpa_state = GunPointState::DO_NOTHING;
      }
      break;
      
    case GunPointState::DO_NOTHING:
      break;
  }
}

bool AlarmManager::isAlarmActive() {
  return temperature_alarm_triggered || vibration_alarm_triggered || gun_point_triggered;
}

bool AlarmManager::isTemperatureAlarm() {
  return temperature_alarm_triggered;
}

bool AlarmManager::isVibrationAlarm() {
  return vibration_alarm_triggered;
}

bool AlarmManager::isGunPointActive() {
  return gun_point_triggered;
}

