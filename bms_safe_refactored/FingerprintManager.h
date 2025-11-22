#ifndef FINGERPRINT_MANAGER_H
#define FINGERPRINT_MANAGER_H

#include <Adafruit_Fingerprint.h>
#include <HardwareSerial.h>
#include <LiquidCrystal.h>
#include "SystemConfig.h"
#include "ErrorCodes.h"

/**
 * Fingerprint Manager Class
 * Handles fingerprint sensor operations
 */
class FingerprintManager {
private:
  Adafruit_Fingerprint* finger;
  HardwareSerial* serial;
  LiquidCrystal* lcd;
  
  // Fingerprint FSM states
  enum class FingerprintState {
    IDLE = 1,           // Waiting for master fingerprint
    WRONG_MASTER = 2,   // Master fingerprint failed
    ENTER_USER = 3,     // Waiting for user fingerprint
    WRONG_USER = 4,     // User fingerprint failed
    DOOR_UNLOCKED = 5   // Authentication successful
  };
  
  FingerprintState state;
  
  // Helper functions
  void clearScreenAndEnrollFinger();
  
public:
  FingerprintManager(HardwareSerial* serialPort);
  bool initialize();
  
  // Set LCD pointer (for display during enrollment)
  void setLCD(LiquidCrystal* lcdInstance);
  
  // Fingerprint operations
  int8_t getFingerprintID();
  ErrorCode enrollFingerprint(uint8_t id);
  ErrorCode deleteFingerprint(uint8_t id);
  
  // State machine
  FingerprintState getState();
  void setState(FingerprintState newState);
  void resetState();
  
  // Sensor info
  bool getSensorInfo();
  uint8_t getTemplateCount();
};

#endif // FINGERPRINT_MANAGER_H

