#include "UIStateMachine.h"
#include "MainSystem.h"
#include "UserManager.h"
#include "HolidayManager.h"
#include "FingerprintManager.h"
#include "DoorController.h"
#include "RTCHandler.h"
#include "BuzzerController.h"
#include "BatteryMonitor.h"
#include "AlarmManager.h"
#include "pitches.h"
#include <HardwareSerial.h>
#include <avr/pgmspace.h>
#include <Arduino.h>

// PROGMEM data
const char num_to_alpha[10][3] PROGMEM = {
  {'Y', 'Z', '\0'},  // 0
  {'A', 'B', 'C'},   // 1
  {'D', 'E', 'F'},   // 2
  {'G', 'H', 'I'},   // 3
  {'J', 'K', 'L'},   // 4
  {'M', 'N', '0'},   // 5
  {'P', 'Q', 'R'},   // 6
  {'S', 'T', '\0'},  // 7
  {'U', 'V', '\0'},  // 8
  {'W', 'X', '\0'}   // 9
};

const uint16_t melody[] PROGMEM = {
  NOTE_C4, NOTE_G3, NOTE_G3, NOTE_A3, NOTE_G3, 0, NOTE_B3, NOTE_C4
};

// Private helper variables
static char prev_key = '\0';
static uint8_t times_prssd = 0;
static char get_character = '\0';
static unsigned long prev_rels_time_for_alpha = 0;

// Constructor
UIStateMachine::UIStateMachine(LiquidCrystal* lcdInstance, Adafruit_Keypad* keypadInstance, MainSystem* mainSys)
  : lcd(lcdInstance),
    keypad(keypadInstance),
    mainSystem(mainSys),
    currentState(MAIN),
    isDisplayed(false),
    isNum(true),
    key('\0'),
    isNewKey(false),
    passLength(0),
    firstUserVerified(0),
    userBioAuthFailCount(0),
    currentUserID(0),
    displayOnTimer(0),
    pressTime(0),
    releaseTime(0),
    timeDifference(0),
    lcdState(LCD_STATE_ON),
    holidayMenuState(HOLIDAY_MENU_MAIN),
    holidayInputDate(0),
    holidayInputMonth(0),
    holidayInputYear(0),
    holidayInputCounter(0),
    holidayViewIndex(0),
    holidayIsRemoveMode(false),
    dateTimeLen(0),
    dateTimeCursorIndex(0),
    inputMobileNumberLength(0),
    inputMobileNumberCount(0),
    userIDInputLength(0),
    userIDInputScreenType(ADD_USER),
    tempUserID(0),
    buzzerCounter(0),
    inputBuzzerTimeout(0),
    otpLength(0),
    otpNotMatched(false),
    doorOpened(false),
    doorClosed(false),
    doorOpenTime(0),
    alphaCounter(0)
{
  memset(password, '\0', sizeof(password));
  memset(inputMobileNumber, '\0', sizeof(inputMobileNumber));
  memset(userIDInput, '\0', sizeof(userIDInput));
  memset(dateTime, 0, sizeof(dateTime));
  memset(otp, 0, sizeof(otp));
  memset(generatedOTP, 0, sizeof(generatedOTP));
  memset(charGeneratedOTP, '\0', sizeof(charGeneratedOTP));
}

// Destructor
UIStateMachine::~UIStateMachine() {
  // Cleanup if needed
}

// Initialize
bool UIStateMachine::initialize() {
  if (lcd == nullptr || keypad == nullptr || mainSystem == nullptr) {
    return false;
  }
  
  // Initialize LCD
  lcdInitScreen();
  
  // Initialize keypad
  keypad->begin();
  
  // Set initial state
  currentState = MAIN;
  isDisplayed = false;
  
  return true;
}

// Main task function
void UIStateMachine::task() {
  // Handle keypad input
  handleKeypadInput();
  
  // Update display based on current state
  updateDisplay();
  
  // Handle timeout
  if (currentState == MAIN) {
    if (millis() - displayOnTimer > SystemConfig::DISPLAY_ON_TIMEOUT) {
      passLength = 0;
      lcdPowerOff();
      userBioAuthFailCount = 0;
      firstUserVerified = 0;
    }
  }
}

// Get current state
UIStateMachine::DisplayState UIStateMachine::getCurrentState() {
  return currentState;
}

// Set state
void UIStateMachine::setState(DisplayState newState) {
  currentState = newState;
  isDisplayed = false;
}

// Get first user verified
int8_t UIStateMachine::getFirstUserVerified() {
  return firstUserVerified;
}

// Set first user verified
void UIStateMachine::setFirstUserVerified(int8_t value) {
  firstUserVerified = value;
}

// Get user bio auth fail count
uint8_t UIStateMachine::getUserBioAuthFailCount() {
  return userBioAuthFailCount;
}

// Reset user bio auth fail count
void UIStateMachine::resetUserBioAuthFailCount() {
  userBioAuthFailCount = 0;
}

// Get current user ID
uint8_t UIStateMachine::getCurrentUserID() {
  return currentUserID;
}

// Set current user ID
void UIStateMachine::setCurrentUserID(uint8_t userID) {
  currentUserID = userID;
}

// Handle keypad input
void UIStateMachine::handleKeypadInput() {
  while (keypad->available()) {
    keypadEvent e = keypad->read();
    
    if (e.bit.EVENT == KEY_JUST_PRESSED) {
      pressTime = millis();
      if (lcdState == LCD_STATE_ON) {
        // Play a short beep for keypress (use tone() directly like original code)
        tone(SystemConfig::BUZZER_PIN, pgm_read_word(&melody[0]), 200);
        delay(100);
        noTone(SystemConfig::BUZZER_PIN);
      }
    }
    else if (e.bit.EVENT == KEY_JUST_RELEASED) {
      timeDifference = millis() - pressTime;
      
      if (timeDifference < SystemConfig::KEY_DEBOUNCE_DELAY) {
        continue;  // Debounce: ignore very short presses
      }
      
      key = (char)e.bit.KEY;
      releaseTime = millis();
      displayOnTimer = millis();
      
      switch (key) {
        case KEY_POWER:
          // Lock the safe and turn off display
          break;
        case KEY_MUTE: {
          // Toggle mute setting
          BuzzerController* buzzer = mainSystem->getBuzzerController();
          if (buzzer != nullptr) {
            buzzer->turnOff();
          }
          doorOpenTime = millis();
          break;
        }
        case KEY_LOCK: {
          // Lock the door
          isDisplayed = false;
          DoorController* door = mainSystem->getDoorController();
          if (door != nullptr) {
            door->closeDoor(currentUserID);
          }
          setState(LOCK_DOOR_STATE);
          break;
        }
        default:
          if (lcdState == LCD_STATE_ON) {
            isNewKey = true;
            break;
          }
      }
    }
  }
}

// Check if new index (for alpha input)
bool UIStateMachine::isNewIndex() {
  if (!isNum && key >= '0' && key <= '9') {
    uint8_t temp_key = uint8_t(key) - 48;
    
    if (prev_key != key || (releaseTime - prev_rels_time_for_alpha) > 400) {
      prev_rels_time_for_alpha = releaseTime;
      times_prssd = 0;
      prev_key = key;
      get_character = pgm_read_byte(&num_to_alpha[temp_key][times_prssd]);
      return true;
    }
    else if ((releaseTime - prev_rels_time_for_alpha) < 400) {
      prev_rels_time_for_alpha = releaseTime;
      times_prssd++;
      
      if (temp_key > 0 && temp_key <= 6) {
        get_character = pgm_read_byte(&num_to_alpha[temp_key][times_prssd % 3]);
      }
      else {
        uint8_t max_chars = (temp_key == 0) ? 2 : 2;
        get_character = pgm_read_byte(&num_to_alpha[temp_key][times_prssd % max_chars]);
      }
      return false;
    }
  }
  
  times_prssd = 0;
  get_character = key;
  return true;
}

// Get pressed character
char UIStateMachine::getPressedCharacter() {
  return get_character;
}

// LCD initialization
void UIStateMachine::lcdInit() {
  lcd->noDisplay();
  lcd->begin(16, 2);
  lcd->display();
  
  BatteryMonitor* battery = mainSystem->getBatteryMonitor();
  uint16_t batteryPercent = (battery != nullptr) ? battery->getBatteryPercentage() : 100;
  
  RTCHandler* rtc = mainSystem->getRTCHandler();
  uint8_t date = 1, month = 1, year = 24, hour = 0, minute = 0;
  if (rtc != nullptr) {
    date = rtc->getDate();
    month = rtc->getMonth();
    year = rtc->getYear();
    hour = rtc->getHour();
    minute = rtc->getMinute();
  }
  
  lcd->setCursor(1, 0);
  lcd->print(F("BMS SAFE ("));
  lcd->print(batteryPercent);
  lcd->print(F("%)"));
  
  lcd->setCursor(0, 1);
  if (date < 10) lcd->print('0');
  lcd->print(date);
  lcd->print('/');
  if (month < 10) lcd->print('0');
  lcd->print(month);
  lcd->print('/');
  lcd->print(year);
  lcd->print(F("  "));
  if (hour < 10) lcd->print('0');
  lcd->print(hour);
  lcd->print(':');
  if (minute < 10) lcd->print('0');
  lcd->print(minute);
  
  delay(500);
  lcd->setCursor(0, 1);
}

// LCD init screen
void UIStateMachine::lcdInitScreen() {
  lcdPowerOn();
  lcd->noDisplay();
  lcd->begin(16, 2);
  lcd->display();
  lcd->setCursor(1, 0);
  lcd->print(F("   BMS SAFE"));
  lcd->setCursor(0, 1);
  lcd->print(F("....WELCOME...."));
  delay(2000);
  lcdInit();
}

// LCD power on
void UIStateMachine::lcdPowerOn() {
  pinMode(SystemConfig::LCD_GND_PIN, OUTPUT);
  pinMode(SystemConfig::LCD_VCC_PIN, OUTPUT);
  digitalWrite(SystemConfig::LCD_GND_PIN, LOW);
  digitalWrite(SystemConfig::LCD_VCC_PIN, HIGH);
  lcdState = LCD_STATE_ON;
  displayOnTimer = millis();
}

// LCD power off
void UIStateMachine::lcdPowerOff() {
  digitalWrite(SystemConfig::LCD_GND_PIN, LOW);
  digitalWrite(SystemConfig::LCD_VCC_PIN, LOW);
  lcdState = !LCD_STATE_ON;
}

// Update display based on current state
void UIStateMachine::updateDisplay() {
  switch (currentState) {
    case MAIN:
      handleMainState();
      break;
    case MASTER_MAIN:
      handleMasterMainState();
      break;
    case MASTER_INPUT_STATE:
      handleMasterInputState();
      break;
    case LOCK_DOOR_STATE:
      handleLockDoorState();
      break;
    case MASTER_ADD_USER:
      handleMasterAddUser();
      break;
    case MASTER_REMOVE_USER:
      handleMasterRemoveUser();
      break;
    case MASTER_PASSWORD:
      handleMasterPassword();
      break;
    case MASTER_DAT_TIM:
      handleMasterDateTime();
      break;
    case INPUT_MOBILE_NUMBER:
      handleInputMobileNumber();
      break;
    case MASTER_ADD_USER_MOBILE_NUMBER:
      handleInputMobileNumber();
      break;
    case HOLIDAY_SCREEN:
      handleHolidayScreen();
      break;
    case BACKUP_SCREEN:
      handleBackupScreen();
      break;
    case BUZZER_SCREEN:
      handleBuzzerScreen();
      break;
    case FINGERPRINT_SCREEN:
      handleFingerprintScreen();
      break;
    case ADD_FINGERPRINT_SCREEN:
      handleAddFingerprintScreen();
      break;
    case USER_ID_INPUT_SCREEN:
      handleUserIDInputScreen();
      break;
    case USER:
      handleUserState();
      break;
    case USER_INPUT_STATE:
      handleUserInputState();
      break;
    case USER_PASSWORD:
      handleUserPassword();
      break;
    case USER_LOCK_DOOR_STATE:
      handleUserLockDoorState();
      break;
    default:
      break;
  }
}

// Continue with state handlers in next part...

// Handle main state (password entry)
void UIStateMachine::handleMainState() {
  passwordInputFSM();
}

// Handle master main state
void UIStateMachine::handleMasterMainState() {
  DoorController* door = mainSystem->getDoorController();
  if (door == nullptr) return;
  
  if (!isDisplayed) {
    if (door->isDoorOpening()) {
      lcd->clear();
      lcd->setCursor(0, 0);
      lcd->print(F("OPENING DOOR "));
      lcd->print(door->getDoorOpenCount());
      isDisplayed = true;
    }
    else if (door->isDoorOpen()) {
      lcd->clear();
      lcd->setCursor(0, 0);
      lcd->print(F("DOOR OPENED  "));
      lcd->print(door->getDoorOpenCount());
      isDisplayed = true;
      doorOpenTime = millis();
      setState(MASTER_INPUT_STATE);
      return;
    }
  }
  
  if (!door->isDoorOpening() && door->isDoorOpen()) {
    isDisplayed = false;
  }
}

// Handle master input state (master menu)
void UIStateMachine::handleMasterInputState() {
  BuzzerController* buzzer = mainSystem->getBuzzerController();
  if (buzzer != nullptr) {
    uint16_t buzzerTimeout = 10; // Get from config
    uint32_t applicableTimeout = buzzerTimeout * 60000UL;
    if (millis() - doorOpenTime > applicableTimeout) {
      doorOpenTime = millis();
      buzzer->turnOn();
    }
  }
  
  if (!isDisplayed) {
    isDisplayed = true;
    lcd->clear();
    lcd->setCursor(0, 0);
    lcd->print(F("MASTER SCREEN"));
  }
  
  if (isNewKey) {
    isNewKey = false;
    switch (key) {
      case '1':
        isDisplayed = false;
        setState(MASTER_ADD_USER);
        break;
      case '2':
        isDisplayed = false;
        setState(MASTER_REMOVE_USER);
        break;
      case '3':
        isDisplayed = false;
        setState(MASTER_PASSWORD);
        break;
      case '4':
        isDisplayed = false;
        dateTimeLen = 0;
        memset(dateTime, 0, sizeof(dateTime));
        setState(MASTER_DAT_TIM);
        break;
      case '5':
        isDisplayed = false;
        inputMobileNumberLength = 0;
        setState(INPUT_MOBILE_NUMBER);
        break;
      case '6':
        isDisplayed = false;
        holidayMenuState = HOLIDAY_MENU_MAIN;
        holidayInputDate = 0;
        holidayInputMonth = 0;
        holidayInputYear = 0;
        holidayInputCounter = 0;
        holidayViewIndex = 0;
        holidayIsRemoveMode = false;
        setState(HOLIDAY_SCREEN);
        break;
      case '7':
        isDisplayed = false;
        alphaCounter = 0;
        setState(BACKUP_SCREEN);
        break;
      case '8':
        isDisplayed = false;
        buzzerCounter = 0;
        setState(BUZZER_SCREEN);
        break;
      case '9':
        isDisplayed = false;
        userIDInputScreenType = ADD_FINGERPRINT;
        setState(USER_ID_INPUT_SCREEN);
        break;
      case KEY_CANCEL:
        passLength = 0;
        isDisplayed = false;
        setState(MAIN);
        break;
      case KEY_LOCK: {
        isDisplayed = false;
        DoorController* door = mainSystem->getDoorController();
        if (door != nullptr) {
          door->closeDoor(currentUserID);
        }
        setState(LOCK_DOOR_STATE);
        break;
      }
      default:
        break;
    }
  }
}

// Handle lock door state
void UIStateMachine::handleLockDoorState() {
  DoorController* door = mainSystem->getDoorController();
  if (door == nullptr) return;
  
  if (!isDisplayed) {
    if (door->isDoorClosing()) {
      lcd->clear();
      lcd->setCursor(0, 0);
      lcd->print(F("CLOSING DOOR "));
      lcd->print(door->getDoorOpenCount());
      isDisplayed = true;
    }
    else if (door->isDoorClosed()) {
      lcd->clear();
      lcd->setCursor(0, 0);
      lcd->print(F("DOOR CLOSED  "));
      lcd->print(door->getDoorOpenCount());
      isDisplayed = false;
      delay(3000);
      lcdPowerOff();
      setState(MAIN);
      return;
    }
  }
  
  if (!door->isDoorClosing() && door->isDoorClosed()) {
    isDisplayed = false;
  }
}

// Password input FSM
void UIStateMachine::passwordInputFSM() {
  FingerprintManager* fingerprint = mainSystem->getFingerprintManager();
  if (fingerprint != nullptr) {
    // Handle fingerprint authentication
    // TODO: Implement fingerprint FSM
  }
  
  if (!isDisplayed) {
    isDisplayed = true;
    lcd->clear();
    lcd->setCursor(0, 0);
    
    switch (currentState) {
      case MAIN:
        lcd->print(F("PASSWORD:"));
        break;
      case MASTER_PASSWORD:
        lcd->print(F("MASTER PW:"));
        break;
      case USER_PASSWORD:
        lcd->setCursor(0, 0);
        lcd->print(F("USER-"));
        if (currentUserID < 10) lcd->print('0');
        lcd->print(currentUserID);
        lcd->print(F(" PW "));
        break;
      default:
        break;
    }
    
    lcd->setCursor(11, 0);
    if (isNum) {
      lcd->print(F("NUM"));
    } else {
      lcd->print(F("ALPHA"));
    }
    
    for (uint8_t i = 0; i < passLength; i++) {
      lcd->setCursor(i, 1);
      lcd->print('*');
    }
  }
  
  if (isNewKey) {
    isNewKey = false;
    switch (key) {
      case KEY_ALPHA_NUM:
        isNum = !isNum;
        isDisplayed = false;
        break;
      case KEY_CANCEL:
        isDisplayed = false;
        if (passLength == 0) {
          if (currentState == MASTER_PASSWORD) {
            setState(MASTER_INPUT_STATE);
          } else if (currentState == USER_PASSWORD) {
            setState(USER);
          } else {
            setState(MAIN);
          }
        }
        resetPasswordInput();
        firstUserVerified = 0;
        userBioAuthFailCount = 0;
        break;
      case KEY_ENTER:
        if (passLength >= SystemConfig::MIN_PASSWORD_LEN && 
            passLength <= SystemConfig::MAX_PASSWORD_LEN) {
          isDisplayed = false;
          switch (currentState) {
            case MAIN:
              verifyDualPassword();
              // Check for gun point activation
              if (timeDifference > SystemConfig::GUN_POINT_PRESS_TIMEOUT) {
                // Gun point activated
                AlarmManager* alarm = mainSystem->getAlarmManager();
                if (alarm != nullptr) {
                  // TODO: Trigger gun point alarm
                }
                generateRandomOTP();
                lcdPowerOff();
                lcdPowerOn();
              }
              break;
            case MASTER_PASSWORD:
              {
                UserManager* userMgr = mainSystem->getUserManager();
                if (userMgr != nullptr) {
                  // Update master password
                  // TODO: Implement password update
                }
                isDisplayed = true;
                lcd->clear();
                lcd->setCursor(0, 0);
                lcd->print(F("MASTER PW:"));
                lcd->setCursor(0, 1);
                lcd->print(F("PW UPDATED!!"));
                delay(1000);
                resetPasswordInput();
                isDisplayed = false;
                setState(MASTER_MAIN);
              }
              break;
            case USER_PASSWORD:
              {
                UserManager* userMgr = mainSystem->getUserManager();
                if (userMgr != nullptr) {
                  // Update user password
                  // TODO: Implement password update
                }
                isDisplayed = true;
                lcd->setCursor(0, 1);
                lcd->print(F("PW UPDATED!!"));
                delay(1000);
                resetPasswordInput();
                isDisplayed = false;
                setState(USER_INPUT_STATE);
              }
              break;
            default:
              break;
          }
        } else {
          lcd->clear();
          lcd->setCursor(0, 0);
          lcd->print(F("Password Short!"));
          isDisplayed = false;
          delay(1000);
        }
        break;
      default:
        {
          bool newIndex = isNewIndex();
          if (newIndex) {
            passLength++;
          }
          lcd->setCursor(passLength - 1, 1);
          lcd->print(getPressedCharacter());
          delay(400);
          lcd->setCursor(passLength - 1, 1);
          lcd->print('*');
          
          if (newIndex) {
            password[passLength - 1] = getPressedCharacter();
          } else {
            password[passLength - 1] = getPressedCharacter();
          }
        }
        break;
    }
  }
}

// Verify dual password
bool UIStateMachine::verifyDualPassword() {
  uint8_t userIDLength = 0;
  uint8_t parsedUserID = parseUserIDFromPassword(password, passLength, &userIDLength);
  
  if (parsedUserID == 0) {
    displayError(PSTR("Invld Password!!"));
    delay(1000);
    setState(MAIN);
    resetPasswordInput();
    firstUserVerified = 0;
    return false;
  }
  
  UserManager* userMgr = mainSystem->getUserManager();
  if (userMgr == nullptr) {
    return false;
  }
  
  bool isValid = userMgr->validatePassword(parsedUserID, &password[userIDLength], 
                                           passLength - userIDLength);
  
  if ((firstUserVerified || parsedUserID == SystemConfig::MASTER_USER_ID) && isValid) {
    if (firstUserVerified) {
      if (parsedUserID == SystemConfig::MASTER_USER_ID) {
        // Master accessing master menu
        currentUserID = parsedUserID;
        setState(MASTER_INPUT_STATE);
        isDisplayed = false;
        userBioAuthFailCount = 0;
        firstUserVerified = 0;
      } else {
        // Regular user accessing door
        currentUserID = parsedUserID;
        checkIfDoorAccessIsAllowed(parsedUserID);
        firstUserVerified = 0;
        userBioAuthFailCount = 0;
      }
    } else if (parsedUserID == SystemConfig::MASTER_USER_ID) {
      // First master authentication
      firstUserVerified = 1;
      isDisplayed = true;
      lcd->clear();
      lcd->setCursor(0, 0);
      lcd->print(F("USER PASS/BIO :"));
      passLength = 0;
      memset(password, '\0', sizeof(password));
      currentUserID = 0;
      userBioAuthFailCount = 0;
    }
    return true;
  } else {
    displayError(PSTR("Invld Password!!"));
    
    if (firstUserVerified == 1) {
      userBioAuthFailCount++;
      if (userBioAuthFailCount >= 1) {
        // Send alert
        AlarmManager* alarm = mainSystem->getAlarmManager();
        if (alarm != nullptr) {
          // TODO: Send auth fail alert
        }
        userBioAuthFailCount = 0;
        setState(MAIN);
        resetPasswordInput();
        firstUserVerified = 0;
      } else {
        // Retry on same screen
        lcd->clear();
        lcd->setCursor(0, 0);
        lcd->print(F("USER PASS/BIO :"));
        passLength = 0;
        memset(password, '\0', sizeof(password));
        isDisplayed = true;
      }
    } else {
      setState(MAIN);
      resetPasswordInput();
      firstUserVerified = 0;
    }
    return false;
  }
}

// Parse user ID from password
uint8_t UIStateMachine::parseUserIDFromPassword(char* pass, uint8_t passLen, uint8_t* userIDLen) {
  if (passLen >= 2 && pass[0] >= '1' && pass[0] <= '2' && pass[1] >= '0' && pass[1] <= '8') {
    uint8_t userID = (pass[0] - '0') * 10 + (pass[1] - '0');
    if (userID >= 1 && userID <= SystemConfig::MAX_NUM_OF_USERS) {
      *userIDLen = 2;
      return userID;
    }
  }
  
  if (passLen >= 2 && pass[0] == '0' && pass[1] >= '1' && pass[1] <= '9') {
    uint8_t userID = pass[1] - '0';
    if (userID >= 1 && userID <= SystemConfig::MAX_NUM_OF_USERS) {
      *userIDLen = 2;
      return userID;
    }
  }
  
  if (passLen >= 1 && pass[0] >= '1' && pass[0] <= '9') {
    uint8_t userID = pass[0] - '0';
    if (userID >= 1 && userID <= SystemConfig::MAX_NUM_OF_USERS) {
      *userIDLen = 1;
      return userID;
    }
  }
  
  return 0;
}

// Check if door access is allowed
void UIStateMachine::checkIfDoorAccessIsAllowed(uint8_t userID) {
  isDisplayed = false;
  resetPasswordInput();
  
  DoorController* door = mainSystem->getDoorController();
  if (door == nullptr) return;
  
  ErrorCode accessResult = door->checkDoorAccess(userID);
  
  if (accessResult == ErrorCode::SUCCESS) {
    currentUserID = userID;
    if (userID == SystemConfig::MASTER_USER_ID) {
      setState(MASTER_MAIN);
      door->openDoor(userID);
    } else {
      setState(USER);
      door->openDoor(userID);
    }
  } else {
    displayError(PSTR("Access Denied!"));
    delay(2000);
    setState(MAIN);
    resetPasswordInput();
  }
}

// Reset password input
void UIStateMachine::resetPasswordInput() {
  passLength = 0;
  memset(password, '\0', sizeof(password));
  currentUserID = 0;
}

// Generate random OTP
void UIStateMachine::generateRandomOTP() {
  RTCHandler* rtc = mainSystem->getRTCHandler();
  if (rtc == nullptr) return;
  
  uint8_t minute = rtc->getMinute();
  uint8_t hour = rtc->getHour();
  
  uint16_t temp = (minute + (hour * 10)) / 10;
  if (temp > 10) temp = temp / 10;
  if (temp > 10) temp = temp / 10;
  
  for (uint8_t i = 0; i < 6; i++) {
    generatedOTP[i] = random(temp, 9);
    charGeneratedOTP[i] = generatedOTP[i] + '0';
  }
  charGeneratedOTP[6] = '\0';
}

// Display error
void UIStateMachine::displayError(const char* errorMsg) {
  lcd->clear();
  lcd->setCursor(0, 0);
  lcd->print((__FlashStringHelper*)errorMsg);
  isDisplayed = false;
}

// Display success
void UIStateMachine::displaySuccess(const char* successMsg) {
  lcd->clear();
  lcd->setCursor(0, 0);
  lcd->print((__FlashStringHelper*)successMsg);
  isDisplayed = false;
}

// Placeholder implementations for remaining handlers
void UIStateMachine::handleMasterAddUser() {
  userIDInputScreenType = ADD_USER;
  isDisplayed = false;
  setState(USER_ID_INPUT_SCREEN);
}

void UIStateMachine::handleMasterRemoveUser() {
  userIDInputScreenType = REMOVE_USER;
  isDisplayed = false;
  setState(USER_ID_INPUT_SCREEN);
}

void UIStateMachine::handleMasterPassword() {
  passwordInputFSM();
}

void UIStateMachine::handleMasterDateTime() {
  dateTimeInputFSM();
}

void UIStateMachine::handleInputMobileNumber() {
  mobileNumberInputFSM(currentUserID);
}

void UIStateMachine::handleHolidayScreen() {
  holidayMenuFSM();
}

void UIStateMachine::handleBackupScreen() {
  backupScreenFSM();
}

void UIStateMachine::handleBuzzerScreen() {
  buzzerInputFSM();
}

void UIStateMachine::handleFingerprintScreen() {
  userIDInputScreenType = ADD_FINGERPRINT;
  isDisplayed = false;
  setState(USER_ID_INPUT_SCREEN);
}

void UIStateMachine::handleAddFingerprintScreen() {
  // TODO: Implement fingerprint enrollment
  FingerprintManager* fingerprint = mainSystem->getFingerprintManager();
  if (fingerprint != nullptr && tempUserID > 0) {
    ErrorCode result = fingerprint->enrollFingerprint(tempUserID);
    if (result == ErrorCode::SUCCESS) {
      isDisplayed = false;
      setState(MASTER_INPUT_STATE);
    } else {
      isDisplayed = false;
      setState(FINGERPRINT_SCREEN);
    }
  }
}

void UIStateMachine::handleUserIDInputScreen() {
  userIDInputFSM();
}

void UIStateMachine::handleUserState() {
  DoorController* door = mainSystem->getDoorController();
  if (door == nullptr) return;
  
  if (!isDisplayed) {
    if (door->isDoorOpening()) {
      lcd->clear();
      lcd->setCursor(0, 0);
      lcd->print(F("OPENING DOOR.."));
      lcd->print(door->getDoorOpenCount());
      isDisplayed = true;
    }
    else if (door->isDoorOpen()) {
      lcd->clear();
      lcd->setCursor(0, 0);
      lcd->print(F("DOOR OPENED  "));
      lcd->print(door->getDoorOpenCount());
      isDisplayed = true;
      setState(USER_INPUT_STATE);
      return;
    }
  }
  
  if (!door->isDoorOpening() && door->isDoorOpen()) {
    isDisplayed = false;
  }
}

void UIStateMachine::handleUserInputState() {
  DoorController* door = mainSystem->getDoorController();
  if (!isDisplayed) {
    isDisplayed = true;
    lcd->clear();
    lcd->setCursor(0, 0);
    lcd->print(F("OPENING DOOR "));
    if (door != nullptr) {
      lcd->print(door->getDoorOpenCount());
      door->openDoor(currentUserID);
    }
  }
  
  if (isNewKey) {
    isNewKey = false;
    switch (key) {
      case KEY_LOCK:
        if (door != nullptr) {
          door->closeDoor(currentUserID);
        }
        lcd->clear();
        lcd->setCursor(0, 0);
        lcd->print(F("CLOSING DOOR ..."));
        isDisplayed = false;
        setState(LOCK_DOOR_STATE);
        break;
      case '1':
        isDisplayed = false;
        setState(USER_PASSWORD);
        break;
      default:
        break;
    }
  }
}

void UIStateMachine::handleUserPassword() {
  passwordInputFSM();
}

void UIStateMachine::handleUserLockDoorState() {
  handleLockDoorState();
}

// Stub implementations for complex FSMs (to be expanded)
void UIStateMachine::dateTimeInputFSM() {
  // TODO: Implement date/time input FSM
}

void UIStateMachine::mobileNumberInputFSM(uint8_t targetUserID) {
  // TODO: Implement mobile number input FSM
  (void)targetUserID;  // Suppress unused parameter warning
}

void UIStateMachine::userIDInputFSM() {
  // TODO: Implement user ID input FSM
}

void UIStateMachine::buzzerInputFSM() {
  // TODO: Implement buzzer input FSM
}

void UIStateMachine::holidayMenuFSM() {
  // TODO: Implement holiday menu FSM
}

void UIStateMachine::backupScreenFSM() {
  // TODO: Implement backup screen FSM
}

void UIStateMachine::otpInputFSM() {
  // TODO: Implement OTP input FSM
}

void UIStateMachine::resetHolidayInput() {
  holidayInputDate = 0;
  holidayInputMonth = 0;
  holidayInputYear = 0;
  holidayInputCounter = 0;
}

void UIStateMachine::resetMobileNumberInput() {
  inputMobileNumberLength = 0;
  inputMobileNumberCount = 0;
  memset(inputMobileNumber, '\0', sizeof(inputMobileNumber));
}

void UIStateMachine::resetUserIDInput() {
  userIDInputLength = 0;
  memset(userIDInput, '\0', sizeof(userIDInput));
  tempUserID = 0;
}

void UIStateMachine::resetDateTimeInput() {
  dateTimeLen = 0;
  dateTimeCursorIndex = 0;
  memset(dateTime, 0, sizeof(dateTime));
}

bool UIStateMachine::isPasswordValid(uint8_t userID, char* pass, uint8_t passLen) {
  UserManager* userMgr = mainSystem->getUserManager();
  if (userMgr == nullptr) return false;
  return userMgr->validatePassword(userID, pass, passLen);
}

bool UIStateMachine::verifyPassword() {
  // Legacy single-stage password verification
  return verifyDualPassword();
}

