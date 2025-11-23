#ifndef HOLIDAY_MANAGER_H
#define HOLIDAY_MANAGER_H

#include "SystemConfig.h"
#include "ErrorCodes.h"
#include "EEPROMStorage.h"

/**
 * Holiday Manager Class
 * Handles holiday management operations
 * Memory optimized - reads from EEPROM on-demand
 */
class HolidayManager {
private:
  EEPROMStorage* eeprom;
  uint8_t holiday_count;
  
  // Helper functions
  bool validateDate(uint8_t date, uint8_t month, uint8_t year);
  bool isHolidayExists(uint8_t date, uint8_t month, uint8_t year);
  ErrorCode removeHolidayAtIndex(uint8_t index);
  
public:
  HolidayManager(EEPROMStorage* storage);
  bool initialize();
  
  // Holiday operations
  ErrorCode addHoliday(uint8_t date, uint8_t month, uint8_t year);
  ErrorCode removeHoliday(uint8_t date, uint8_t month, uint8_t year);
  bool isHoliday(uint8_t date, uint8_t month, uint8_t year);
  
  // View operations
  uint8_t getHolidayCount();
  ErrorCode getHoliday(uint8_t index, uint8_t* date, uint8_t* month, uint8_t* year);
  
  // Utility
  void refreshCount();
};

#endif // HOLIDAY_MANAGER_H

