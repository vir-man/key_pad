#include "GSMHandler.h"
#include <avr/pgmspace.h>
#include <string.h>

// Message texts stored in PROGMEM
const char msg_door_opened[] PROGMEM = "Door Opened!\n";
const char msg_door_closed[] PROGMEM = "Door Closed!\n";
const char msg_auth_failed[] PROGMEM = "Authentication Failed!\n";
const char msg_temp_alarm[] PROGMEM = "Temperature Alarm!\n";
const char msg_vibration_alarm[] PROGMEM = "Vibration Alarm!\n";
const char msg_gun_point[] PROGMEM = "Gun Point Activated!\n";
const char msg_door_unlock_accepted[] PROGMEM = "Door Unlock Command Accepted!";
const char msg_door_lock_accepted[] PROGMEM = "Door Lock Command Accepted!";
const char msg_command_executed[] PROGMEM = "Command Executed";
const char msg_password_changed[] PROGMEM = "Password Changed";
const char msg_user_no_registered[] PROGMEM = "User No Registered";
const char msg_para_missing[] PROGMEM = "Parameters missing";
const char msg_para_invalid[] PROGMEM = "Invalid parameters";
const char msg_pw_not_valid[] PROGMEM = "Password is not valid";
const char msg_no_access[] PROGMEM = "No Access Allowed";
const char msg_pw_length_error[] PROGMEM = "Password length is not in limit";
const char msg_user_already_exists[] PROGMEM = "User already exists";
const char msg_user_not_found[] PROGMEM = "User not found";
const char msg_pw_not_configured[] PROGMEM = "Password is not configured";
const char msg_mobile_not_registered[] PROGMEM = "Mobile number is not registered";

GSMHandler::GSMHandler(HardwareSerial* serial, UserManager* userMgr) 
  : gsmSerial(serial), userManager(userMgr), queue_index(0) {
  memset(type_list, 0, sizeof(type_list));
  memset(message_details, 0, sizeof(message_details));
}

bool GSMHandler::initialize() {
  if (gsmSerial == nullptr) {
    return false;
  }
  
  gsmSerial->begin(SystemConfig::GSM_BAUD_RATE);
  delay(1000);
  
  // Initialize GSM module with AT commands
  gsmSerial->println("AT");
  delay(500);
  
  gsmSerial->println("AT+CMGF=1");  // Text mode
  delay(500);
  
  gsmSerial->println("AT+CNMI=2,2,0,0,0");  // New message indication
  delay(500);
  
  return true;
}

void GSMHandler::sendSMS(const char* number, const char* message) {
  if (gsmSerial == nullptr || number == nullptr || message == nullptr) {
    return;
  }
  
  gsmSerial->print("AT+CMGS=\"");
  gsmSerial->print(number);
  gsmSerial->println("\"");
  delay(500);
  
  gsmSerial->println(message);
  delay(500);
  gsmSerial->println((char)26);  // Ctrl+Z to send
  delay(1000);
}

void GSMHandler::makeCall(const char* number) {
  if (gsmSerial == nullptr || number == nullptr) {
    return;
  }
  
  gsmSerial->print("ATD");
  gsmSerial->print(number);
  gsmSerial->println(";");
  delay(1000);
}

bool GSMHandler::getMobileNumber(uint8_t user_index, char* mobile) {
  if (userManager == nullptr || mobile == nullptr) {
    return false;
  }
  
  uint8_t user_id = user_index + 1;
  if (userManager->getMobileNumber(user_id, mobile) == ErrorCode::SUCCESS) {
    return true;
  }
  
  return false;
}

const char* GSMHandler::getMessageText(uint8_t message_type, uint8_t user_id) {
  switch(message_type) {
    case SystemConfig::OPEN_DOOR_MSG:
      return msg_door_opened;
    case SystemConfig::CLOSE_DOOR_MSG:
      return msg_door_closed;
    case SystemConfig::AUTH_FAIL_MSG:
      return msg_auth_failed;
    case SystemConfig::TEMP_ALARM_MSG:
      return msg_temp_alarm;
    case SystemConfig::VIBRATION_ALARM_MSG:
      return msg_vibration_alarm;
    case SystemConfig::GUN_POINT_MSG:
      return msg_gun_point;
    default:
      return "";
  }
}

ErrorCode GSMHandler::addToQueue(uint8_t message_type, uint8_t user_id) {
  if (queue_index >= SystemConfig::MAX_QUEUE_SIZE) {
    return ErrorCode::GSM_QUEUE_FULL;
  }
  
  type_list[queue_index] = message_type;
  message_details[queue_index] = user_id;
  queue_index++;
  
  return ErrorCode::SUCCESS;
}

void GSMHandler::processQueue() {
  if (queue_index == 0) {
    return;
  }
  
  uint8_t msg_type = type_list[queue_index - 1];
  uint8_t user_id = message_details[queue_index - 1];
  
  char mobile[SystemConfig::MOBILE_NUMBER_LENGTH + 1];
  memset(mobile, 0, sizeof(mobile));
  
  // Get mobile number
  uint8_t user_index = (user_id > 0) ? (user_id - 1) : 0;
  if (!getMobileNumber(user_index, mobile)) {
    queue_index--;
    return;
  }
  
  // Get message text
  const char* msg_text = getMessageText(msg_type, user_id);
  
  // Send SMS
  sendSMS(mobile, msg_text);
  
  // Remove from queue
  queue_index--;
}

ErrorCode GSMHandler::sendSMSDirect(uint8_t user_id, const char* message) {
  if (userManager == nullptr || message == nullptr) {
    return ErrorCode::SYSTEM_NOT_INITIALIZED;
  }
  
  char mobile[SystemConfig::MOBILE_NUMBER_LENGTH + 1];
  memset(mobile, 0, sizeof(mobile));
  
  if (userManager->getMobileNumber(user_id, mobile) != ErrorCode::SUCCESS) {
    return ErrorCode::USER_NOT_FOUND;
  }
  
  sendSMS(mobile, message);
  return ErrorCode::SUCCESS;
}

ErrorCode GSMHandler::sendSMSDirect(const char* mobile, const char* message) {
  if (mobile == nullptr || message == nullptr) {
    return ErrorCode::SMS_INVALID_PARAMETERS;
  }
  
  sendSMS(mobile, message);
  return ErrorCode::SUCCESS;
}

ErrorCode GSMHandler::makeCallDirect(uint8_t user_id) {
  if (userManager == nullptr) {
    return ErrorCode::SYSTEM_NOT_INITIALIZED;
  }
  
  char mobile[SystemConfig::MOBILE_NUMBER_LENGTH + 1];
  memset(mobile, 0, sizeof(mobile));
  
  if (userManager->getMobileNumber(user_id, mobile) != ErrorCode::SUCCESS) {
    return ErrorCode::USER_NOT_FOUND;
  }
  
  makeCall(mobile);
  return ErrorCode::SUCCESS;
}

ErrorCode GSMHandler::makeCallDirect(const char* mobile) {
  if (mobile == nullptr) {
    return ErrorCode::SMS_INVALID_PARAMETERS;
  }
  
  makeCall(mobile);
  return ErrorCode::SUCCESS;
}

void GSMHandler::task() {
  processQueue();
}

void GSMHandler::housekeepingTask() {
  // Check SMS master timeout, etc.
  // This is handled by AuthenticationManager
}

uint8_t GSMHandler::getQueueSize() {
  return queue_index;
}

void GSMHandler::clearQueue() {
  queue_index = 0;
  memset(type_list, 0, sizeof(type_list));
  memset(message_details, 0, sizeof(message_details));
}

bool GSMHandler::isInitialized() {
  return (gsmSerial != nullptr);
}

