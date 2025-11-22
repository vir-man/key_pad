#include "EEPROMStorage.h"
#include <string.h>

EEPROMStorage::EEPROMStorage() {
  alpha_speed_start_address = 0;
  door_open_count_start_address = 0;
  buzzer_timeout_start_address = 0;
  holiday_count_start_address = 0;
  holiday_data_start_address = 0;
}

bool EEPROMStorage::initialize() {
  // Compute global parameter addresses
  alpha_speed_start_address = eeprom_addr_after_users();
  door_open_count_start_address = alpha_speed_start_address + SystemConfig::ALPHA_SPEED_LEN_COUNT;
  buzzer_timeout_start_address = door_open_count_start_address + SystemConfig::DOOR_OPEN_COUNT_LEN_COUNT;
  holiday_count_start_address = buzzer_timeout_start_address + SystemConfig::BUZZER_TIMEOUT_LEN_COUNT;
  holiday_data_start_address = holiday_count_start_address + SystemConfig::HOLIDAY_COUNT_SIZE;
  
  // Validate and initialize defaults if needed
  if (!validateData()) {
    // Initialize defaults
    writeAlphaSpeed(SystemConfig::DEFAULT_ALPHA_SPEED);
    writeBuzzerTimeout(SystemConfig::DEFAULT_BUZZER_TIMEOUT);
    writeDoorOpenCount(0);
    writeHolidayCount(0);
  }
  
  return true;
}

// Helper functions
bool EEPROMStorage::check_is_configured_byte_address(uint16_t addr) {
  return (bool)(EEPROM.read(addr));
}

void EEPROMStorage::configure_byte_address(uint16_t addr) {
  EEPROM.write(addr, true);
}

void EEPROMStorage::clear_byte_address(uint16_t addr) {
  EEPROM.write(addr, false);
}

void EEPROMStorage::WriteEepromArray(uint16_t start_addr, const char* data, uint8_t len) {
  for (uint8_t i = 0; i < len; i++) {
    EEPROM.write(start_addr + i, data[i]);
  }
}

void EEPROMStorage::LoadFromEeprom(uint16_t start_addr, char* data, uint8_t len) {
  for (uint8_t i = 0; i < len; i++) {
    data[i] = EEPROM.read(start_addr + i);
  }
}

void EEPROMStorage::clearEepromArray(uint16_t start_addr, uint16_t end_addr) {
  for (uint16_t i = start_addr; i <= end_addr; i++) {
    EEPROM.write(i, 0);
  }
}

// User data operations
bool EEPROMStorage::isPasswordConfigured(uint8_t user_index) {
  if (user_index >= SystemConfig::MAX_USER_TO_BE_STORED) {
    return false;
  }
  return check_is_configured_byte_address(eeprom_addr_is_pw(user_index));
}

ErrorCode EEPROMStorage::readUserPassword(uint8_t user_index, char* password, uint8_t* password_len) {
  if (user_index >= SystemConfig::MAX_USER_TO_BE_STORED || password == nullptr || password_len == nullptr) {
    return ErrorCode::EEPROM_ADDRESS_OUT_OF_RANGE;
  }
  
  if (!isPasswordConfigured(user_index)) {
    return ErrorCode::USER_NOT_FOUND;
  }
  
  *password_len = EEPROM.read(eeprom_addr_pw_len(user_index));
  if (*password_len > SystemConfig::PASSWORD_STORE_COUNT) {
    return ErrorCode::EEPROM_DATA_CORRUPTED;
  }
  
  LoadFromEeprom(eeprom_addr_pw(user_index), password, *password_len);
  return ErrorCode::SUCCESS;
}

ErrorCode EEPROMStorage::readUserMobile(uint8_t user_index, char* mobile) {
  if (user_index >= SystemConfig::MAX_USER_TO_BE_STORED || mobile == nullptr) {
    return ErrorCode::EEPROM_ADDRESS_OUT_OF_RANGE;
  }
  
  if (!isPasswordConfigured(user_index)) {
    return ErrorCode::USER_NOT_FOUND;
  }
  
  LoadFromEeprom(eeprom_addr_mobile(user_index), mobile, SystemConfig::MOBILE_NUMBER_LENGTH);
  return ErrorCode::SUCCESS;
}

ErrorCode EEPROMStorage::writeUserPassword(uint8_t user_index, const char* password, uint8_t password_len) {
  if (user_index >= SystemConfig::MAX_USER_TO_BE_STORED || password == nullptr) {
    return ErrorCode::EEPROM_ADDRESS_OUT_OF_RANGE;
  }
  
  if (password_len < SystemConfig::MIN_PASSWORD_LEN || password_len > SystemConfig::MAX_PASSWORD_LEN) {
    return ErrorCode::USER_PASSWORD_TOO_SHORT;
  }
  
  EEPROM.write(eeprom_addr_pw_len(user_index), password_len);
  WriteEepromArray(eeprom_addr_pw(user_index), password, password_len);
  configure_byte_address(eeprom_addr_is_pw(user_index));
  
  return ErrorCode::SUCCESS;
}

ErrorCode EEPROMStorage::writeUserMobile(uint8_t user_index, const char* mobile) {
  if (user_index >= SystemConfig::MAX_USER_TO_BE_STORED || mobile == nullptr) {
    return ErrorCode::EEPROM_ADDRESS_OUT_OF_RANGE;
  }
  
  WriteEepromArray(eeprom_addr_mobile(user_index), mobile, SystemConfig::MOBILE_NUMBER_LENGTH);
  return ErrorCode::SUCCESS;
}

ErrorCode EEPROMStorage::clearUserPassword(uint8_t user_index) {
  if (user_index >= SystemConfig::MAX_USER_TO_BE_STORED) {
    return ErrorCode::EEPROM_ADDRESS_OUT_OF_RANGE;
  }
  
  clear_byte_address(eeprom_addr_is_pw(user_index));
  clear_byte_address(eeprom_addr_pw_len(user_index));
  clearEepromArray(eeprom_addr_mobile(user_index), 
                   eeprom_addr_mobile(user_index) + SystemConfig::MOBILE_NUMBER_LENGTH);
  clearEepromArray(eeprom_addr_pw(user_index), 
                   eeprom_addr_pw(user_index) + SystemConfig::PASSWORD_STORE_COUNT);
  clearTimeSlot(user_index);
  
  return ErrorCode::SUCCESS;
}

// Time slot operations
bool EEPROMStorage::isTimeSlotConfigured(uint8_t user_index) {
  if (user_index >= SystemConfig::MAX_USER_TO_BE_STORED) {
    return false;
  }
  return check_is_configured_byte_address(eeprom_addr_is_inout(user_index));
}

ErrorCode EEPROMStorage::readTimeSlot(uint8_t user_index, uint8_t* in_hour, uint8_t* in_min, 
                                      uint8_t* out_hour, uint8_t* out_min) {
  if (user_index >= SystemConfig::MAX_USER_TO_BE_STORED || 
      in_hour == nullptr || in_min == nullptr || out_hour == nullptr || out_min == nullptr) {
    return ErrorCode::EEPROM_ADDRESS_OUT_OF_RANGE;
  }
  
  if (!isTimeSlotConfigured(user_index)) {
    return ErrorCode::USER_NOT_FOUND;
  }
  
  *in_hour = EEPROM.read(eeprom_addr_in_time(user_index));
  *in_min = EEPROM.read(eeprom_addr_in_time(user_index) + 1);
  *out_hour = EEPROM.read(eeprom_addr_out_time(user_index));
  *out_min = EEPROM.read(eeprom_addr_out_time(user_index) + 1);
  
  return ErrorCode::SUCCESS;
}

ErrorCode EEPROMStorage::writeTimeSlot(uint8_t user_index, uint8_t in_hour, uint8_t in_min, 
                                       uint8_t out_hour, uint8_t out_min) {
  if (user_index >= SystemConfig::MAX_USER_TO_BE_STORED) {
    return ErrorCode::EEPROM_ADDRESS_OUT_OF_RANGE;
  }
  
  if (in_hour > 23 || in_min > 59 || out_hour > 23 || out_min > 59) {
    return ErrorCode::SMS_INVALID_PARAMETERS;
  }
  
  EEPROM.write(eeprom_addr_in_time(user_index), in_hour);
  EEPROM.write(eeprom_addr_in_time(user_index) + 1, in_min);
  EEPROM.write(eeprom_addr_out_time(user_index), out_hour);
  EEPROM.write(eeprom_addr_out_time(user_index) + 1, out_min);
  configure_byte_address(eeprom_addr_is_inout(user_index));
  
  return ErrorCode::SUCCESS;
}

ErrorCode EEPROMStorage::clearTimeSlot(uint8_t user_index) {
  if (user_index >= SystemConfig::MAX_USER_TO_BE_STORED) {
    return ErrorCode::EEPROM_ADDRESS_OUT_OF_RANGE;
  }
  
  clear_byte_address(eeprom_addr_is_inout(user_index));
  return ErrorCode::SUCCESS;
}

// Holiday operations
uint8_t EEPROMStorage::readHolidayCount() {
  uint8_t count = EEPROM.read(holiday_count_start_address);
  if (count > SystemConfig::MAX_HOLIDAYS) {
    return 0;
  }
  return count;
}

ErrorCode EEPROMStorage::readHoliday(uint8_t index, uint8_t* date, uint8_t* month, uint8_t* year) {
  if (index >= SystemConfig::MAX_HOLIDAYS || date == nullptr || month == nullptr || year == nullptr) {
    return ErrorCode::EEPROM_ADDRESS_OUT_OF_RANGE;
  }
  
  uint8_t count = readHolidayCount();
  if (index >= count) {
    return ErrorCode::HOLIDAY_NOT_FOUND;
  }
  
  uint16_t addr = holiday_data_start_address + (index * SystemConfig::HOLIDAY_DATA_SIZE);
  *date = EEPROM.read(addr);
  *month = EEPROM.read(addr + 1);
  *year = EEPROM.read(addr + 2);
  
  return ErrorCode::SUCCESS;
}

ErrorCode EEPROMStorage::writeHoliday(uint8_t index, uint8_t date, uint8_t month, uint8_t year) {
  if (index >= SystemConfig::MAX_HOLIDAYS) {
    return ErrorCode::HOLIDAY_STORAGE_FULL;
  }
  
  if (date == 0 || date > 31 || month == 0 || month > 12 || year > 99) {
    return ErrorCode::HOLIDAY_INVALID_DATE;
  }
  
  uint16_t addr = holiday_data_start_address + (index * SystemConfig::HOLIDAY_DATA_SIZE);
  EEPROM.write(addr, date);
  EEPROM.write(addr + 1, month);
  EEPROM.write(addr + 2, year);
  
  return ErrorCode::SUCCESS;
}

ErrorCode EEPROMStorage::writeHolidayCount(uint8_t count) {
  if (count > SystemConfig::MAX_HOLIDAYS) {
    return ErrorCode::HOLIDAY_STORAGE_FULL;
  }
  
  EEPROM.write(holiday_count_start_address, count);
  return ErrorCode::SUCCESS;
}

// System parameters
uint16_t EEPROMStorage::readAlphaSpeed() {
  uint8_t b1 = EEPROM.read(alpha_speed_start_address);
  uint8_t b2 = EEPROM.read(alpha_speed_start_address + 1);
  uint16_t speed = (b1 << 8) + b2;
  
  if (speed == 0) {
    writeAlphaSpeed(SystemConfig::DEFAULT_ALPHA_SPEED);
    return SystemConfig::DEFAULT_ALPHA_SPEED;
  }
  
  return speed;
}

ErrorCode EEPROMStorage::writeAlphaSpeed(uint16_t speed) {
  uint8_t b1 = speed >> 8;
  uint8_t b2 = speed & 0xFF;
  EEPROM.write(alpha_speed_start_address, b1);
  EEPROM.write(alpha_speed_start_address + 1, b2);
  return ErrorCode::SUCCESS;
}

uint16_t EEPROMStorage::readDoorOpenCount() {
  uint8_t b1 = EEPROM.read(door_open_count_start_address);
  uint8_t b2 = EEPROM.read(door_open_count_start_address + 1);
  return (b1 << 8) + b2;
}

ErrorCode EEPROMStorage::writeDoorOpenCount(uint16_t count) {
  uint8_t b1 = count >> 8;
  uint8_t b2 = count & 0xFF;
  EEPROM.write(door_open_count_start_address, b1);
  EEPROM.write(door_open_count_start_address + 1, b2);
  return ErrorCode::SUCCESS;
}

uint16_t EEPROMStorage::readBuzzerTimeout() {
  uint8_t b1 = EEPROM.read(buzzer_timeout_start_address);
  uint8_t b2 = EEPROM.read(buzzer_timeout_start_address + 1);
  uint16_t timeout = (b1 << 8) + b2;
  
  if (timeout == 0) {
    writeBuzzerTimeout(SystemConfig::DEFAULT_BUZZER_TIMEOUT);
    return SystemConfig::DEFAULT_BUZZER_TIMEOUT;
  }
  
  return timeout;
}

ErrorCode EEPROMStorage::writeBuzzerTimeout(uint16_t timeout) {
  uint8_t b1 = timeout >> 8;
  uint8_t b2 = timeout & 0xFF;
  EEPROM.write(buzzer_timeout_start_address, b1);
  EEPROM.write(buzzer_timeout_start_address + 1, b2);
  return ErrorCode::SUCCESS;
}

// Utility
void EEPROMStorage::clearAll() {
  for (uint16_t i = 0; i < EEPROM.length(); i++) {
    EEPROM.write(i, 0);
  }
}

bool EEPROMStorage::validateData() {
  // Basic validation - check if addresses are within EEPROM range
  if (holiday_data_start_address + (SystemConfig::MAX_HOLIDAYS * SystemConfig::HOLIDAY_DATA_SIZE) > EEPROM.length()) {
    return false;
  }
  return true;
}

