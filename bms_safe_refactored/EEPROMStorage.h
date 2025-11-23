#ifndef EEPROM_STORAGE_H
#define EEPROM_STORAGE_H

#include <EEPROM.h>
#include <Arduino.h>
#include "SystemConfig.h"
#include "ErrorCodes.h"

/**
 * EEPROM Storage Manager
 * Handles all EEPROM read/write operations with error handling
 * Memory optimized - computes addresses on-the-fly
 */
class EEPROMStorage {
private:
  // EEPROM address offsets (computed once)
  uint16_t alpha_speed_start_address;
  uint16_t door_open_count_start_address;
  uint16_t buzzer_timeout_start_address;
  uint16_t holiday_count_start_address;
  uint16_t holiday_data_start_address;
  
  // Helper functions for address computation
  static inline uint16_t eeprom_addr_user_base(uint8_t index) {
    return SystemConfig::EEPROM_STARTING_ADDRESS + 
           (uint16_t)index * SystemConfig::USER_BLOCK_SIZE;
  }
  
  static inline uint16_t eeprom_addr_is_pw(uint8_t index) {
    return eeprom_addr_user_base(index);
  }
  
  static inline uint16_t eeprom_addr_mobile(uint8_t index) {
    return eeprom_addr_is_pw(index) + 1;
  }
  
  static inline uint16_t eeprom_addr_pw_len(uint8_t index) {
    return eeprom_addr_mobile(index) + SystemConfig::MOBILE_NUMBER_LENGTH + 1;
  }
  
  static inline uint16_t eeprom_addr_pw(uint8_t index) {
    return eeprom_addr_pw_len(index) + 1;
  }
  
  static inline uint16_t eeprom_addr_is_inout(uint8_t index) {
    return eeprom_addr_pw(index) + SystemConfig::PASSWORD_STORE_COUNT + 1;
  }
  
  static inline uint16_t eeprom_addr_in_time(uint8_t index) {
    return eeprom_addr_is_inout(index) + 1;
  }
  
  static inline uint16_t eeprom_addr_out_time(uint8_t index) {
    return eeprom_addr_in_time(index) + SystemConfig::IN_OUT_TIME_LEN_COUNT;
  }
  
  static inline uint16_t eeprom_addr_after_users() {
    return eeprom_addr_out_time(SystemConfig::MAX_USER_TO_BE_STORED - 1) + 
           SystemConfig::IN_OUT_TIME_LEN_COUNT + 1;
  }
  
  // Helper functions
  bool check_is_configured_byte_address(uint16_t addr);
  void configure_byte_address(uint16_t addr);
  void clear_byte_address(uint16_t addr);
  void WriteEepromArray(uint16_t start_addr, const char* data, uint8_t len);
  void LoadFromEeprom(uint16_t start_addr, char* data, uint8_t len);
  void clearEepromArray(uint16_t start_addr, uint16_t end_addr);
  
public:
  EEPROMStorage();
  bool initialize();
  
  // User data operations
  bool isPasswordConfigured(uint8_t user_index);
  ErrorCode readUserPassword(uint8_t user_index, char* password, uint8_t* password_len);
  ErrorCode readUserMobile(uint8_t user_index, char* mobile);
  ErrorCode writeUserPassword(uint8_t user_index, const char* password, uint8_t password_len);
  ErrorCode writeUserMobile(uint8_t user_index, const char* mobile);
  ErrorCode clearUserPassword(uint8_t user_index);
  
  // Time slot operations
  bool isTimeSlotConfigured(uint8_t user_index);
  ErrorCode readTimeSlot(uint8_t user_index, uint8_t* in_hour, uint8_t* in_min, 
                    uint8_t* out_hour, uint8_t* out_min);
  ErrorCode writeTimeSlot(uint8_t user_index, uint8_t in_hour, uint8_t in_min, 
                     uint8_t out_hour, uint8_t out_min);
  ErrorCode clearTimeSlot(uint8_t user_index);
  
  // Holiday operations (read from EEPROM on-demand)
  uint8_t readHolidayCount();
  ErrorCode readHoliday(uint8_t index, uint8_t* date, uint8_t* month, uint8_t* year);
  ErrorCode writeHoliday(uint8_t index, uint8_t date, uint8_t month, uint8_t year);
  ErrorCode writeHolidayCount(uint8_t count);
  
  // System parameters
  uint16_t readAlphaSpeed();
  ErrorCode writeAlphaSpeed(uint16_t speed);
  uint16_t readDoorOpenCount();
  ErrorCode writeDoorOpenCount(uint16_t count);
  uint16_t readBuzzerTimeout();
  ErrorCode writeBuzzerTimeout(uint16_t timeout);
  
  // Utility
  void clearAll();
  bool validateData();
};

#endif // EEPROM_STORAGE_H

