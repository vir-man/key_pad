#include "HolidayManager.h"

HolidayManager::HolidayManager(EEPROMStorage* storage) : eeprom(storage), holiday_count(0) {
}

bool HolidayManager::initialize() {
  if (eeprom == nullptr) {
    return false;
  }
  
  refreshCount();
  return true;
}

bool HolidayManager::validateDate(uint8_t date, uint8_t month, uint8_t year) {
  if (date == 0 || date > 31) {
    return false;
  }
  if (month == 0 || month > 12) {
    return false;
  }
  if (year > 99) {
    return false;
  }
  
  // Basic validation - could add more sophisticated date validation
  return true;
}

bool HolidayManager::isHolidayExists(uint8_t date, uint8_t month, uint8_t year) {
  uint8_t count = eeprom->readHolidayCount();
  
  for (uint8_t i = 0; i < count && i < SystemConfig::MAX_HOLIDAYS; i++) {
    uint8_t h_date, h_month, h_year;
    if (eeprom->readHoliday(i, &h_date, &h_month, &h_year) == ErrorCode::SUCCESS) {
      if (h_date == date && h_month == month && h_year == year) {
        return true;
      }
    }
  }
  
  return false;
}

ErrorCode HolidayManager::addHoliday(uint8_t date, uint8_t month, uint8_t year) {
  if (eeprom == nullptr) {
    return ErrorCode::SYSTEM_NOT_INITIALIZED;
  }
  
  if (!validateDate(date, month, year)) {
    return ErrorCode::HOLIDAY_INVALID_DATE;
  }
  
  refreshCount();
  
  if (holiday_count >= SystemConfig::MAX_HOLIDAYS) {
    return ErrorCode::HOLIDAY_STORAGE_FULL;
  }
  
  if (isHolidayExists(date, month, year)) {
    return ErrorCode::HOLIDAY_ALREADY_EXISTS;
  }
  
  // Write holiday to EEPROM at current count position
  ErrorCode err = eeprom->writeHoliday(holiday_count, date, month, year);
  if (err != ErrorCode::SUCCESS) {
    return err;
  }
  
  // Update count
  holiday_count++;
  err = eeprom->writeHolidayCount(holiday_count);
  if (err != ErrorCode::SUCCESS) {
    return err;
  }
  
  return ErrorCode::SUCCESS;
}

ErrorCode HolidayManager::removeHoliday(uint8_t date, uint8_t month, uint8_t year) {
  if (eeprom == nullptr) {
    return ErrorCode::SYSTEM_NOT_INITIALIZED;
  }
  
  refreshCount();
  
  // Find the holiday
  uint8_t found_index = SystemConfig::MAX_HOLIDAYS;
  for (uint8_t i = 0; i < holiday_count && i < SystemConfig::MAX_HOLIDAYS; i++) {
    uint8_t h_date, h_month, h_year;
    if (eeprom->readHoliday(i, &h_date, &h_month, &h_year) == ErrorCode::SUCCESS) {
      if (h_date == date && h_month == month && h_year == year) {
        found_index = i;
        break;
      }
    }
  }
  
  if (found_index >= SystemConfig::MAX_HOLIDAYS) {
    return ErrorCode::HOLIDAY_NOT_FOUND;
  }
  
  return removeHolidayAtIndex(found_index);
}

ErrorCode HolidayManager::removeHolidayAtIndex(uint8_t index) {
  if (index >= holiday_count || index >= SystemConfig::MAX_HOLIDAYS) {
    return ErrorCode::HOLIDAY_NOT_FOUND;
  }
  
  // Shift remaining holidays in EEPROM
  for (uint8_t i = index; i < holiday_count - 1; i++) {
    uint8_t h_date, h_month, h_year;
    if (eeprom->readHoliday(i + 1, &h_date, &h_month, &h_year) == ErrorCode::SUCCESS) {
      eeprom->writeHoliday(i, h_date, h_month, h_year);
    }
  }
  
  // Update count
  holiday_count--;
  ErrorCode err = eeprom->writeHolidayCount(holiday_count);
  if (err != ErrorCode::SUCCESS) {
    return err;
  }
  
  return ErrorCode::SUCCESS;
}

bool HolidayManager::isHoliday(uint8_t date, uint8_t month, uint8_t year) {
  if (eeprom == nullptr) {
    return false;
  }
  
  return isHolidayExists(date, month, year);
}

uint8_t HolidayManager::getHolidayCount() {
  refreshCount();
  return holiday_count;
}

ErrorCode HolidayManager::getHoliday(uint8_t index, uint8_t* date, uint8_t* month, uint8_t* year) {
  if (eeprom == nullptr || date == nullptr || month == nullptr || year == nullptr) {
    return ErrorCode::SYSTEM_NOT_INITIALIZED;
  }
  
  refreshCount();
  
  if (index >= holiday_count || index >= SystemConfig::MAX_HOLIDAYS) {
    return ErrorCode::HOLIDAY_NOT_FOUND;
  }
  
  return eeprom->readHoliday(index, date, month, year);
}

void HolidayManager::refreshCount() {
  if (eeprom != nullptr) {
    holiday_count = eeprom->readHolidayCount();
    if (holiday_count > SystemConfig::MAX_HOLIDAYS) {
      holiday_count = 0;
      eeprom->writeHolidayCount(0);
    }
  }
}

