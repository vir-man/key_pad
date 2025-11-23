#ifndef AUTHENTICATION_MANAGER_H
#define AUTHENTICATION_MANAGER_H

#include "SystemConfig.h"
#include "ErrorCodes.h"
#include "UserManager.h"
#include "HolidayManager.h"

// Forward declarations
class DoorController;
class RTCHandler;

/**
 * Authentication Manager Class
 * Handles dual authentication (master + user)
 * Supports both password and fingerprint authentication
 */
class AuthenticationManager {
private:
  UserManager* userManager;
  HolidayManager* holidayManager;
  DoorController* doorController;
  RTCHandler* rtcHandler;
  
  // Authentication state
  bool first_user_verified;  // Master verified, waiting for user
  uint8_t user_bio_auth_fail_count;
  
  // SMS dual authentication state
  bool sms_master_verified;
  unsigned long sms_master_verified_time;
  
  // Helper functions
  bool checkTimeSlotAndHoliday(uint8_t user_id);
  void resetAuthState();
  
public:
  AuthenticationManager(UserManager* userMgr, HolidayManager* holidayMgr);
  bool initialize(DoorController* doorCtrl, RTCHandler* rtc);
  
  // Password authentication
  ErrorCode verifyMasterPassword(const char* password, uint8_t password_len);
  ErrorCode verifyUserPassword(uint8_t user_id, const char* password, uint8_t password_len);
  ErrorCode verifyDualPassword(const char* password, uint8_t password_len, uint8_t* authenticated_user_id);
  
  // Fingerprint authentication
  ErrorCode verifyMasterFingerprint(int8_t fingerprint_id);
  ErrorCode verifyUserFingerprint(int8_t fingerprint_id, uint8_t* authenticated_user_id);
  
  // SMS authentication (two-step)
  ErrorCode smsVerifyMaster(uint8_t user_id, const char* password, uint8_t password_len);
  ErrorCode smsVerifyUser(uint8_t user_id, const char* password, uint8_t password_len);
  
  // State management
  bool isMasterVerified();
  bool isFirstUserVerified();
  void setFirstUserVerified(bool state);
  void resetFailureCount();
  uint8_t getFailureCount();
  void incrementFailureCount();
  
  // Access control
  bool checkAccessAllowed(uint8_t user_id);
  
  // Cleanup
  void checkSMSMasterTimeout();
};

#endif // AUTHENTICATION_MANAGER_H

