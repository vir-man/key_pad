#include "AuthenticationManager.h"
#include "DoorController.h"
#include "RTCHandler.h"

AuthenticationManager::AuthenticationManager(UserManager* userMgr, HolidayManager* holidayMgr) 
  : userManager(userMgr), holidayManager(holidayMgr), doorController(nullptr), rtcHandler(nullptr),
    first_user_verified(false), user_bio_auth_fail_count(0),
    sms_master_verified(false), sms_master_verified_time(0) {
}

bool AuthenticationManager::initialize(DoorController* doorCtrl, RTCHandler* rtc) {
  doorController = doorCtrl;
  rtcHandler = rtc;
  resetAuthState();
  return true;
}

void AuthenticationManager::resetAuthState() {
  first_user_verified = false;
  user_bio_auth_fail_count = 0;
  sms_master_verified = false;
  sms_master_verified_time = 0;
}

bool AuthenticationManager::checkTimeSlotAndHoliday(uint8_t user_id) {
  if (userManager == nullptr || holidayManager == nullptr || rtcHandler == nullptr) {
    return false;
  }
  
  // First check if today is a holiday
  uint8_t date, month, year;
  rtcHandler->getDate(&date, &month, &year);
  if (holidayManager->isHoliday(date, month, year)) {
    return false;  // Holiday - no access
  }
  
  // Then check time slot
  uint8_t hour, minute;
  rtcHandler->getTime(&hour, &minute);
  return userManager->isAccessAllowed(user_id, hour, minute);
}

ErrorCode AuthenticationManager::verifyMasterPassword(const char* password, uint8_t password_len) {
  if (userManager == nullptr || password == nullptr) {
    return ErrorCode::SYSTEM_NOT_INITIALIZED;
  }
  
  if (userManager->validatePassword(SystemConfig::MASTER_USER_ID, password, password_len)) {
    first_user_verified = true;
    user_bio_auth_fail_count = 0;
    return ErrorCode::SUCCESS;
  }
  
  return ErrorCode::AUTH_INVALID_PASSWORD;
}

ErrorCode AuthenticationManager::verifyUserPassword(uint8_t user_id, const char* password, uint8_t password_len) {
  if (userManager == nullptr || password == nullptr) {
    return ErrorCode::SYSTEM_NOT_INITIALIZED;
  }
  
  if (!first_user_verified) {
    return ErrorCode::AUTH_MASTER_NOT_VERIFIED;
  }
  
  if (!userManager->validatePassword(user_id, password, password_len)) {
    incrementFailureCount();
    if (user_bio_auth_fail_count >= 1) {
      resetAuthState();
      return ErrorCode::AUTH_INVALID_PASSWORD;
    }
    return ErrorCode::AUTH_INVALID_PASSWORD;
  }
  
  // Check time slot and holidays
  if (!checkTimeSlotAndHoliday(user_id)) {
    resetAuthState();
    return ErrorCode::DOOR_ACCESS_DENIED_HOLIDAY;
  }
  
  // Success - reset state
  resetAuthState();
  return ErrorCode::SUCCESS;
}

ErrorCode AuthenticationManager::verifyDualPassword(const char* password, uint8_t password_len, uint8_t* authenticated_user_id) {
  if (userManager == nullptr || password == nullptr || authenticated_user_id == nullptr) {
    return ErrorCode::SYSTEM_NOT_INITIALIZED;
  }
  
  // Parse user ID from password (format: userID + password, e.g., "21234" = user 2, password 1234)
  uint8_t user_id = 0;
  uint8_t user_id_length = 0;
  
  // Extract user ID (first 1-2 digits)
  for (uint8_t i = 0; i < password_len && i < 2; i++) {
    if (password[i] >= '0' && password[i] <= '9') {
      user_id = user_id * 10 + (password[i] - '0');
      user_id_length++;
    } else {
      break;
    }
  }
  
  if (user_id < 1 || user_id > SystemConfig::MAX_NUM_OF_USERS) {
    return ErrorCode::AUTH_INVALID_USER_ID;
  }
  
  // Extract actual password (after user ID)
  const char* actual_password = password + user_id_length;
  uint8_t actual_password_len = password_len - user_id_length;
  
  if (actual_password_len < SystemConfig::MIN_PASSWORD_LEN) {
    return ErrorCode::USER_PASSWORD_TOO_SHORT;
  }
  
  // If master (user_id == 1) and not verified yet
  if (user_id == SystemConfig::MASTER_USER_ID && !first_user_verified) {
    ErrorCode err = verifyMasterPassword(actual_password, actual_password_len);
    if (err == ErrorCode::SUCCESS) {
      *authenticated_user_id = user_id;
      return ErrorCode::SUCCESS;  // Master verified, waiting for user
    }
    return err;
  }
  
  // If master verified and user enters master password again, open master menu
  if (first_user_verified && user_id == SystemConfig::MASTER_USER_ID) {
    if (userManager->validatePassword(SystemConfig::MASTER_USER_ID, actual_password, actual_password_len)) {
      *authenticated_user_id = user_id;
      resetAuthState();
      return ErrorCode::SUCCESS;  // Master menu access
    }
  }
  
  // Verify user password
  ErrorCode err = verifyUserPassword(user_id, actual_password, actual_password_len);
  if (err == ErrorCode::SUCCESS) {
    *authenticated_user_id = user_id;
  }
  
  return err;
}

ErrorCode AuthenticationManager::verifyMasterFingerprint(int8_t fingerprint_id) {
  if (fingerprint_id != SystemConfig::MASTER_USER_ID) {
    return ErrorCode::AUTH_FINGERPRINT_NOT_FOUND;
  }
  
  first_user_verified = true;
  user_bio_auth_fail_count = 0;
  return ErrorCode::SUCCESS;
}

ErrorCode AuthenticationManager::verifyUserFingerprint(int8_t fingerprint_id, uint8_t* authenticated_user_id) {
  if (authenticated_user_id == nullptr) {
    return ErrorCode::SYSTEM_NOT_INITIALIZED;
  }
  
  if (!first_user_verified) {
    return ErrorCode::AUTH_MASTER_NOT_VERIFIED;
  }
  
  if (fingerprint_id < 1 || fingerprint_id > SystemConfig::MAX_NUM_OF_USERS) {
    incrementFailureCount();
    if (user_bio_auth_fail_count >= 2) {
      resetAuthState();
      return ErrorCode::AUTH_FINGERPRINT_NOT_FOUND;
    }
    return ErrorCode::AUTH_FINGERPRINT_NOT_FOUND;
  }
  
  // Check time slot and holidays
  if (!checkTimeSlotAndHoliday(fingerprint_id)) {
    resetAuthState();
    return ErrorCode::DOOR_ACCESS_DENIED_HOLIDAY;
  }
  
  *authenticated_user_id = fingerprint_id;
  resetAuthState();
  return ErrorCode::SUCCESS;
}

ErrorCode AuthenticationManager::smsVerifyMaster(uint8_t user_id, const char* password, uint8_t password_len) {
  if (userManager == nullptr || password == nullptr) {
    return ErrorCode::SYSTEM_NOT_INITIALIZED;
  }
  
  if (user_id != SystemConfig::MASTER_USER_ID) {
    return ErrorCode::AUTH_INVALID_USER_ID;
  }
  
  if (userManager->validatePassword(SystemConfig::MASTER_USER_ID, password, password_len)) {
    sms_master_verified = true;
    sms_master_verified_time = millis();
    return ErrorCode::SUCCESS;
  }
  
  sms_master_verified = false;
  return ErrorCode::AUTH_INVALID_PASSWORD;
}

ErrorCode AuthenticationManager::smsVerifyUser(uint8_t user_id, const char* password, uint8_t password_len) {
  if (userManager == nullptr || password == nullptr) {
    return ErrorCode::SYSTEM_NOT_INITIALIZED;
  }
  
  // Check if master was verified first
  checkSMSMasterTimeout();
  if (!sms_master_verified) {
    return ErrorCode::AUTH_MASTER_NOT_VERIFIED;
  }
  
  if (user_id == SystemConfig::MASTER_USER_ID) {
    return ErrorCode::AUTH_INVALID_USER_ID;  // Cannot use master as user in step 2
  }
  
  if (!userManager->validatePassword(user_id, password, password_len)) {
    sms_master_verified = false;  // Reset on failure
    return ErrorCode::AUTH_INVALID_PASSWORD;
  }
  
  // Check time slot and holidays
  if (!checkTimeSlotAndHoliday(user_id)) {
    sms_master_verified = false;
    return ErrorCode::DOOR_ACCESS_DENIED_HOLIDAY;
  }
  
  // Success - reset master verification
  sms_master_verified = false;
  return ErrorCode::SUCCESS;
}

bool AuthenticationManager::isMasterVerified() {
  return first_user_verified;
}

bool AuthenticationManager::isFirstUserVerified() {
  return first_user_verified;
}

void AuthenticationManager::setFirstUserVerified(bool state) {
  first_user_verified = state;
  if (state) {
    user_bio_auth_fail_count = 0;
  }
}

void AuthenticationManager::resetFailureCount() {
  user_bio_auth_fail_count = 0;
}

uint8_t AuthenticationManager::getFailureCount() {
  return user_bio_auth_fail_count;
}

void AuthenticationManager::incrementFailureCount() {
  user_bio_auth_fail_count++;
}

bool AuthenticationManager::checkAccessAllowed(uint8_t user_id) {
  return checkTimeSlotAndHoliday(user_id);
}

void AuthenticationManager::checkSMSMasterTimeout() {
  if (sms_master_verified && 
      (millis() - sms_master_verified_time > SystemConfig::SMS_MASTER_VERIFY_TIMEOUT)) {
    sms_master_verified = false;
  }
}

