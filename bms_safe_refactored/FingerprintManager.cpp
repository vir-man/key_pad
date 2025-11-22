#include "FingerprintManager.h"
#include <LiquidCrystal.h>

extern LiquidCrystal lcd;

FingerprintManager::FingerprintManager(HardwareSerial* serialPort) 
  : serial(serialPort), state(FingerprintManager::FingerprintState::IDLE) {
  finger = nullptr;
}

bool FingerprintManager::initialize() {
  if (serial == nullptr) {
    return false;
  }
  
  finger = new Adafruit_Fingerprint(serial);
  
  // begin() returns void, just initialize the serial
  finger->begin(SystemConfig::GSM_BAUD_RATE);
  
  // Verify password to check if sensor is responding
  if (finger->verifyPassword()) {
    return true;
  }
  
  return false;
}

void FingerprintManager::clearScreenAndEnrollFinger() {
  lcd.clear();
  lcd.setCursor(0, 0);
}

int8_t FingerprintManager::getFingerprintID() {
  if (finger == nullptr) {
    return -1;
  }
  
  uint8_t p = finger->getImage();
  if (p != FINGERPRINT_OK) {
    return -1;
  }
  
  p = finger->image2Tz();
  if (p != FINGERPRINT_OK) {
    return -1;
  }
  
  p = finger->fingerSearch();
  if (p == FINGERPRINT_OK) {
    return finger->fingerID;
  } else if (p == FINGERPRINT_NOTFOUND) {
    return SystemConfig::MAX_NUM_OF_USERS + 2;
  }
  
  return -1;
}

ErrorCode FingerprintManager::enrollFingerprint(uint8_t id) {
  if (finger == nullptr) {
    return ErrorCode::SYSTEM_NOT_INITIALIZED;
  }
  
  if (id < 1 || id > SystemConfig::MAX_NUM_OF_USERS) {
    return ErrorCode::AUTH_INVALID_USER_ID;
  }
  
  clearScreenAndEnrollFinger();
  lcd.print("PLACE FINGER");
  
  // Wait for valid finger
  int p = -1;
  while (p != FINGERPRINT_OK) {
    p = finger->getImage();
    if (p == FINGERPRINT_NOFINGER) {
      continue;
    } else if (p != FINGERPRINT_OK) {
      clearScreenAndEnrollFinger();
      lcd.print("UNKNOWN ERROR!");
      return ErrorCode::AUTH_FINGERPRINT_ERROR;
    }
  }
  
  clearScreenAndEnrollFinger();
  lcd.print("IMAGE TAKEN");
  
  p = finger->image2Tz(1);
  if (p != FINGERPRINT_OK) {
    clearScreenAndEnrollFinger();
    lcd.print("UNKNOWN ERROR!");
    return ErrorCode::AUTH_FINGERPRINT_ERROR;
  }
  
  clearScreenAndEnrollFinger();
  lcd.print("IMAGE CONVERTED");
  delay(500);
  
  clearScreenAndEnrollFinger();
  lcd.print("REMOVE FINGER!");
  delay(2000);
  
  // Wait for finger removal
  p = 0;
  while (p != FINGERPRINT_NOFINGER) {
    p = finger->getImage();
  }
  
  clearScreenAndEnrollFinger();
  lcd.print("CONFIRM FINGER!");
  
  // Wait for same finger again
  p = -1;
  while (p != FINGERPRINT_OK) {
    p = finger->getImage();
    if (p == FINGERPRINT_NOFINGER) {
      continue;
    } else if (p != FINGERPRINT_OK) {
      clearScreenAndEnrollFinger();
      lcd.print("UNKNOWN ERROR!");
      return ErrorCode::AUTH_FINGERPRINT_ERROR;
    }
  }
  
  clearScreenAndEnrollFinger();
  lcd.print("IMAGE TAKEN");
  
  p = finger->image2Tz(2);
  if (p != FINGERPRINT_OK) {
    clearScreenAndEnrollFinger();
    lcd.print("UNKNOWN ERROR!");
    return ErrorCode::AUTH_FINGERPRINT_ERROR;
  }
  
  clearScreenAndEnrollFinger();
  lcd.print("IMAGE CONVERTED");
  delay(500);
  
  // Create model
  p = finger->createModel();
  if (p != FINGERPRINT_OK) {
    clearScreenAndEnrollFinger();
    lcd.print("FINGERS NOT");
    lcd.setCursor(0, 1);
    lcd.print("MATCHED!");
    return ErrorCode::AUTH_FINGERPRINT_ERROR;
  }
  
  // Store model
  p = finger->storeModel(id);
  if (p != FINGERPRINT_OK) {
    clearScreenAndEnrollFinger();
    lcd.print("STORAGE FAILED!");
    return ErrorCode::AUTH_FINGERPRINT_ERROR;
  }
  
  clearScreenAndEnrollFinger();
  lcd.print("FINGERPRINT");
  lcd.setCursor(0, 1);
  lcd.print("ENROLLED!");
  
  return ErrorCode::SUCCESS;
}

ErrorCode FingerprintManager::deleteFingerprint(uint8_t id) {
  if (finger == nullptr) {
    return ErrorCode::SYSTEM_NOT_INITIALIZED;
  }
  
  if (id < 1 || id > SystemConfig::MAX_NUM_OF_USERS) {
    return ErrorCode::AUTH_INVALID_USER_ID;
  }
  
  uint8_t p = finger->deleteModel(id);
  if (p == FINGERPRINT_OK) {
    return ErrorCode::SUCCESS;
  }
  
  return ErrorCode::AUTH_FINGERPRINT_ERROR;
}

FingerprintManager::FingerprintState FingerprintManager::getState() {
  return state;
}

void FingerprintManager::setState(FingerprintManager::FingerprintState newState) {
  state = newState;
}

void FingerprintManager::resetState() {
  state = FingerprintManager::FingerprintState::IDLE;
}

bool FingerprintManager::getSensorInfo() {
  if (finger == nullptr) {
    return false;
  }
  
  finger->getParameters();
  return true;
}

uint8_t FingerprintManager::getTemplateCount() {
  if (finger == nullptr) {
    return 0;
  }
  
  finger->getTemplateCount();
  return finger->templateCount;
}

