#ifndef UI_STATE_MACHINE_H
#define UI_STATE_MACHINE_H

#include "SystemConfig.h"
#include <LiquidCrystal.h>
#include <Adafruit_Keypad.h>
#include <Arduino.h>
#include <avr/pgmspace.h>

// Forward declarations
class MainSystem;
class UserManager;
class HolidayManager;
class FingerprintManager;
class DoorController;
class RTCHandler;
class BuzzerController;

/**
 * UI State Machine Class
 * Manages all LCD display states and user input flows
 * Implements all user flows from README.md
 */
class UIStateMachine {
public:
  // Display Screen States (matching original system)
  enum DisplayState {
    MAIN = 0,
    MASTER_MAIN = 1,
    MASTER_ADD_USER = 2,
    MASTER_REMOVE_USER = 3,
    MASTER_PASSWORD = 4,
    MASTER_DAT_TIM = 5,
    MASTER_BACKUP = 6,
    USER = 7,
    USER_PASSWORD = 8,
    HOLIDAY_SCREEN = 9,
    BACKUP_SCREEN = 10,
    MASTER_INPUT_STATE = 11,
    LOCK_DOOR_STATE = 12,
    INPUT_MOBILE_NUMBER = 13,
    MASTER_ADD_USER_MOBILE_NUMBER = 14,
    USER_INPUT_STATE = 15,
    BUZZER_SCREEN = 16,
    FINGERPRINT_SCREEN = 17,
    ADD_FINGERPRINT_SCREEN = 18,
    USER_ID_INPUT_SCREEN = 19,
    USER_LOCK_DOOR_STATE = 30
  };
  
  // Holiday Menu Sub-states
  enum HolidayMenuState {
    HOLIDAY_MENU_MAIN = 0,
    HOLIDAY_MENU_ADD = 1,
    HOLIDAY_MENU_REMOVE = 2,
    HOLIDAY_MENU_VIEW = 3,
    HOLIDAY_MENU_INPUT_DATE = 4,
    HOLIDAY_MENU_INPUT_MONTH = 5,
    HOLIDAY_MENU_INPUT_YEAR = 6
  };
  
  // User ID Input Screen Types
  enum UserIDInputType {
    ADD_USER = 1,
    REMOVE_USER = 2,
    ADD_FINGERPRINT = 3
  };
  
  // Special Key Definitions
  static constexpr char KEY_CANCEL = '@';
  static constexpr char KEY_ENTER = '#';  // UNLOCK key
  static constexpr char KEY_POWER = '!';
  static constexpr char KEY_LOCK = '*';
  static constexpr char KEY_MUTE = '^';
  static constexpr char KEY_ALPHA_NUM = '&';
  
  // Constructor and Destructor
  UIStateMachine(
    LiquidCrystal* lcd,
    Adafruit_Keypad* keypad,
    MainSystem* mainSystem
  );
  ~UIStateMachine();
  
  // Initialization
  bool initialize();
  
  // Main task function (call in loop)
  void task();
  
  // Keypad input processing (call FIRST in main loop for immediate response)
  void handleKeypadInput();
  
  // State management
  DisplayState getCurrentState();
  void setState(DisplayState newState);
  
  // Password input state tracking
  int8_t getFirstUserVerified();
  void setFirstUserVerified(int8_t value);
  uint8_t getUserBioAuthFailCount();
  void resetUserBioAuthFailCount();
  
  // User ID tracking
  uint8_t getCurrentUserID();
  void setCurrentUserID(uint8_t userID);
  
private:
  // Core components
  LiquidCrystal* lcd;
  Adafruit_Keypad* keypad;
  MainSystem* mainSystem;
  
  // State variables
  DisplayState currentState;
  bool isDisplayed;
  bool isNum;  // NUM or ALPHA input mode
  char key;
  bool isNewKey;
  
  // Password input
  char password[16];
  uint8_t passLength;
  int8_t firstUserVerified;
  uint8_t userBioAuthFailCount;
  uint8_t currentUserID;
  
  // Timing
  unsigned long displayOnTimer;
  unsigned long pressTime;
  unsigned long releaseTime;
  unsigned long timeDifference;
  
  // LCD state
  bool lcdState;
  static constexpr bool LCD_STATE_ON = true;
  
  // Holiday menu state
  HolidayMenuState holidayMenuState;
  uint8_t holidayInputDate;
  uint8_t holidayInputMonth;
  uint8_t holidayInputYear;
  uint8_t holidayInputCounter;
  uint8_t holidayViewIndex;
  bool holidayIsRemoveMode;
  
  // Date/Time input
  uint8_t dateTime[13];
  uint8_t dateTimeLen;
  uint8_t dateTimeCursorIndex;
  
  // Mobile number input
  char inputMobileNumber[11];
  uint8_t inputMobileNumberLength;
  uint8_t inputMobileNumberCount;
  char prevInputMobileNumber[11];  // For confirmation
  bool mobileNumberNotMatched;
  
  // User ID input
  char userIDInput[3];
  uint8_t userIDInputLength;
  UserIDInputType userIDInputScreenType;
  uint8_t tempUserID;
  
  // Buzzer input
  uint16_t buzzerCounter;
  uint16_t inputBuzzerTimeout;
  
  // OTP input (for gun point/alarm deactivation)
  uint8_t otp[6];
  uint8_t otpLength;
  uint8_t generatedOTP[6];
  char charGeneratedOTP[7];
  bool otpNotMatched;
  
  // Door state tracking
  bool doorOpened;
  bool doorClosed;
  unsigned long doorOpenTime;
  
  // Backup screen
  uint16_t alphaCounter;
  
  // Private methods - Keypad handling
  char getPressedCharacter();
  bool isNewIndex();
  
  // Private methods - LCD display management
  void lcdInit();
  void lcdInitScreen();
  void lcdPowerOn();
  void lcdPowerOff();
  void updateDisplay();
  
  // Private methods - State handlers
  void handleMainState();
  void handleMasterMainState();
  void handleMasterInputState();
  void handleLockDoorState();
  void handleMasterAddUser();
  void handleMasterRemoveUser();
  void handleMasterPassword();
  void handleMasterDateTime();
  void handleInputMobileNumber();
  void handleHolidayScreen();
  void handleBackupScreen();
  void handleBuzzerScreen();
  void handleFingerprintScreen();
  void handleAddFingerprintScreen();
  void handleUserIDInputScreen();
  void handleUserState();
  void handleUserInputState();
  void handleUserPassword();
  void handleUserLockDoorState();
  
  // Private methods - Input FSMs
  void passwordInputFSM();
  void dateTimeInputFSM();
  void mobileNumberInputFSM(uint8_t targetUserID);
  void userIDInputFSM();
  void buzzerInputFSM();
  void holidayMenuFSM();
  void backupScreenFSM();
  void otpInputFSM();
  
  // Private methods - Authentication
  bool verifyDualPassword();
  bool verifyPassword();
  uint8_t parseUserIDFromPassword(char* password, uint8_t passLen, uint8_t* userIDLength);
  bool isPasswordValid(uint8_t userID, char* password, uint8_t passLen);
  void checkIfDoorAccessIsAllowed(uint8_t userID);
  
  // Private methods - OTP
  void generateRandomOTP();
  
  // Private methods - Display helpers
  void displayPasswordScreen();
  void displayHolidayMenu();
  void displayHolidayAdd();
  void displayHolidayRemove();
  void displayHolidayView();
  void displayMasterMenu();
  void displayError(const char* errorMsg);
  void displaySuccess(const char* successMsg);
  
  // Helper methods
  void resetPasswordInput();
  void resetHolidayInput();
  void resetMobileNumberInput();
  void resetUserIDInput();
  void resetDateTimeInput();
};

#endif // UI_STATE_MACHINE_H

