#ifndef SMS_COMMAND_PARSER_H
#define SMS_COMMAND_PARSER_H

#include <HardwareSerial.h>
#include "SystemConfig.h"
#include "ErrorCodes.h"
#include "UserManager.h"
#include "AuthenticationManager.h"
#include "DoorController.h"
#include "HolidayManager.h"
#include "GSMHandler.h"

/**
 * SMS Command Parser Class
 * Parses and executes SMS commands
 * Handles all SMS command flows from README.md
 */
class SMSCommandParser {
private:
  HardwareSerial* gsmSerial;
  UserManager* userManager;
  AuthenticationManager* authManager;
  DoorController* doorController;
  HolidayManager* holidayManager;
  GSMHandler* gsmHandler;
  
  // Parsing buffers
  char serial_buffer[SystemConfig::SMS_SERIAL_BUFFER_SIZE];
  char cmd[SystemConfig::MAX_CMD_LEN];
  char para[SystemConfig::MAX_PARAMETER][SystemConfig::MAX_PARA_LEN];
  uint8_t para_len[SystemConfig::MAX_PARAMETER];
  uint8_t para_count;
  uint8_t cmd_length;
  
  char received_mobile_number[SystemConfig::MOBILE_NUMBER_LENGTH + 1];
  int8_t received_mobile_number_index;
  
  // Helper functions
  uint16_t readSerialToBuffer(Stream &stream, char *buffer, uint16_t max_len);
  bool parseCommand(const char* input, uint16_t inputLen);
  void copyArray(const char* src, char* dst, uint8_t len);
  uint8_t parseUserID(const char* str, uint8_t len);
  void sendResponse(const char* message);
  void sendResponseWithDesc(uint8_t desc_code);
  
public:
  SMSCommandParser(HardwareSerial* serial, UserManager* userMgr, 
                   AuthenticationManager* authMgr, DoorController* doorCtrl,
                   HolidayManager* holidayMgr, GSMHandler* gsm);
  bool initialize();
  
  // Command execution
  uint8_t executeCommand(const char* cmd, uint8_t para_count, char para[][SystemConfig::MAX_PARA_LEN], uint8_t para_len[]);
  
  // API functions
  uint8_t apiUnlockDoor();
  uint8_t apiLockDoor();
  uint8_t apiAddUser();
  uint8_t apiRemoveUser();
  uint8_t apiChangePassword();
  uint8_t apiUpdateTimeSlot();
  uint8_t apiLostPassword();
  uint8_t apiFactoryReset();
  
  // Task function (call in loop)
  void task();
  
  // Getters
  int8_t getReceivedMobileNumberIndex();
};

#endif // SMS_COMMAND_PARSER_H

