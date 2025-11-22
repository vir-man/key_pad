#include "UserManager.h"
#include <string.h>

UserManager::UserManager(EEPROMStorage* storage) : eeprom(storage) {
  // Initialize cache arrays
  memset(password_configured, 0, sizeof(password_configured));
  memset(mobile_cache, 0, sizeof(mobile_cache));
  memset(password_length_cache, 0, sizeof(password_length_cache));
  memset(password_cache, 0, sizeof(password_cache));
  memset(time_slot_configured, 0, sizeof(time_slot_configured));
  memset(in_time_hour, 0, sizeof(in_time_hour));
  memset(in_time_minute, 0, sizeof(in_time_minute));
  memset(out_time_hour, 0, sizeof(out_time_hour));
  memset(out_time_minute, 0, sizeof(out_time_minute));
}

bool UserManager::initialize() {
  if (eeprom == nullptr) {
    return false;
  }
  
  // Load all users from EEPROM into cache
  for (uint8_t i = 0; i < SystemConfig::MAX_USER_TO_BE_STORED; i++) {
    loadUserFromEEPROM(i);
  }
  
  // Initialize master user if not exists
  if (!password_configured[SystemConfig::MASTER_USER_INDEX]) {
    const char default_mobile[] = "0000000000";
    const char default_password[] = "1234";
    addUser(SystemConfig::MASTER_USER_ID, default_mobile, default_password, 4);
  }
  
  return true;
}

void UserManager::loadUserFromEEPROM(uint8_t user_index) {
  if (user_index >= SystemConfig::MAX_USER_TO_BE_STORED) {
    return;
  }
  
  password_configured[user_index] = eeprom->isPasswordConfigured(user_index);
  
  if (password_configured[user_index]) {
    // Load password
    uint8_t len = 0;
    if (eeprom->readUserPassword(user_index, password_cache[user_index], &len) == ErrorCode::SUCCESS) {
      password_length_cache[user_index] = len;
    }
    
    // Load mobile
    eeprom->readUserMobile(user_index, mobile_cache[user_index]);
    
    // Load time slot
    time_slot_configured[user_index] = eeprom->isTimeSlotConfigured(user_index);
    if (time_slot_configured[user_index]) {
      eeprom->readTimeSlot(user_index, &in_time_hour[user_index], &in_time_minute[user_index],
                          &out_time_hour[user_index], &out_time_minute[user_index]);
    }
  }
}

bool UserManager::validatePassword(const char* password, uint8_t len) {
  if (password == nullptr) {
    return false;
  }
  return (len >= SystemConfig::MIN_PASSWORD_LEN && len <= SystemConfig::MAX_PASSWORD_LEN);
}

bool UserManager::validateMobile(const char* mobile) {
  if (mobile == nullptr) {
    return false;
  }
  // Check if all digits
  for (uint8_t i = 0; i < SystemConfig::MOBILE_NUMBER_LENGTH; i++) {
    if (mobile[i] < '0' || mobile[i] > '9') {
      return false;
    }
  }
  return true;
}

bool UserManager::validateUserID(uint8_t user_id) {
  return (user_id >= 1 && user_id <= SystemConfig::MAX_NUM_OF_USERS);
}

ErrorCode UserManager::addUser(uint8_t user_id, const char* mobile, const char* password, uint8_t password_len) {
  if (!validateUserID(user_id)) {
    return ErrorCode::USER_INVALID_ID;
  }
  
  if (user_id == SystemConfig::MASTER_USER_ID) {
    return ErrorCode::USER_INVALID_ID;  // Cannot add master user
  }
  
  uint8_t user_index = user_id - 1;
  
  if (password_configured[user_index]) {
    return ErrorCode::USER_ALREADY_EXISTS;
  }
  
  if (!validateMobile(mobile)) {
    return ErrorCode::USER_MOBILE_INVALID;
  }
  
  if (!validatePassword(password, password_len)) {
    if (password_len < SystemConfig::MIN_PASSWORD_LEN) {
      return ErrorCode::USER_PASSWORD_TOO_SHORT;
    }
    return ErrorCode::USER_PASSWORD_TOO_LONG;
  }
  
  // Check if mobile already exists
  if (findUserByMobile(mobile) >= 0) {
    return ErrorCode::USER_ALREADY_EXISTS;
  }
  
  // Write to EEPROM
  ErrorCode err = eeprom->writeUserMobile(user_index, mobile);
  if (err != ErrorCode::SUCCESS) {
    return err;
  }
  
  err = eeprom->writeUserPassword(user_index, password, password_len);
  if (err != ErrorCode::SUCCESS) {
    return err;
  }
  
  // Update cache
  loadUserFromEEPROM(user_index);
  
  return ErrorCode::SUCCESS;
}

ErrorCode UserManager::removeUser(uint8_t user_id) {
  if (!validateUserID(user_id)) {
    return ErrorCode::USER_INVALID_ID;
  }
  
  if (user_id == SystemConfig::MASTER_USER_ID) {
    return ErrorCode::USER_INVALID_ID;  // Cannot remove master user
  }
  
  uint8_t user_index = user_id - 1;
  
  if (!password_configured[user_index]) {
    return ErrorCode::USER_NOT_FOUND;
  }
  
  // Clear from EEPROM
  ErrorCode err = eeprom->clearUserPassword(user_index);
  if (err != ErrorCode::SUCCESS) {
    return err;
  }
  
  // Clear cache
  password_configured[user_index] = false;
  memset(mobile_cache[user_index], 0, SystemConfig::MOBILE_NUMBER_LENGTH);
  password_length_cache[user_index] = 0;
  memset(password_cache[user_index], 0, SystemConfig::PASSWORD_STORE_COUNT);
  time_slot_configured[user_index] = false;
  
  return ErrorCode::SUCCESS;
}

ErrorCode UserManager::updateUserPassword(uint8_t user_id, const char* old_password, uint8_t old_len, 
                                          const char* new_password, uint8_t new_len) {
  if (!validateUserID(user_id)) {
    return ErrorCode::USER_INVALID_ID;
  }
  
  uint8_t user_index = user_id - 1;
  
  if (!password_configured[user_index]) {
    return ErrorCode::USER_NOT_FOUND;
  }
  
  // Validate old password
  if (!validatePassword(user_id, old_password, old_len)) {
    return ErrorCode::AUTH_INVALID_PASSWORD;
  }
  
  if (!validatePassword(new_password, new_len)) {
    if (new_len < SystemConfig::MIN_PASSWORD_LEN) {
      return ErrorCode::USER_PASSWORD_TOO_SHORT;
    }
    return ErrorCode::USER_PASSWORD_TOO_LONG;
  }
  
  // Write new password to EEPROM
  ErrorCode err = eeprom->writeUserPassword(user_index, new_password, new_len);
  if (err != ErrorCode::SUCCESS) {
    return err;
  }
  
  // Update cache
  password_length_cache[user_index] = new_len;
  memcpy(password_cache[user_index], new_password, new_len);
  
  return ErrorCode::SUCCESS;
}

ErrorCode UserManager::updateUserMobile(uint8_t user_id, const char* mobile) {
  if (!validateUserID(user_id)) {
    return ErrorCode::USER_INVALID_ID;
  }
  
  uint8_t user_index = user_id - 1;
  
  if (!password_configured[user_index]) {
    return ErrorCode::USER_NOT_FOUND;
  }
  
  if (!validateMobile(mobile)) {
    return ErrorCode::USER_MOBILE_INVALID;
  }
  
  // Check if mobile already exists for another user
  int8_t existing_user = findUserByMobile(mobile);
  if (existing_user >= 0 && existing_user != user_index) {
    return ErrorCode::USER_ALREADY_EXISTS;
  }
  
  // Write to EEPROM
  ErrorCode err = eeprom->writeUserMobile(user_index, mobile);
  if (err != ErrorCode::SUCCESS) {
    return err;
  }
  
  // Update cache
  memcpy(mobile_cache[user_index], mobile, SystemConfig::MOBILE_NUMBER_LENGTH);
  
  return ErrorCode::SUCCESS;
}

bool UserManager::userExists(uint8_t user_id) {
  if (!validateUserID(user_id)) {
    return false;
  }
  return password_configured[user_id - 1];
}

bool UserManager::validatePassword(uint8_t user_id, const char* password, uint8_t password_len) {
  if (!validateUserID(user_id) || password == nullptr) {
    return false;
  }
  
  uint8_t user_index = user_id - 1;
  
  if (!password_configured[user_index]) {
    return false;
  }
  
  if (password_len != password_length_cache[user_index]) {
    return false;
  }
  
  // Compare passwords
  for (uint8_t i = 0; i < password_len; i++) {
    if (password[i] != password_cache[user_index][i]) {
      return false;
    }
  }
  
  return true;
}

ErrorCode UserManager::getPassword(uint8_t user_id, char* password, uint8_t* password_len) {
  if (!validateUserID(user_id) || password == nullptr || password_len == nullptr) {
    return ErrorCode::USER_INVALID_ID;
  }
  
  uint8_t user_index = user_id - 1;
  
  if (!password_configured[user_index]) {
    return ErrorCode::USER_NOT_FOUND;
  }
  
  *password_len = password_length_cache[user_index];
  memcpy(password, password_cache[user_index], *password_len);
  
  return ErrorCode::SUCCESS;
}

ErrorCode UserManager::getMobileNumber(uint8_t user_id, char* mobile) {
  if (!validateUserID(user_id) || mobile == nullptr) {
    return ErrorCode::USER_INVALID_ID;
  }
  
  uint8_t user_index = user_id - 1;
  
  if (!password_configured[user_index]) {
    return ErrorCode::USER_NOT_FOUND;
  }
  
  memcpy(mobile, mobile_cache[user_index], SystemConfig::MOBILE_NUMBER_LENGTH);
  return ErrorCode::SUCCESS;
}

int8_t UserManager::findUserByMobile(const char* mobile) {
  if (mobile == nullptr) {
    return -1;
  }
  
  for (uint8_t i = 0; i < SystemConfig::MAX_USER_TO_BE_STORED; i++) {
    if (password_configured[i]) {
      bool match = true;
      for (uint8_t j = 0; j < SystemConfig::MOBILE_NUMBER_LENGTH; j++) {
        if (mobile[j] != mobile_cache[i][j]) {
          match = false;
          break;
        }
      }
      if (match) {
        return i;
      }
    }
  }
  
  return -1;
}

ErrorCode UserManager::setTimeSlot(uint8_t user_id, uint8_t in_hour, uint8_t in_min, 
                                    uint8_t out_hour, uint8_t out_min) {
  if (!validateUserID(user_id)) {
    return ErrorCode::USER_INVALID_ID;
  }
  
  uint8_t user_index = user_id - 1;
  
  if (!password_configured[user_index]) {
    return ErrorCode::USER_NOT_FOUND;
  }
  
  if (in_hour > 23 || in_min > 59 || out_hour > 23 || out_min > 59) {
    return ErrorCode::SMS_INVALID_PARAMETERS;
  }
  
  // Write to EEPROM
  ErrorCode err = eeprom->writeTimeSlot(user_index, in_hour, in_min, out_hour, out_min);
  if (err != ErrorCode::SUCCESS) {
    return err;
  }
  
  // Update cache
  time_slot_configured[user_index] = true;
  in_time_hour[user_index] = in_hour;
  in_time_minute[user_index] = in_min;
  out_time_hour[user_index] = out_hour;
  out_time_minute[user_index] = out_min;
  
  return ErrorCode::SUCCESS;
}

ErrorCode UserManager::clearTimeSlot(uint8_t user_id) {
  if (!validateUserID(user_id)) {
    return ErrorCode::USER_INVALID_ID;
  }
  
  uint8_t user_index = user_id - 1;
  
  ErrorCode err = eeprom->clearTimeSlot(user_index);
  if (err != ErrorCode::SUCCESS) {
    return err;
  }
  
  time_slot_configured[user_index] = false;
  
  return ErrorCode::SUCCESS;
}

bool UserManager::isTimeSlotConfigured(uint8_t user_id) {
  if (!validateUserID(user_id)) {
    return false;
  }
  return time_slot_configured[user_id - 1];
}

bool UserManager::isAccessAllowed(uint8_t user_id, uint8_t current_hour, uint8_t current_minute) {
  if (!validateUserID(user_id)) {
    return false;
  }
  
  uint8_t user_index = user_id - 1;
  
  // If no time slot configured, allow access
  if (!time_slot_configured[user_index]) {
    return true;
  }
  
  // Calculate current time as minutes since midnight
  uint16_t current_time = current_hour * 100 + current_minute;
  uint16_t in_time = in_time_hour[user_index] * 100 + in_time_minute[user_index];
  uint16_t out_time = out_time_hour[user_index] * 100 + out_time_minute[user_index];
  
  // Check if current time is within allowed window
  if (current_time >= in_time && current_time <= out_time) {
    return true;
  }
  
  return false;
}

bool UserManager::isPasswordConfigured(uint8_t user_id) {
  if (!validateUserID(user_id)) {
    return false;
  }
  return password_configured[user_id - 1];
}

uint8_t UserManager::getPasswordLength(uint8_t user_id) {
  if (!validateUserID(user_id)) {
    return 0;
  }
  return password_length_cache[user_id - 1];
}

