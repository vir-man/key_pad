#include "ErrorCodes.h"
#include <avr/pgmspace.h>

const char* getErrorMessage(ErrorCode code) {
  switch(code) {
    case ErrorCode::SUCCESS:
      return PSTR("Success");
    case ErrorCode::AUTH_INVALID_PASSWORD:
      return PSTR("Invalid password");
    case ErrorCode::AUTH_INVALID_USER_ID:
      return PSTR("Invalid user ID");
    case ErrorCode::AUTH_MASTER_NOT_VERIFIED:
      return PSTR("Master not verified");
    case ErrorCode::AUTH_USER_NOT_VERIFIED:
      return PSTR("User not verified");
    case ErrorCode::AUTH_FINGERPRINT_NOT_FOUND:
      return PSTR("Fingerprint not found");
    case ErrorCode::AUTH_FINGERPRINT_ERROR:
      return PSTR("Fingerprint error");
    case ErrorCode::AUTH_TIMEOUT_EXPIRED:
      return PSTR("Authentication timeout");
    case ErrorCode::USER_NOT_FOUND:
      return PSTR("User not found");
    case ErrorCode::USER_ALREADY_EXISTS:
      return PSTR("User already exists");
    case ErrorCode::USER_INVALID_ID:
      return PSTR("Invalid user ID");
    case ErrorCode::USER_PASSWORD_TOO_SHORT:
      return PSTR("Password too short");
    case ErrorCode::USER_PASSWORD_TOO_LONG:
      return PSTR("Password too long");
    case ErrorCode::USER_MOBILE_INVALID:
      return PSTR("Invalid mobile number");
    case ErrorCode::DOOR_SENSOR_NOT_ALIGNED:
      return PSTR("Sensor not aligned");
    case ErrorCode::DOOR_OPEN_TIMEOUT:
      return PSTR("Door open timeout");
    case ErrorCode::DOOR_CLOSE_TIMEOUT:
      return PSTR("Door close timeout");
    case ErrorCode::DOOR_ACCESS_DENIED_HOLIDAY:
      return PSTR("Holiday - No access");
    case ErrorCode::DOOR_ACCESS_DENIED_TIME_SLOT:
      return PSTR("Outside time slot");
    case ErrorCode::DOOR_ALREADY_OPEN:
      return PSTR("Door already open");
    case ErrorCode::DOOR_ALREADY_CLOSED:
      return PSTR("Door already closed");
    case ErrorCode::HOLIDAY_NOT_FOUND:
      return PSTR("Holiday not found");
    case ErrorCode::HOLIDAY_ALREADY_EXISTS:
      return PSTR("Holiday already exists");
    case ErrorCode::HOLIDAY_INVALID_DATE:
      return PSTR("Invalid date");
    case ErrorCode::HOLIDAY_STORAGE_FULL:
      return PSTR("Holiday storage full");
    case ErrorCode::EEPROM_READ_ERROR:
      return PSTR("EEPROM read error");
    case ErrorCode::EEPROM_WRITE_ERROR:
      return PSTR("EEPROM write error");
    case ErrorCode::EEPROM_ADDRESS_OUT_OF_RANGE:
      return PSTR("EEPROM address out of range");
    case ErrorCode::EEPROM_DATA_CORRUPTED:
      return PSTR("EEPROM data corrupted");
    case ErrorCode::GSM_NOT_INITIALIZED:
      return PSTR("GSM not initialized");
    case ErrorCode::GSM_SEND_FAILED:
      return PSTR("GSM send failed");
    case ErrorCode::GSM_QUEUE_FULL:
      return PSTR("GSM queue full");
    case ErrorCode::GSM_MODULE_ERROR:
      return PSTR("GSM module error");
    case ErrorCode::SMS_INVALID_COMMAND:
      return PSTR("Invalid SMS command");
    case ErrorCode::SMS_INVALID_PARAMETERS:
      return PSTR("Invalid SMS parameters");
    case ErrorCode::SMS_MISSING_PARAMETERS:
      return PSTR("Missing SMS parameters");
    case ErrorCode::SMS_MOBILE_NOT_REGISTERED:
      return PSTR("Mobile not registered");
    case ErrorCode::SYSTEM_NOT_INITIALIZED:
      return PSTR("System not initialized");
    case ErrorCode::SYSTEM_MEMORY_LOW:
      return PSTR("Low memory");
    case ErrorCode::SYSTEM_HARDWARE_ERROR:
      return PSTR("Hardware error");
    default:
      return PSTR("Unknown error");
  }
}

