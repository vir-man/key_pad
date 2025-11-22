#include "SMSCommandParser.h"
#include <string.h>
#include <avr/pgmspace.h>

// Response message codes (matching original system)
// Note: Using SMS_MSG_ prefix to avoid conflicts with SystemConfig constants
#define SMS_MSG_USER_NO_REGISTERED 1
#define SMS_MSG_PARA_MISSING 2
#define SMS_MSG_PARA_INVALID 3
#define SMS_MSG_PW_IS_NO_VALID 4
#define SMS_MSG_NO_ACCESS_ALLOWED 5
#define SMS_MSG_DOOR_UNLOCK_CMD_ACCEPTED 6
#define SMS_MSG_DOOR_LOCK_CMD_ACCEPTED 7
#define SMS_MSG_PW_LENGH_IS_NOT_IN_LIMIT 8
#define SMS_MSG_USER_ALREADY_EXISTS 9
#define SMS_MSG_USER_NOT_FOUND 10
#define SMS_MSG_PW_CHANGED 11
#define SMS_MSG_PW_IS_NOT_CONFIGURED 12
#define SMS_MSG_MOBILE_NUMBER_IS_NOT_REGISTERD 13
#define SMS_MSG_CMD_EXECUTED 14

SMSCommandParser::SMSCommandParser(HardwareSerial* serial, UserManager* userMgr, 
                                     AuthenticationManager* authMgr, DoorController* doorCtrl,
                                     HolidayManager* holidayMgr, GSMHandler* gsm)
  : gsmSerial(serial), userManager(userMgr), authManager(authMgr), 
    doorController(doorCtrl), holidayManager(holidayMgr), gsmHandler(gsm),
    para_count(0), cmd_length(0), received_mobile_number_index(-1) {
  memset(serial_buffer, 0, sizeof(serial_buffer));
  memset(cmd, 0, sizeof(cmd));
  memset(para, 0, sizeof(para));
  memset(para_len, 0, sizeof(para_len));
  memset(received_mobile_number, 0, sizeof(received_mobile_number));
}

bool SMSCommandParser::initialize() {
  if (gsmSerial == nullptr || userManager == nullptr || authManager == nullptr || 
      doorController == nullptr || holidayManager == nullptr || gsmHandler == nullptr) {
    return false;
  }
  return true;
}

uint16_t SMSCommandParser::readSerialToBuffer(Stream &stream, char *buffer, uint16_t max_len) {
  uint16_t index = 0;
  unsigned long start_time = millis();
  unsigned long last_char_time = start_time;
  const unsigned long TIMEOUT_MS = 3000;
  const unsigned long IDLE_TIMEOUT_MS = 500;
  bool has_cmt = false;
  bool found_end = false;
  
  memset(buffer, 0, max_len);
  
  while (index < max_len - 1) {
    if (stream.available()) {
      char c = stream.read();
      buffer[index++] = c;
      last_char_time = millis();
      
      if (c == '\n' || c == '\r') {
        continue;  // Skip newlines
      }
      
      if (strstr(buffer, "+CMT:") != nullptr) {
        has_cmt = true;
      }
      
      if (c == SystemConfig::MSG_END_CHAR) {  // '#'
        found_end = true;
        if (has_cmt) {
          break;  // Complete message received
        }
      }
    } else {
      if (has_cmt && found_end) {
        break;  // Complete message received
      }
      if (millis() - last_char_time > IDLE_TIMEOUT_MS && index > 0) {
        if (!has_cmt) {
          break;  // No message start, timeout
        }
      }
      if (millis() - start_time > TIMEOUT_MS) {
        break;  // Total timeout
      }
      delay(10);
    }
  }
  
  buffer[index] = '\0';
  return index;
}

bool SMSCommandParser::parseCommand(const char* input, uint16_t inputLen) {
  if (input == nullptr || inputLen == 0) {
    return false;
  }
  
  // Find command start '&'
  int16_t start_idx = -1;
  int16_t end_idx = -1;
  
  for (uint16_t i = 0; i < inputLen; i++) {
    if (input[i] == SystemConfig::MSG_START_CHAR) {  // '&'
      start_idx = i;
    } else if (input[i] == SystemConfig::MSG_END_CHAR && start_idx >= 0) {  // '#'
      end_idx = i;
      break;
    }
  }
  
  if (start_idx < 0 || end_idx < 0 || end_idx <= start_idx) {
    return false;
  }
  
  // Extract segment between & and #
  uint16_t segLen = end_idx - start_idx - 1;
  if (segLen == 0) {
    return false;
  }
  
  // Parse command and parameters
  memset(cmd, 0, sizeof(cmd));
  memset(para, 0, sizeof(para));
  memset(para_len, 0, sizeof(para_len));
  para_count = 0;
  cmd_length = 0;
  
  uint16_t cmd_start = start_idx + 1;
  uint16_t cmd_end = cmd_start;
  
  // Find command end (first comma or end)
  for (uint16_t i = cmd_start; i < start_idx + 1 + segLen; i++) {
    if (input[i] == SystemConfig::CMD_SEPARATOR) {  // ','
      cmd_end = i;
      break;
    }
    cmd_end = i + 1;
  }
  
  // Extract command
  uint8_t cmd_idx = 0;
  for (uint16_t i = cmd_start; i < cmd_end && cmd_idx < SystemConfig::MAX_CMD_LEN - 1; i++) {
    if (input[i] != '\n' && input[i] != '\r' && input[i] != ' ') {
      cmd[cmd_idx++] = input[i];
    }
  }
  cmd_length = cmd_idx;
  
  // Extract parameters
  if (cmd_end < start_idx + 1 + segLen) {
    uint16_t para_start = cmd_end + 1;
    uint8_t current_para = 0;
    uint8_t current_para_idx = 0;
    
    for (uint16_t i = para_start; i <= start_idx + 1 + segLen && current_para < SystemConfig::MAX_PARAMETER; i++) {
      if (input[i] == SystemConfig::CMD_SEPARATOR || i == start_idx + 1 + segLen) {
        if (current_para_idx > 0) {
          para_len[current_para] = current_para_idx;
          current_para++;
          current_para_idx = 0;
        }
      } else if (input[i] != '\n' && input[i] != '\r' && input[i] != ' ') {
        if (current_para_idx < SystemConfig::MAX_PARA_LEN - 1) {
          para[current_para][current_para_idx++] = input[i];
        }
      }
    }
    
    if (current_para_idx > 0) {
      para_len[current_para] = current_para_idx;
      para_count = current_para + 1;
    } else {
      para_count = current_para;
    }
  }
  
  // Extract mobile number from +CMT: header
  const char* cmt_pos = strstr(input, "+CMT:");
  if (cmt_pos != nullptr) {
    const char* quote1 = strchr(cmt_pos, '"');
    if (quote1 != nullptr) {
      const char* quote2 = strchr(quote1 + 1, '"');
      if (quote2 != nullptr) {
        uint8_t mobile_len = quote2 - quote1 - 1;
        if (mobile_len <= SystemConfig::MOBILE_NUMBER_LENGTH) {
          memcpy(received_mobile_number, quote1 + 1, mobile_len);
          received_mobile_number[mobile_len] = '\0';
          
          // Find user index by mobile number
          received_mobile_number_index = userManager->findUserByMobile(received_mobile_number);
        }
      }
    }
  }
  
  return true;
}

void SMSCommandParser::copyArray(const char* src, char* dst, uint8_t len) {
  for (uint8_t i = 0; i < len; i++) {
    dst[i] = src[i];
  }
}

uint8_t SMSCommandParser::parseUserID(const char* str, uint8_t len) {
  uint8_t user_id = 0;
  for (uint8_t i = 0; i < len && i < 2; i++) {
    if (str[i] >= '0' && str[i] <= '9') {
      user_id = user_id * 10 + (str[i] - '0');
    }
  }
  return user_id;
}

void SMSCommandParser::sendResponse(const char* message) {
  if (gsmHandler == nullptr || received_mobile_number_index < 0) {
    return;
  }
  
  gsmHandler->sendSMSDirect(received_mobile_number, message);
}

void SMSCommandParser::sendResponseWithDesc(uint8_t desc_code) {
  const char* message = "";
  
  switch(desc_code) {
    case SMS_MSG_DOOR_UNLOCK_CMD_ACCEPTED:
      message = PSTR("Door Unlock Command Accepted!");
      break;
    case SMS_MSG_DOOR_LOCK_CMD_ACCEPTED:
      message = PSTR("Door Lock Command Accepted!");
      break;
    case SMS_MSG_CMD_EXECUTED:
      message = PSTR("Command Executed");
      break;
    case SMS_MSG_PW_CHANGED:
      message = PSTR("Password Changed");
      break;
    case SMS_MSG_USER_NO_REGISTERED:
      message = PSTR("User No Registered");
      break;
    case SMS_MSG_PARA_MISSING:
      message = PSTR("Parameters missing");
      break;
    case SMS_MSG_PARA_INVALID:
      message = PSTR("Invalid parameters");
      break;
    case SMS_MSG_PW_IS_NO_VALID:
      message = PSTR("Password is not valid");
      break;
    case SMS_MSG_NO_ACCESS_ALLOWED:
      message = PSTR("No Access Allowed");
      break;
    case SMS_MSG_PW_LENGH_IS_NOT_IN_LIMIT:
      message = PSTR("Password length is not in limit");
      break;
    case SMS_MSG_USER_ALREADY_EXISTS:
      message = PSTR("User already exists");
      break;
    case SMS_MSG_USER_NOT_FOUND:
      message = PSTR("User not found");
      break;
    case SMS_MSG_PW_IS_NOT_CONFIGURED:
      message = PSTR("Password is not configured");
      break;
    case SMS_MSG_MOBILE_NUMBER_IS_NOT_REGISTERD:
      message = PSTR("Mobile number is not registered");
      break;
    default:
      message = PSTR("Unknown error");
      break;
  }
  
  sendResponse(message);
}

uint8_t SMSCommandParser::apiUnlockDoor() {
  if (received_mobile_number_index < 0) {
    sendResponseWithDesc(SMS_MSG_USER_NO_REGISTERED);
    return SystemConfig::CMD_NOT_FOUND;
  }
  
  if (para_count < 2) {
    sendResponseWithDesc(SMS_MSG_PARA_MISSING);
    return SystemConfig::CMD_NOT_FOUND;
  }
  
  uint8_t user_id = parseUserID(para[0], para_len[0]);
  if (user_id == 0 || user_id > SystemConfig::MAX_NUM_OF_USERS) {
    sendResponseWithDesc(SMS_MSG_PARA_INVALID);
    return SystemConfig::CMD_NOT_FOUND;
  }
  
  if (para_len[1] < SystemConfig::MIN_PASSWORD_LEN || para_len[1] > SystemConfig::MAX_PASSWORD_LEN) {
    sendResponseWithDesc(SMS_MSG_PW_LENGH_IS_NOT_IN_LIMIT);
    return SystemConfig::CMD_NOT_FOUND;
  }
  
  char password[SystemConfig::PASSWORD_STORE_COUNT];
  memset(password, 0, sizeof(password));
  copyArray(para[1], password, para_len[1]);
  
  // Two-step authentication
  if (user_id == SystemConfig::MASTER_USER_ID) {
    // Step 1: Master verification
    ErrorCode err = authManager->smsVerifyMaster(user_id, password, para_len[1]);
    if (err == ErrorCode::SUCCESS) {
      sendResponseWithDesc(SMS_MSG_DOOR_UNLOCK_CMD_ACCEPTED);
      return SystemConfig::CMD_EXECUTED;
    } else {
      sendResponseWithDesc(SMS_MSG_PW_IS_NO_VALID);
      return SystemConfig::CMD_NOT_FOUND;
    }
  } else {
    // Step 2: User verification and unlock
    ErrorCode err = authManager->smsVerifyUser(user_id, password, para_len[1]);
    if (err == ErrorCode::SUCCESS) {
      // Unlock door
      doorController->openDoor(user_id);
      sendResponseWithDesc(SMS_MSG_DOOR_UNLOCK_CMD_ACCEPTED);
      return SystemConfig::CMD_EXECUTED;
    } else if (err == ErrorCode::AUTH_MASTER_NOT_VERIFIED) {
      sendResponseWithDesc(SMS_MSG_NO_ACCESS_ALLOWED);
      return SystemConfig::CMD_NOT_FOUND;
    } else if (err == ErrorCode::DOOR_ACCESS_DENIED_HOLIDAY || err == ErrorCode::DOOR_ACCESS_DENIED_TIME_SLOT) {
      sendResponseWithDesc(SMS_MSG_NO_ACCESS_ALLOWED);
      return SystemConfig::CMD_NOT_FOUND;
    } else {
      sendResponseWithDesc(SMS_MSG_PW_IS_NO_VALID);
      return SystemConfig::CMD_NOT_FOUND;
    }
  }
}

uint8_t SMSCommandParser::apiLockDoor() {
  if (received_mobile_number_index < 0) {
    sendResponseWithDesc(SMS_MSG_USER_NO_REGISTERED);
    return SystemConfig::CMD_NOT_FOUND;
  }
  
  if (para_count < 2) {
    sendResponseWithDesc(SMS_MSG_PARA_MISSING);
    return SystemConfig::CMD_NOT_FOUND;
  }
  
  uint8_t user_id = parseUserID(para[0], para_len[0]);
  if (user_id == 0 || user_id > SystemConfig::MAX_NUM_OF_USERS) {
    sendResponseWithDesc(SMS_MSG_PARA_INVALID);
    return SystemConfig::CMD_NOT_FOUND;
  }
  
  if (para_len[1] < SystemConfig::MIN_PASSWORD_LEN || para_len[1] > SystemConfig::MAX_PASSWORD_LEN) {
    sendResponseWithDesc(SMS_MSG_PW_LENGH_IS_NOT_IN_LIMIT);
    return SystemConfig::CMD_NOT_FOUND;
  }
  
  char password[SystemConfig::PASSWORD_STORE_COUNT];
  memset(password, 0, sizeof(password));
  copyArray(para[1], password, para_len[1]);
  
  if (!userManager->validatePassword(user_id, password, para_len[1])) {
    sendResponseWithDesc(SMS_MSG_PW_IS_NO_VALID);
    return SystemConfig::CMD_NOT_FOUND;
  }
  
  // Lock door
  doorController->closeDoor(user_id);
  sendResponseWithDesc(SMS_MSG_DOOR_LOCK_CMD_ACCEPTED);
  return SystemConfig::CMD_EXECUTED;
}

uint8_t SMSCommandParser::apiAddUser() {
  if (received_mobile_number_index != SystemConfig::MASTER_USER_INDEX) {
    sendResponseWithDesc(SMS_MSG_NO_ACCESS_ALLOWED);
    return SystemConfig::CMD_NOT_FOUND;
  }
  
  if (para_count < 3) {
    sendResponseWithDesc(SMS_MSG_PARA_MISSING);
    return SystemConfig::CMD_NOT_FOUND;
  }
  
  uint8_t user_id = parseUserID(para[0], para_len[0]);
  if (user_id == 0 || user_id > SystemConfig::MAX_NUM_OF_USERS || user_id == SystemConfig::MASTER_USER_ID) {
    sendResponseWithDesc(SMS_MSG_PARA_INVALID);
    return SystemConfig::CMD_NOT_FOUND;
  }
  
  if (para_len[1] != SystemConfig::MOBILE_NUMBER_LENGTH) {
    sendResponseWithDesc(SMS_MSG_PARA_INVALID);
    return SystemConfig::CMD_NOT_FOUND;
  }
  
  if (para_len[2] < SystemConfig::MIN_PASSWORD_LEN || para_len[2] > SystemConfig::MAX_PASSWORD_LEN) {
    sendResponseWithDesc(SMS_MSG_PW_LENGH_IS_NOT_IN_LIMIT);
    return SystemConfig::CMD_NOT_FOUND;
  }
  
  char mobile[SystemConfig::MOBILE_NUMBER_LENGTH + 1];
  memset(mobile, 0, sizeof(mobile));
  copyArray(para[1], mobile, SystemConfig::MOBILE_NUMBER_LENGTH);
  
  char password[SystemConfig::PASSWORD_STORE_COUNT];
  memset(password, 0, sizeof(password));
  copyArray(para[2], password, para_len[2]);
  
  ErrorCode err = userManager->addUser(user_id, mobile, password, para_len[2]);
  if (err == ErrorCode::SUCCESS) {
    sendResponseWithDesc(SMS_MSG_CMD_EXECUTED);
    return SystemConfig::CMD_EXECUTED;
  } else if (err == ErrorCode::USER_ALREADY_EXISTS) {
    sendResponseWithDesc(SMS_MSG_USER_ALREADY_EXISTS);
    return SystemConfig::CMD_NOT_FOUND;
  } else {
    sendResponseWithDesc(SMS_MSG_PARA_INVALID);
    return SystemConfig::CMD_NOT_FOUND;
  }
}

uint8_t SMSCommandParser::apiRemoveUser() {
  if (received_mobile_number_index != SystemConfig::MASTER_USER_INDEX) {
    sendResponseWithDesc(SMS_MSG_NO_ACCESS_ALLOWED);
    return SystemConfig::CMD_NOT_FOUND;
  }
  
  if (para_count < 1) {
    sendResponseWithDesc(SMS_MSG_PARA_MISSING);
    return SystemConfig::CMD_NOT_FOUND;
  }
  
  uint8_t user_id = parseUserID(para[0], para_len[0]);
  if (user_id == 0 || user_id > SystemConfig::MAX_NUM_OF_USERS || user_id == SystemConfig::MASTER_USER_ID) {
    sendResponseWithDesc(SMS_MSG_PARA_INVALID);
    return SystemConfig::CMD_NOT_FOUND;
  }
  
  ErrorCode err = userManager->removeUser(user_id);
  if (err == ErrorCode::SUCCESS) {
    sendResponseWithDesc(SMS_MSG_CMD_EXECUTED);
    return SystemConfig::CMD_EXECUTED;
  } else {
    sendResponseWithDesc(SMS_MSG_USER_NOT_FOUND);
    return SystemConfig::CMD_NOT_FOUND;
  }
}

uint8_t SMSCommandParser::apiChangePassword() {
  if (received_mobile_number_index < 0) {
    sendResponseWithDesc(SMS_MSG_USER_NO_REGISTERED);
    return SystemConfig::CMD_NOT_FOUND;
  }
  
  if (para_count < 3) {
    sendResponseWithDesc(SMS_MSG_PARA_MISSING);
    return SystemConfig::CMD_NOT_FOUND;
  }
  
  uint8_t user_id = parseUserID(para[0], para_len[0]);
  if (user_id == 0 || user_id > SystemConfig::MAX_NUM_OF_USERS) {
    sendResponseWithDesc(SMS_MSG_PARA_INVALID);
    return SystemConfig::CMD_NOT_FOUND;
  }
  
  // Check access: master can change any, user can only change own
  if (received_mobile_number_index != SystemConfig::MASTER_USER_INDEX && 
      received_mobile_number_index != (user_id - 1)) {
    sendResponseWithDesc(SMS_MSG_NO_ACCESS_ALLOWED);
    return SystemConfig::CMD_NOT_FOUND;
  }
  
  if (para_len[1] < SystemConfig::MIN_PASSWORD_LEN || para_len[1] > SystemConfig::MAX_PASSWORD_LEN ||
      para_len[2] < SystemConfig::MIN_PASSWORD_LEN || para_len[2] > SystemConfig::MAX_PASSWORD_LEN) {
    sendResponseWithDesc(SMS_MSG_PW_LENGH_IS_NOT_IN_LIMIT);
    return SystemConfig::CMD_NOT_FOUND;
  }
  
  char old_password[SystemConfig::PASSWORD_STORE_COUNT];
  char new_password[SystemConfig::PASSWORD_STORE_COUNT];
  memset(old_password, 0, sizeof(old_password));
  memset(new_password, 0, sizeof(new_password));
  copyArray(para[1], old_password, para_len[1]);
  copyArray(para[2], new_password, para_len[2]);
  
  ErrorCode err = userManager->updateUserPassword(user_id, old_password, para_len[1], 
                                                  new_password, para_len[2]);
  if (err == ErrorCode::SUCCESS) {
    sendResponseWithDesc(SMS_MSG_PW_CHANGED);
    return SystemConfig::CMD_EXECUTED;
  } else {
    sendResponseWithDesc(SMS_MSG_PW_IS_NO_VALID);
    return SystemConfig::CMD_NOT_FOUND;
  }
}

uint8_t SMSCommandParser::apiUpdateTimeSlot() {
  if (received_mobile_number_index != SystemConfig::MASTER_USER_INDEX) {
    sendResponseWithDesc(SMS_MSG_NO_ACCESS_ALLOWED);
    return SystemConfig::CMD_NOT_FOUND;
  }
  
  if (para_count < 5) {
    sendResponseWithDesc(SMS_MSG_PARA_MISSING);
    return SystemConfig::CMD_NOT_FOUND;
  }
  
  uint8_t user_id = parseUserID(para[0], para_len[0]);
  if (user_id == 0 || user_id > SystemConfig::MAX_NUM_OF_USERS) {
    sendResponseWithDesc(SMS_MSG_PARA_INVALID);
    return SystemConfig::CMD_NOT_FOUND;
  }
  
  uint8_t in_hour = parseUserID(para[1], para_len[1]);
  uint8_t in_min = parseUserID(para[2], para_len[2]);
  uint8_t out_hour = parseUserID(para[3], para_len[3]);
  uint8_t out_min = parseUserID(para[4], para_len[4]);
  
  if (in_hour > 23 || in_min > 59 || out_hour > 23 || out_min > 59) {
    sendResponseWithDesc(SMS_MSG_PARA_INVALID);
    return SystemConfig::CMD_NOT_FOUND;
  }
  
  ErrorCode err = userManager->setTimeSlot(user_id, in_hour, in_min, out_hour, out_min);
  if (err == ErrorCode::SUCCESS) {
    sendResponseWithDesc(SMS_MSG_CMD_EXECUTED);
    return SystemConfig::CMD_EXECUTED;
  } else {
    sendResponseWithDesc(SMS_MSG_PARA_INVALID);
    return SystemConfig::CMD_NOT_FOUND;
  }
}

uint8_t SMSCommandParser::apiLostPassword() {
  if (received_mobile_number_index < 0) {
    sendResponseWithDesc(SMS_MSG_USER_NO_REGISTERED);
    return SystemConfig::CMD_NOT_FOUND;
  }
  
  // Only master can request lost password
  if (received_mobile_number_index != SystemConfig::MASTER_USER_INDEX) {
    sendResponseWithDesc(SMS_MSG_NO_ACCESS_ALLOWED);
    return SystemConfig::CMD_NOT_FOUND;
  }
  
  if (para_count < 1) {
    sendResponseWithDesc(SMS_MSG_PARA_MISSING);
    return SystemConfig::CMD_NOT_FOUND;
  }
  
  uint8_t user_id = parseUserID(para[0], para_len[0]);
  if (user_id == 0 || user_id > SystemConfig::MAX_NUM_OF_USERS) {
    sendResponseWithDesc(SMS_MSG_PARA_INVALID);
    return SystemConfig::CMD_NOT_FOUND;
  }
  
  if (!userManager->isPasswordConfigured(user_id)) {
    sendResponseWithDesc(SMS_MSG_PW_IS_NOT_CONFIGURED);
    return SystemConfig::CMD_NOT_FOUND;
  }
  
  // Send password via SMS
  char password[SystemConfig::PASSWORD_STORE_COUNT];
  uint8_t password_len = 0;
  if (userManager->getPassword(user_id, password, &password_len) == ErrorCode::SUCCESS) {
    sendResponse(password);
    return SystemConfig::CMD_EXECUTED;
  }
  
  sendResponseWithDesc(SMS_MSG_PW_IS_NOT_CONFIGURED);
  return SystemConfig::CMD_NOT_FOUND;
}

uint8_t SMSCommandParser::apiFactoryReset() {
  if (received_mobile_number_index != SystemConfig::MASTER_USER_INDEX) {
    sendResponseWithDesc(SMS_MSG_NO_ACCESS_ALLOWED);
    return SystemConfig::CMD_NOT_FOUND;
  }
  
  if (para_count < 1) {
    sendResponseWithDesc(SMS_MSG_PARA_MISSING);
    return SystemConfig::CMD_NOT_FOUND;
  }
  
  // Verify master reset password
  const char* master_pw = SystemConfig::getMasterResetPassword();
  if (para_len[0] != SystemConfig::MASTER_PW_LEN) {
    sendResponseWithDesc(SMS_MSG_PW_IS_NO_VALID);
    return SystemConfig::CMD_NOT_FOUND;
  }
  
  bool match = true;
  for (uint8_t i = 0; i < SystemConfig::MASTER_PW_LEN; i++) {
    if (para[0][i] != pgm_read_byte(master_pw + i)) {
      match = false;
      break;
    }
  }
  
  if (!match) {
    sendResponseWithDesc(SMS_MSG_PW_IS_NO_VALID);
    return SystemConfig::CMD_NOT_FOUND;
  }
  
  // Factory reset - clear all users except master
  for (uint8_t i = 1; i < SystemConfig::MAX_NUM_OF_USERS; i++) {
    userManager->removeUser(i + 1);
  }
  
  sendResponseWithDesc(SMS_MSG_CMD_EXECUTED);
  return SystemConfig::CMD_EXECUTED;
}

uint8_t SMSCommandParser::executeCommand(const char* cmd_str, uint8_t para_cnt, 
                                         char para_arr[][SystemConfig::MAX_PARA_LEN], 
                                         uint8_t para_lengths[]) {
  // Copy parameters
  para_count = para_cnt;
  for (uint8_t i = 0; i < para_count && i < SystemConfig::MAX_PARAMETER; i++) {
    para_len[i] = para_lengths[i];
    copyArray(para_arr[i], para[i], para_lengths[i]);
  }
  
  // Execute based on command
  if (strcmp(cmd_str, "UNLOCK") == 0) {
    return apiUnlockDoor();
  } else if (strcmp(cmd_str, "LOCK") == 0) {
    return apiLockDoor();
  } else if (strcmp(cmd_str, "ADD_USER") == 0) {
    return apiAddUser();
  } else if (strcmp(cmd_str, "REMOVE_USER") == 0) {
    return apiRemoveUser();
  } else if (strcmp(cmd_str, "CHANGE_PASSWORD") == 0) {
    return apiChangePassword();
  } else if (strcmp(cmd_str, "UPDATE_TIME_SLOT") == 0 || strcmp(cmd_str, "TIME_SLOT") == 0) {
    return apiUpdateTimeSlot();
  } else if (strcmp(cmd_str, "LOST_PASSWORD") == 0) {
    return apiLostPassword();
  } else if (strcmp(cmd_str, "FACTORY_RESET") == 0) {
    return apiFactoryReset();
  }
  
  return SystemConfig::CMD_NOT_FOUND;
}

void SMSCommandParser::task() {
  if (gsmSerial == nullptr) {
    return;
  }
  
  // Read serial data
  uint16_t len = readSerialToBuffer(*gsmSerial, serial_buffer, SystemConfig::SERIAL_BUFFER_SIZE);
  if (len == 0) {
    return;
  }
  
  // Parse command
  if (!parseCommand(serial_buffer, len)) {
    return;
  }
  
  // Execute command
  executeCommand(cmd, para_count, para, para_len);
}

int8_t SMSCommandParser::getReceivedMobileNumberIndex() {
  return received_mobile_number_index;
}

