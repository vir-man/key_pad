#ifndef USER_MANAGER_H
#define USER_MANAGER_H

#include "SystemConfig.h"
#include "ErrorCodes.h"
#include "EEPROMStorage.h"

/**
 * User Manager Class
 * Handles all user management operations
 * Memory optimized - uses EEPROMStorage for persistence
 */
class UserManager {
private:
  EEPROMStorage* eeprom;
  
  // In-memory cache (minimal - only what's needed)
  bool password_configured[SystemConfig::MAX_USER_TO_BE_STORED];
  char mobile_cache[SystemConfig::MAX_USER_TO_BE_STORED][SystemConfig::MOBILE_NUMBER_LENGTH];
  uint8_t password_length_cache[SystemConfig::MAX_USER_TO_BE_STORED];
  char password_cache[SystemConfig::MAX_USER_TO_BE_STORED][SystemConfig::PASSWORD_STORE_COUNT];
  bool time_slot_configured[SystemConfig::MAX_USER_TO_BE_STORED];
  uint8_t in_time_hour[SystemConfig::MAX_USER_TO_BE_STORED];
  uint8_t in_time_minute[SystemConfig::MAX_USER_TO_BE_STORED];
  uint8_t out_time_hour[SystemConfig::MAX_USER_TO_BE_STORED];
  uint8_t out_time_minute[SystemConfig::MAX_USER_TO_BE_STORED];
  
  // Helper functions
  bool validatePassword(const char* password, uint8_t len);
  bool validateMobile(const char* mobile);
  bool validateUserID(uint8_t user_id);
  void loadUserFromEEPROM(uint8_t user_index);
  
public:
  UserManager(EEPROMStorage* storage);
  bool initialize();
  
  // User operations
  ErrorCode addUser(uint8_t user_id, const char* mobile, const char* password, uint8_t password_len);
  ErrorCode removeUser(uint8_t user_id);
  ErrorCode updateUserPassword(uint8_t user_id, const char* old_password, uint8_t old_len, 
                               const char* new_password, uint8_t new_len);
  ErrorCode updateUserMobile(uint8_t user_id, const char* mobile);
  bool userExists(uint8_t user_id);
  
  // Password operations
  bool validatePassword(uint8_t user_id, const char* password, uint8_t password_len);
  ErrorCode getPassword(uint8_t user_id, char* password, uint8_t* password_len);
  
  // Mobile number operations
  ErrorCode getMobileNumber(uint8_t user_id, char* mobile);
  int8_t findUserByMobile(const char* mobile);
  
  // Time slot operations
  ErrorCode setTimeSlot(uint8_t user_id, uint8_t in_hour, uint8_t in_min, 
                        uint8_t out_hour, uint8_t out_min);
  ErrorCode clearTimeSlot(uint8_t user_id);
  bool isTimeSlotConfigured(uint8_t user_id);
  bool isAccessAllowed(uint8_t user_id, uint8_t current_hour, uint8_t current_minute);
  
  // Getters
  bool isPasswordConfigured(uint8_t user_id);
  uint8_t getPasswordLength(uint8_t user_id);
};

#endif // USER_MANAGER_H

