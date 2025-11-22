#ifndef GSM_HANDLER_H
#define GSM_HANDLER_H

#include <HardwareSerial.h>
#include "SystemConfig.h"
#include "ErrorCodes.h"
#include "UserManager.h"

/**
 * GSM Handler Class
 * Handles GSM module communication (SIM7600)
 * Manages SMS sending and message queue
 */
class GSMHandler {
private:
  HardwareSerial* gsmSerial;
  UserManager* userManager;
  
  // Message queue
  uint8_t type_list[SystemConfig::MAX_QUEUE_SIZE];
  uint8_t message_details[SystemConfig::MAX_QUEUE_SIZE];
  uint8_t queue_index;
  
  // Helper functions
  void sendSMS(const char* number, const char* message);
  void makeCall(const char* number);
  const char* getMessageText(uint8_t message_type, uint8_t user_id);
  bool getMobileNumber(uint8_t user_index, char* mobile);
  
public:
  GSMHandler(HardwareSerial* serial, UserManager* userMgr);
  bool initialize();
  
  // Queue operations
  ErrorCode addToQueue(uint8_t message_type, uint8_t user_id);
  void processQueue();
  uint8_t getQueueSize();
  void clearQueue();
  
  // Direct operations
  ErrorCode sendSMSDirect(uint8_t user_id, const char* message);
  ErrorCode sendSMSDirect(const char* mobile, const char* message);
  ErrorCode makeCallDirect(uint8_t user_id);
  ErrorCode makeCallDirect(const char* mobile);
  
  // Task function (call in loop)
  void task();
  void housekeepingTask();
  
  // Status
  bool isInitialized();
};

#endif // GSM_HANDLER_H

