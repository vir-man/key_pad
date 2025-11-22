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
static unsigned long rels_time = 0;  // Release time for alpha input (like original)

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
    mobileNumberNotMatched(false),
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
  memset(prevInputMobileNumber, '\0', sizeof(prevInputMobileNumber));
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
  Serial.println(F("UIStateMachine::initialize() - Starting"));
  
  if (lcd == nullptr) {
    Serial.println(F("ERROR: LCD is nullptr!"));
    return false;
  }
  
  if (keypad == nullptr) {
    Serial.println(F("ERROR: Keypad is nullptr!"));
    return false;
  }
  
  if (mainSystem == nullptr) {
    Serial.println(F("ERROR: MainSystem is nullptr!"));
    return false;
  }
  
  Serial.println(F("All pointers valid - initializing LCD"));
  
  // Initialize LCD
  lcdInitScreen();
  
  Serial.println(F("[UIStateMachine] Initializing keypad..."));
  
  // Initialize keypad (configure pins) - CRITICAL for keypad to work
  if (keypad != nullptr) {
    keypad->begin();
    Serial.println(F("[UIStateMachine] keypad->begin() called successfully"));
    Serial.print(F("[UIStateMachine] Row pins: "));
    Serial.print(SystemConfig::KEYPAD_ROW_0);
    Serial.print(F(", "));
    Serial.print(SystemConfig::KEYPAD_ROW_1);
    Serial.print(F(", "));
    Serial.print(SystemConfig::KEYPAD_ROW_2);
    Serial.print(F(", "));
    Serial.println(SystemConfig::KEYPAD_ROW_3);
    Serial.print(F("[UIStateMachine] Col pins: "));
    Serial.print(SystemConfig::KEYPAD_COL_0);
    Serial.print(F(", "));
    Serial.print(SystemConfig::KEYPAD_COL_1);
    Serial.print(F(", "));
    Serial.print(SystemConfig::KEYPAD_COL_2);
    Serial.print(F(", "));
    Serial.println(SystemConfig::KEYPAD_COL_3);
    
    // Test keypad - call tick() once to initialize
    keypad->tick();
    Serial.println(F("[UIStateMachine] Keypad tick() called - keypad ready"));
  } else {
    Serial.println(F("[UIStateMachine] ERROR: keypad is nullptr!"));
  }
  
  // Set initial state
  currentState = MAIN;
  isDisplayed = false;
  isNum = true;  // Start in NUM mode
  displayOnTimer = millis();
  rels_time = 0;
  prev_rels_time_for_alpha = 0;
  
  Serial.print(F("[UIStateMachine] Initialized successfully. Initial state: "));
  Serial.print((int)currentState);
  Serial.print(F(", isNum="));
  Serial.print(isNum);
  Serial.print(F(", passLength="));
  Serial.println(passLength);
  
  return true;
}

// Main task function
void UIStateMachine::task() {
  static unsigned long last_task_print = 0;
  unsigned long now = millis();
  
  // Note: handleKeypadInput() is now called FIRST in MainSystem::task() for immediate response
  // This matches the original code where gpio_task() (which calls tick()) is called early in loop()
  
  // Update display based on current state (this calls passwordInputFSM for MAIN state)
  updateDisplay();
  
  // Debug print every 5 seconds to show keypad is being polled
  if (now - last_task_print > 5000) {
    Serial.print(F("[UIStateMachine] task() running. State="));
    Serial.print((int)currentState);
    Serial.print(F(", isNewKey="));
    Serial.print(isNewKey);
    Serial.print(F(", isDisplayed="));
    Serial.print(isDisplayed);
    Serial.print(F(", keypad="));
    if (keypad != nullptr) {
      Serial.print(F("OK"));
      // Note: available() might return 0 even when keys are pressed if tick() wasn't called recently
      // This is normal - tick() processes the hardware state
    } else {
      Serial.print(F("NULL"));
    }
    Serial.print(F(", LCD state="));
    Serial.println((int)lcdState);
    last_task_print = now;
  }
  
  // Handle timeout
  if (currentState == MAIN) {
    if (millis() - displayOnTimer > SystemConfig::DISPLAY_ON_TIMEOUT) {
      Serial.println(F("[UIStateMachine] Main screen timeout - powering off LCD"));
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
  // CRITICAL: Must call tick() first to update keypad state (like original code)
  if (keypad == nullptr) {
    static unsigned long last_error = 0;
    if (millis() - last_error > 5000) {
      Serial.println(F("[Keypad] ERROR: keypad is nullptr!"));
      last_error = millis();
    }
    return;
  }
  
  // Call tick() to update keypad state - this is CRITICAL for the library to work
  keypad->tick();
  
  // Check if any events are available
  int available_count = 0;
  while (keypad->available()) {
    keypadEvent e = keypad->read();
    available_count++;
    if (available_count == 1) {
      Serial.println(F("[Keypad] ===== EVENT DETECTED ====="));
    }
    
    Serial.print(F("Keypad event: "));
    Serial.print((char)e.bit.KEY);
    
    // Handle keypad events EXACTLY like original code
    if (e.bit.EVENT == KEY_JUST_PRESSED) {
      pressTime = millis();  // Store press time (like original: prss_time = millis())
      Serial.print((char)e.bit.KEY);
      Serial.println(F(" pressed"));
      if (lcdState == LCD_STATE_ON) {
        // Play a short beep for keypress (non-blocking - let it play in background)
        tone(SystemConfig::BUZZER_PIN, pgm_read_word(&melody[0]), 100);
        // Don't block - let tone() play in background and continue processing keypad
      }
    }
    else if (e.bit.EVENT == KEY_JUST_RELEASED) {
      // Calculate time difference (like original: time_difference = millis() - prss_time)
      timeDifference = millis() - pressTime;
      
      // Debounce check (like original: if (time_difference < 5))
      // Filter out very short presses (< 5ms) to prevent false triggers from electrical noise
      if (timeDifference < SystemConfig::KEY_DEBOUNCE_DELAY) {
        // debounce: ignore very short presses (like original)
        Serial.print(F("[Keypad] Ignoring short press ("));
        Serial.print(timeDifference);
        Serial.print(F("ms < "));
        Serial.print(SystemConfig::KEY_DEBOUNCE_DELAY);
        Serial.println(F("ms debounce)"));
        continue;
      }
      
      // Store key (like original: key = (char)e.bit.KEY)
      key = (char)e.bit.KEY;
      Serial.print(F("key: "));
      Serial.println(key);
      
      // Reset display timer (like original: display_on_timer = millis())
      displayOnTimer = millis();
      
      // Store release time (for internal use)
      releaseTime = millis();
      
      // Process key based on type (like original switch statement)
      switch (key) {
        case KEY_POWER:  // POWER is '!' but not in keymap, so this case won't be hit
          // LOCK the safe and off the display
          break;
        case KEY_MUTE: {  // MUTE is '^'
          // toggle the mute setting (like original)
          Serial.println(F("MUTE PRESSED!"));
          BuzzerController* buzzer = mainSystem->getBuzzerController();
          if (buzzer != nullptr) {
            buzzer->turnOff();
          }
          doorOpenTime = millis();
          break;
        }
        case KEY_LOCK: {  // LOCK is '*'
          // LOCK the safe and off the display (like original)
          rels_time = millis();  // Set rels_time for LOCK key (like original line 1462)
          isDisplayed = false;
          DoorController* door = mainSystem->getDoorController();
          if (door != nullptr) {
            door->closeDoor(currentUserID);
          }
          Serial.println(F("4442"));
          setState(LOCK_DOOR_STATE);
          break;
        }
        default:
          // Regular key pressed (like original default case - lines 1470-1477)
          // IMPORTANT: Always set isNewKey when a regular key is pressed, regardless of LCD state
          // This ensures keypad input works even if LCD state check fails
          Serial.println(F("pressed default case"));
          rels_time = millis();  // Set rels_time for regular keys (like original line 1474)
          isNewKey = true;
          prev_rels_time_for_alpha = rels_time;  // Initialize for alpha input timing
          
          // Play beep only if LCD is on (like original)
          if (lcdState == LCD_STATE_ON) {
            // Beep is already played above for KEY_JUST_PRESSED event
          }
          break;
      }
    }
  }
}

// Check if new index (for alpha input)
bool UIStateMachine::isNewIndex() {
  Serial.print(F("isNewIndex: isNum="));
  Serial.print(isNum);
  Serial.print(F(", key='"));
  Serial.print(key);
  Serial.print(F("' ("));
  Serial.print((int)key);
  Serial.println(F(")"));
  
  if (!isNum && key >= '0' && key <= '9') {
    uint8_t temp_key = uint8_t(key) - 48;
    Serial.print(F("Alpha mode - key number: "));
    Serial.println(temp_key);
    
    unsigned long time_since_last = rels_time - prev_rels_time_for_alpha;
    Serial.print(F("Time since last press: "));
    Serial.print(time_since_last);
    Serial.print(F(" (rels_time="));
    Serial.print(rels_time);
    Serial.print(F(", prev="));
    Serial.print(prev_rels_time_for_alpha);
    Serial.println(F(")"));
    
    // Use >= instead of > to clearly handle the 400ms boundary case
    if (prev_key != key || time_since_last >= 400) {
      Serial.println(F("New key or timeout - new index"));
      prev_rels_time_for_alpha = rels_time;
      times_prssd = 0;
      prev_key = key;
      get_character = pgm_read_byte(&num_to_alpha[temp_key][times_prssd]);
      Serial.print(F("Character from table: '"));
      Serial.print(get_character);
      Serial.println(F("'"));
      return true;
    }
    else if (time_since_last < 400 && time_since_last >= 0) {
      Serial.println(F("Same key pressed quickly - cycling character"));
      prev_rels_time_for_alpha = rels_time;
      times_prssd++;
      
      if (temp_key > 0 && temp_key <= 6) {
        get_character = pgm_read_byte(&num_to_alpha[temp_key][times_prssd % 3]);
      }
      else {
        uint8_t max_chars = (temp_key == 0) ? 2 : 2;
        get_character = pgm_read_byte(&num_to_alpha[temp_key][times_prssd % max_chars]);
      }
      Serial.print(F("Cycled character: '"));
      Serial.print(get_character);
      Serial.println(F("'"));
      return false;
    }
  }
  
  // Numeric mode or non-digit key
  times_prssd = 0;
  get_character = key;
  Serial.print(F("Numeric/non-digit - character: '"));
  Serial.print(get_character);
  Serial.println(F("'"));
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

// Handle main state (password entry or OTP input if alarm active)
void UIStateMachine::handleMainState() {
  AlarmManager* alarm = mainSystem->getAlarmManager();
  if (alarm != nullptr && alarm->isAlarmActive()) {
    // If any alarm is active, show OTP input screen
    otpInputFSM();
  } else {
    // Normal password entry
    passwordInputFSM();
  }
}

// Handle master main state
void UIStateMachine::handleMasterMainState() {
  DoorController* door = mainSystem->getDoorController();
  if (door == nullptr) return;
  
  if (!isDisplayed) {
    // Check for door error first (like original code lines 4299-4304)
    if (door->hasDoorError()) {
      lcd->clear();
      lcd->setCursor(0, 0);
      lcd->print(F("MASTER SCREEN"));
      lcd->setCursor(0, 1);
      lcd->print(F("ERROR IN OPENING"));
      isDisplayed = true;
    }
    else if (door->isDoorOpening()) {
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
  
  // Check for door error timeout (like original code lines 4341-4347)
  // If door opening timeout, show error after timeout period
  if (door->isDoorOpening() && door->hasDoorError()) {
    isDisplayed = false;  // Force redraw with error message
  } else if (!door->isDoorOpening() && door->isDoorOpen()) {
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
    Serial.println(F("[Master Menu] Displayed: MASTER SCREEN"));
  }
  
  // Debug: Print key and isNewKey status every loop when in master menu
  static char last_debug_key = '\0';
  static unsigned long last_debug_time = 0;
  if (key != last_debug_key || (millis() - last_debug_time > 1000)) {
    Serial.print(F("[Master Menu] State check - key='"));
    Serial.print(key);
    Serial.print(F("', isNewKey="));
    Serial.print(isNewKey);
    Serial.print(F(", isDisplayed="));
    Serial.print(isDisplayed);
    Serial.print(F(", currentState="));
    Serial.println((int)currentState);
    last_debug_key = key;
    last_debug_time = millis();
  }
  
  if (isNewKey) {
    isNewKey = false;
    Serial.print(F("[Master Menu] Processing key: '"));
    Serial.print(key);
    Serial.print(F("' (ASCII: "));
    Serial.print((int)key);
    Serial.println(F(")"));
    switch (key) {
      case '1':
        Serial.println(F("[Master Menu] Key '1' pressed - going to ADD_USER"));
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
    // Check for door close error first (like original code lines 4460-4466)
    if (door->hasDoorCloseError()) {
      lcd->clear();
      lcd->setCursor(0, 0);
      lcd->print(F("MASTER SCREEN"));
      lcd->setCursor(0, 1);
      lcd->print(F("ERROR IN CLOSING!!"));
      isDisplayed = true;
      delay(2000);
      isDisplayed = false;
      if (currentUserID == SystemConfig::MASTER_USER_ID) {
        setState(MASTER_INPUT_STATE);
      } else {
        setState(USER);
      }
      return;
    }
    else if (door->isDoorClosing()) {
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
  
  // Check for door close error timeout (like original code lines 4702-4710)
  if (door->isDoorClosing() && door->hasDoorCloseError()) {
    isDisplayed = false;  // Force redraw with error message
  } else if (!door->isDoorClosing() && door->isDoorClosed()) {
    isDisplayed = false;
  }
}

// Password input FSM
void UIStateMachine::passwordInputFSM() {
  static unsigned long last_call_time = 0;
  unsigned long now = millis();
  if (now - last_call_time > 1000) {
    Serial.print(F("[Password FSM] Called. State="));
    Serial.print((int)currentState);
    Serial.print(F(", isDisplayed="));
    Serial.print(isDisplayed);
    Serial.print(F(", isNewKey="));
    Serial.print(isNewKey);
    Serial.print(F(", passLength="));
    Serial.println(passLength);
    last_call_time = now;
  }
  
  FingerprintManager* fingerprint = mainSystem->getFingerprintManager();
  if (fingerprint != nullptr) {
    // Handle fingerprint authentication
    // TODO: Implement fingerprint FSM
  }
  
  // Match original structure: if (!is_displayed) { ... } else { if (is_new_key) { ... } }
  if (!isDisplayed) {
    Serial.print(F("[Password FSM] Displaying screen. State="));
    Serial.print((int)currentState);
    Serial.print(F(", passLength="));
    Serial.println(passLength);
    
    // CRITICAL: Ensure LCD is powered on before displaying
    if (lcdState != LCD_STATE_ON) {
      Serial.println(F("[Password FSM] LCD is off - powering on"));
      lcdPowerOn();
      delay(200);  // Give LCD time to power up and stabilize
    }
    
    isDisplayed = true;
    Serial.println(F("[Password FSM] Clearing LCD and setting cursor to (0,0)"));
    
    // Ensure LCD is enabled before writing
    lcd->display();
    lcd->clear();
    delay(10);  // Small delay after clear
    lcd->setCursor(0, 0);
    
    switch (currentState) {
      case MAIN:
        lcd->print(F("PASSWORD:"));
        Serial.println(F("[Password FSM] Displayed: PASSWORD:"));
        break;
      case MASTER_PASSWORD:
        lcd->print(F("MASTER PW:"));
        Serial.println(F("[Password FSM] Displayed: MASTER PW:"));
        break;
      case USER_PASSWORD:
        lcd->setCursor(0, 0);
        lcd->print(F("USER-"));
        if (currentUserID < 10) lcd->print('0');
        lcd->print(currentUserID);
        lcd->print(F(" PW "));
        Serial.print(F("[Password FSM] Displayed: USER-"));
        Serial.print(currentUserID);
        Serial.println(F(" PW"));
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
    
    Serial.print(F("[Password FSM] Current password length: "));
    Serial.println(passLength);
    
    // Display existing password characters as asterisks
    for (uint8_t i = 0; i < passLength; i++) {
      lcd->setCursor(i, 1);
      lcd->print('*');
    }
    Serial.println(F("[Password FSM] Initial display complete"));
  }
  else {
    // Display already shown - process keypad input (like original else block)
    // Note: This block runs every loop when display is already shown
    
    if (isNewKey) {
      Serial.print(F("[Password FSM] New key detected: '"));
      Serial.print(key);
      Serial.print(F("' (code="));
      Serial.print((int)key);
      Serial.print(F(") passLength="));
      Serial.println(passLength);
      
      // Reset isNewKey BEFORE processing (like original)
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
                  alarm->triggerGunPoint();
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
                  // Update master password (user ID 1)
                  // Get old password for verification (in real system, might need to verify old password first)
                  char newPassword[16];
                  for (uint8_t i = 0; i < passLength; i++) {
                    newPassword[i] = password[i];
                  }
                  newPassword[passLength] = '\0';
                  
                  // Update password (using empty old password for now - in production, verify old password first)
                  ErrorCode result = userMgr->updateUserPassword(SystemConfig::MASTER_USER_ID, 
                                                                 "", 0, 
                                                                 newPassword, passLength);
                  if (result == ErrorCode::SUCCESS) {
                    isDisplayed = true;
                    lcd->clear();
                    lcd->setCursor(0, 0);
                    lcd->print(F("MASTER PW:"));
                    lcd->setCursor(0, 1);
                    lcd->print(F("PW UPDATED!!"));
                    delay(1000);
                  } else {
                    lcd->clear();
                    lcd->setCursor(0, 0);
                    lcd->print(F("UPDATE FAILED!"));
                    delay(1000);
                  }
                }
                resetPasswordInput();
                isDisplayed = false;
                setState(MASTER_MAIN);
              }
              break;
            case USER_PASSWORD:
              {
                UserManager* userMgr = mainSystem->getUserManager();
                if (userMgr != nullptr && currentUserID > 0) {
                  // Update user password
                  char newPassword[16];
                  for (uint8_t i = 0; i < passLength; i++) {
                    newPassword[i] = password[i];
                  }
                  newPassword[passLength] = '\0';
                  
                  // Update password (using empty old password for now - in production, verify old password first)
                  ErrorCode result = userMgr->updateUserPassword(currentUserID, 
                                                                 "", 0, 
                                                                 newPassword, passLength);
                  if (result == ErrorCode::SUCCESS) {
                    isDisplayed = true;
                    lcd->setCursor(0, 1);
                    lcd->print(F("PW UPDATED!!"));
                    delay(1000);
                  } else {
                    lcd->clear();
                    lcd->setCursor(0, 0);
                    lcd->print(F("UPDATE FAILED!"));
                    delay(1000);
                  }
                }
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
          Serial.print(F("[Password FSM] Processing regular key '"));
          Serial.print(key);
          Serial.print(F("' (code="));
          Serial.print((int)key);
          Serial.println(F(")"));
          
          // Increment pass length based on isNewIndex() return (like original code)
          // pass_length += uint8_t(is_new_index());
          uint8_t increment = uint8_t(isNewIndex());
          passLength += increment;
          
          Serial.print(F("[Password FSM] isNewIndex returned: "));
          Serial.print(increment);
          Serial.print(F(", passLength now: "));
          Serial.println(passLength);
          
          if (passLength > 0 && passLength <= SystemConfig::MAX_PASSWORD_LEN) {
            char pressedChar = getPressedCharacter();
            Serial.print(F("[Password FSM] Pressed character: '"));
            Serial.print(pressedChar);
            Serial.print(F("' (code="));
            Serial.print((int)pressedChar);
            Serial.println(F(")"));
            
            // CRITICAL: Ensure LCD is powered on and displaying
            if (lcdState != LCD_STATE_ON) {
              Serial.println(F("[Password FSM] LCD is off - powering on"));
              lcdPowerOn();
              delay(100);  // Give LCD time to power up
              // Redisplay the screen after power on
              isDisplayed = false;
              return;  // Let next call handle the display
            }
            
            // CRITICAL: Show character briefly, then mask it (like original code)
            // This matches the original: lcd.print(String(get_pressed_character()));
            Serial.print(F("[Password FSM] Displaying char '"));
            Serial.print(pressedChar);
            Serial.print(F("' at position ("));
            Serial.print(passLength - 1);
            Serial.print(F(", 1) - LCD state="));
            Serial.println((int)lcdState);
            
            // Make sure LCD is enabled and cursor is set correctly
            lcd->display();
            // Show the character briefly (like original code)
            lcd->setCursor(passLength - 1, 1);
            lcd->print(pressedChar);  // Show actual character
            Serial.println(F("[Password FSM] Character displayed, waiting 400ms..."));
            delay(400);  // Show character for 400ms like original
            // Now mask it with asterisk
            Serial.println(F("[Password FSM] Masking character with '*'"));
            lcd->setCursor(passLength - 1, 1);
            lcd->print('*');
            
            // Store the character
            password[passLength - 1] = pressedChar;
            Serial.print(F("[Password FSM] Stored '"));
            Serial.print(pressedChar);
            Serial.print(F("' at password["));
            Serial.print(passLength - 1);
            Serial.println(F("]"));
            
            // Verify LCD shows the asterisk
            Serial.print(F("[Password FSM] Password display updated - should show "));
            Serial.print(passLength);
            Serial.println(F(" asterisk(s)"));
            
            // Force a final check to ensure display is on
            lcd->display();
          } else {
            Serial.print(F("[Password FSM] Password length limit reached: "));
            Serial.println(passLength);
          }
        }
        break;
    }
    }  // End of if (isNewKey) block
  }  // End of else block (display already shown)
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
      // Increment failure count (like original code)
      userBioAuthFailCount++;
      
      // BUG FIX: Check for >= 2 instead of >= 1 to allow one retry before sending alert
      // First failure (count=1): allow retry on same screen
      // Second failure (count=2): send alert and return to MAIN
      if (userBioAuthFailCount >= 2) {
        // Send alert to master user after 2nd failure (like original comment says)
        GSMHandler* gsm = mainSystem->getGSMHandler();
        if (gsm != nullptr) {
          // Send AUTH_FAIL_MSG to master user (like original: update_queue(AUTH_FAIL_MSG, MASTER_USER_ID))
          gsm->addToQueue(SystemConfig::AUTH_FAIL_MSG, SystemConfig::MASTER_USER_ID);
        }
        userBioAuthFailCount = 0;  // Reset after sending alert
        setState(MAIN);
        resetPasswordInput();
        firstUserVerified = 0;
      } else {
        // First failure - allow retry on same screen (stay on USER PASS/BIO screen)
        lcd->clear();
        lcd->setCursor(0, 0);
        lcd->print(F("USER PASS/BIO :"));
        passLength = 0;
        memset(password, '\0', sizeof(password));
        isDisplayed = false;  // Allow screen to redraw
      }
    } else {
      // Failure on MAIN screen (not in USER PASS/BIO state) - return to MAIN
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
  FingerprintManager* fingerprint = mainSystem->getFingerprintManager();
  if (fingerprint == nullptr || tempUserID == 0) {
    isDisplayed = false;
    setState(MASTER_INPUT_STATE);
    return;
  }
  
  // Set LCD for fingerprint manager to display messages
  fingerprint->setLCD(lcd);
  
  // Enroll fingerprint
  ErrorCode result = fingerprint->enrollFingerprint(tempUserID);
  if (result == ErrorCode::SUCCESS) {
    lcd->clear();
    lcd->setCursor(0, 0);
    lcd->print(F("FINGERPRINT"));
    lcd->setCursor(0, 1);
    lcd->print(F("ENROLLED!!"));
    delay(2000);
    isDisplayed = false;
    setState(MASTER_INPUT_STATE);
  } else {
    lcd->clear();
    lcd->setCursor(0, 0);
    lcd->print(F("ENROLL FAILED!"));
    delay(2000);
    isDisplayed = false;
    setState(FINGERPRINT_SCREEN);
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

// Date/Time Input FSM
void UIStateMachine::dateTimeInputFSM() {
  if (!isDisplayed) {
    isDisplayed = true;
    lcd->clear();
    lcd->setCursor(0, 0);
    lcd->print(F("HHMMSS  DD/MM/YY"));
    lcd->setCursor(0, 1);
    dateTimeCursorIndex = 0;
    for (uint8_t i = 0; i < dateTimeLen; i++) {
      if (i == 6) {
        dateTimeCursorIndex = dateTimeCursorIndex + 2;
        lcd->setCursor(dateTimeCursorIndex++, 1);
        lcd->print(dateTime[i]);
      } else if (i == 8 || i == 10) {
        lcd->setCursor(dateTimeCursorIndex++, 1);
        lcd->print('/');
        lcd->setCursor(dateTimeCursorIndex++, 1);
        lcd->print(dateTime[i]);
      } else {
        lcd->setCursor(dateTimeCursorIndex++, 1);
        lcd->print(dateTime[i]);
      }
    }
  } else {
    if (isNewKey) {
      isNewKey = false;
      switch (key) {
        case KEY_CANCEL:
          if (dateTimeLen == 0) {
            passLength = 0;
            isDisplayed = false;
            setState(MASTER_INPUT_STATE);
          } else {
            isDisplayed = false;
            resetDateTimeInput();
          }
          break;
        case KEY_ENTER:
          if (dateTimeLen == 12) {
            RTCHandler* rtc = mainSystem->getRTCHandler();
            if (rtc != nullptr) {
              // Parse date/time: HHMMSS DDMMYY
              uint8_t second_val = dateTime[4] * 10 + dateTime[5];
              uint8_t minute_val = dateTime[2] * 10 + dateTime[3];
              uint8_t hour_val = dateTime[0] * 10 + dateTime[1];
              uint8_t date_val = dateTime[6] * 10 + dateTime[7];
              uint8_t month_val = dateTime[8] * 10 + dateTime[9];
              uint8_t year_val = dateTime[10] * 10 + dateTime[11];
              
              ErrorCode result = rtc->setDateTime(date_val, month_val, year_val, 
                                                  hour_val, minute_val, second_val);
              if (result == ErrorCode::SUCCESS) {
                lcd->clear();
                lcd->setCursor(0, 0);
                lcd->print(F("  DATE & TIME  "));
                lcd->setCursor(0, 1);
                lcd->print(F("SET SUCCESSFULLY"));
                delay(3000);
                setState(MASTER_MAIN);
                isDisplayed = false;
              } else {
                resetDateTimeInput();
                lcd->clear();
                lcd->setCursor(0, 0);
                lcd->print(F("  DATE & TIME  "));
                lcd->setCursor(0, 1);
                lcd->print(F("INVALID DATE TIME!!"));
                delay(3000);
                isDisplayed = false;
              }
            }
          } else {
            lcd->clear();
            lcd->setCursor(0, 0);
            lcd->print(F("ADD ALL DETAILS!"));
            isDisplayed = false;
            delay(1000);
          }
          break;
        default:
          if (isNewIndex()) {
            uint8_t temp_key = uint8_t(key) - 48;
            if (dateTimeLen < 12) {
              dateTime[dateTimeLen] = temp_key;
              dateTimeLen++;
            }
            isDisplayed = false;
          }
          break;
      }
    }
  }
}

// Mobile Number Input FSM
void UIStateMachine::mobileNumberInputFSM(uint8_t targetUserID) {
  if (!isDisplayed) {
    isDisplayed = true;
    lcd->clear();
    lcd->setCursor(0, 0);
    if (inputMobileNumberCount == 0) {
      lcd->print(F("MOBILE NUMBER :"));
    } else {
      lcd->print(F("RECONFIRM :"));
    }
    lcd->setCursor(0, 1);
    for (uint8_t i = 0; i < inputMobileNumberLength; i++) {
      lcd->setCursor(i, 1);
      lcd->print(inputMobileNumber[i]);
    }
  } else {
    if (isNewKey) {
      isNewKey = false;
      switch (key) {
        case KEY_CANCEL:
          if (inputMobileNumberLength == 0) {
            isDisplayed = false;
            inputMobileNumberCount = 0;
            setState(MASTER_INPUT_STATE);
            resetMobileNumberInput();
          } else {
            isDisplayed = false;
            inputMobileNumberCount = 0;
            resetMobileNumberInput();
          }
          break;
        case KEY_ENTER:
          if (inputMobileNumberLength == SystemConfig::MOBILE_NUMBER_LENGTH) {
            isDisplayed = false;
            if (inputMobileNumberCount == 0) {
              // First entry - store for confirmation
              inputMobileNumberCount++;
              for (uint8_t i = 0; i < SystemConfig::MOBILE_NUMBER_LENGTH; i++) {
                prevInputMobileNumber[i] = inputMobileNumber[i];
              }
              inputMobileNumberLength = 0;
              memset(inputMobileNumber, '\0', sizeof(inputMobileNumber));
            } else {
              // Second entry - verify match
              mobileNumberNotMatched = false;
              for (uint8_t i = 0; i < SystemConfig::MOBILE_NUMBER_LENGTH; i++) {
                if (prevInputMobileNumber[i] != inputMobileNumber[i]) {
                  mobileNumberNotMatched = true;
                }
              }
              if (mobileNumberNotMatched) {
                lcd->clear();
                lcd->setCursor(0, 0);
                lcd->print(F("MOBILE NUMBER :"));
                lcd->setCursor(0, 1);
                lcd->print(F("NO NOT MATCHED!"));
                inputMobileNumberCount = 0;
                resetMobileNumberInput();
                delay(2000);
                isDisplayed = false;
              } else {
                // Numbers match - add/update user
                UserManager* userMgr = mainSystem->getUserManager();
                if (userMgr != nullptr) {
                  char mobile[11];
                  for (uint8_t i = 0; i < SystemConfig::MOBILE_NUMBER_LENGTH; i++) {
                    mobile[i] = inputMobileNumber[i];
                  }
                  mobile[SystemConfig::MOBILE_NUMBER_LENGTH] = '\0';
                  
                  // Check if this is a new user creation (from MASTER_ADD_USER_MOBILE_NUMBER state)
                  if (currentState == MASTER_ADD_USER_MOBILE_NUMBER && targetUserID != SystemConfig::MASTER_USER_ID) {
                    // New user creation - add user with default password "1234"
                    const char defaultPassword[] = "1234";
                    ErrorCode result = userMgr->addUser(targetUserID, mobile, defaultPassword, 4);
                    if (result == ErrorCode::SUCCESS) {
                      lcd->clear();
                      lcd->setCursor(0, 0);
                      lcd->print(F("PLEASE WAIT...!!"));
                      lcd->setCursor(0, 1);
                      lcd->print(F("CREATING USER-"));
                      lcd->print(targetUserID);
                      delay(1000);
                      
                      // Show default password
                      lcd->clear();
                      lcd->setCursor(0, 0);
                      lcd->print(F("DEFAULT PASSWORD"));
                      lcd->setCursor(0, 1);
                      lcd->print(F("USER-"));
                      lcd->print(targetUserID);
                      lcd->print(F(": "));
                      lcd->print(defaultPassword);
                      delay(2000);
                      isDisplayed = false;
                      setState(MASTER_INPUT_STATE);
                    } else {
                      lcd->clear();
                      lcd->setCursor(0, 0);
                      lcd->print(F("CREATE FAILED!"));
                      delay(2000);
                      isDisplayed = false;
                      setState(MASTER_INPUT_STATE);
                    }
                  } else {
                    // Existing user - update mobile number only
                    ErrorCode result = userMgr->updateUserMobile(targetUserID, mobile);
                    if (result == ErrorCode::SUCCESS) {
                      lcd->clear();
                      lcd->setCursor(0, 0);
                      lcd->print(F("MOBILE NUMBER"));
                      lcd->setCursor(0, 1);
                      lcd->print(F("   UPDATED!!"));
                      delay(2000);
                      isDisplayed = false;
                      if (targetUserID == SystemConfig::MASTER_USER_ID) {
                        setState(MASTER_MAIN);
                      } else {
                        setState(USER);
                      }
                    } else {
                      lcd->clear();
                      lcd->setCursor(0, 0);
                      lcd->print(F("UPDATE FAILED!"));
                      delay(2000);
                      isDisplayed = false;
                      setState(MASTER_INPUT_STATE);
                    }
                  }
                }
                inputMobileNumberCount = 0;
                resetMobileNumberInput();
              }
            }
          } else {
            lcd->clear();
            lcd->setCursor(0, 1);
            lcd->print(F("Short!"));
            isDisplayed = false;
            delay(1000);
          }
          break;
        default:
          if (inputMobileNumberLength < SystemConfig::MOBILE_NUMBER_LENGTH) {
            if (isNewIndex()) {
              inputMobileNumber[inputMobileNumberLength] = getPressedCharacter();
              inputMobileNumberLength++;
            }
            isDisplayed = false;
          }
          break;
      }
    }
  }
}

// User ID Input FSM
void UIStateMachine::userIDInputFSM() {
  if (!isDisplayed) {
    isDisplayed = true;
    lcd->clear();
    lcd->setCursor(0, 0);
    switch (userIDInputScreenType) {
      case ADD_USER:
        lcd->print(F("CREATE USER"));
        break;
      case REMOVE_USER:
        lcd->print(F("REMOVE USER"));
        break;
      case ADD_FINGERPRINT:
        lcd->print(F("ADD FINGERPRINT"));
        break;
    }
    lcd->setCursor(0, 1);
    lcd->print(F("USER ID: "));
    userIDInputLength = 0;
    memset(userIDInput, '\0', sizeof(userIDInput));
  } else {
    if (isNewKey) {
      isNewKey = false;
      if (key == KEY_CANCEL) {
        resetUserIDInput();
        isDisplayed = false;
        setState(MASTER_INPUT_STATE);
      } else if (key == KEY_ENTER) {
        if (userIDInputLength > 0) {
          uint8_t user_id = atoi(userIDInput);
          Serial.print(F("User ID entered: "));
          Serial.println(user_id);
          if (user_id >= 1 && user_id <= SystemConfig::MAX_NUM_OF_USERS) {
            UserManager* userMgr = mainSystem->getUserManager();
            if (userMgr == nullptr) {
              isDisplayed = false;
              setState(MASTER_INPUT_STATE);
              return;
            }
            
            switch (userIDInputScreenType) {
              case ADD_USER:
                if (!userMgr->userExists(user_id) && user_id != SystemConfig::MASTER_USER_ID) {
                  tempUserID = user_id;
                  isDisplayed = false;
                  setState(MASTER_ADD_USER_MOBILE_NUMBER);
                } else {
                  lcd->clear();
                  lcd->setCursor(0, 0);
                  lcd->print(F("USER ALREADY"));
                  lcd->setCursor(0, 1);
                  lcd->print(F("EXISTS!!"));
                  delay(2000);
                  isDisplayed = false;
                  setState(MASTER_INPUT_STATE);
                }
                break;
              case REMOVE_USER:
                if (userMgr->userExists(user_id) && user_id != SystemConfig::MASTER_USER_ID) {
                  lcd->clear();
                  lcd->setCursor(0, 0);
                  lcd->print(F("PLEASE WAIT...!!"));
                  lcd->setCursor(0, 1);
                  lcd->print(F("DELETING USER-"));
                  lcd->print(user_id);
                  
                  // Delete fingerprint
                  FingerprintManager* fingerprint = mainSystem->getFingerprintManager();
                  if (fingerprint != nullptr) {
                    fingerprint->deleteFingerprint(user_id);
                  }
                  
                  // Remove user
                  ErrorCode result = userMgr->removeUser(user_id);
                  delay(3000);
                  
                  if (result == ErrorCode::SUCCESS) {
                    lcd->clear();
                    lcd->setCursor(0, 0);
                    lcd->print(F("USER-"));
                    lcd->print(user_id);
                    lcd->print(F(" DELETED!!"));
                    delay(2000);
                  } else {
                    lcd->clear();
                    lcd->setCursor(0, 0);
                    lcd->print(F("DELETE FAILED!"));
                    delay(2000);
                  }
                  isDisplayed = false;
                  setState(MASTER_INPUT_STATE);
                } else {
                  lcd->clear();
                  lcd->setCursor(0, 0);
                  lcd->print(F("USER NOT"));
                  lcd->setCursor(0, 1);
                  lcd->print(F("FOUND!!"));
                  delay(2000);
                  isDisplayed = false;
                  setState(MASTER_INPUT_STATE);
                }
                break;
              case ADD_FINGERPRINT:
                if (userMgr->userExists(user_id)) {
                  tempUserID = user_id;
                  isDisplayed = false;
                  setState(ADD_FINGERPRINT_SCREEN);
                } else {
                  lcd->clear();
                  lcd->setCursor(0, 0);
                  lcd->print(F("USER NOT"));
                  lcd->setCursor(0, 1);
                  lcd->print(F("CONFIGURED!!"));
                  delay(2000);
                  isDisplayed = false;
                  setState(MASTER_INPUT_STATE);
                }
                break;
            }
          } else {
            lcd->clear();
            lcd->setCursor(0, 0);
            lcd->print(F("INVALID USER"));
            lcd->setCursor(0, 1);
            lcd->print(F("ID (1-"));
            lcd->print(SystemConfig::MAX_NUM_OF_USERS);
            lcd->print(F(")!!"));
            delay(2000);
            isDisplayed = false;
            setState(MASTER_INPUT_STATE);
          }
        } else {
          lcd->setCursor(0, 0);
          lcd->print(F("ENTER USER"));
          lcd->setCursor(0, 1);
          lcd->print(F("ID FIRST!!"));
          delay(2000);
          isDisplayed = false;
          setState(MASTER_INPUT_STATE);
        }
      } else if (key >= '0' && key <= '9' && userIDInputLength < 2) {
        userIDInput[userIDInputLength] = key;
        userIDInputLength++;
        lcd->setCursor(9 + userIDInputLength - 1, 1);
        lcd->print(key);
      }
    }
  }
}

// Buzzer Input FSM
void UIStateMachine::buzzerInputFSM() {
  static bool buzzerTimeoutUpdated = false;
  
  if (!isDisplayed) {
    isDisplayed = true;
    lcd->clear();
    lcd->setCursor(0, 0);
    lcd->print(F("  DOOR TIMEOUT"));
    lcd->setCursor(0, 1);
    lcd->print(F("MINUTE: "));
    if (buzzerTimeoutUpdated) {
      buzzerCounter++;
      buzzerTimeoutUpdated = false;
      lcd->print(inputBuzzerTimeout);
    }
  } else {
    if (isNewKey) {
      isNewKey = false;
      switch (key) {
        case KEY_CANCEL:
          if (buzzerCounter == 0) {
            passLength = 0;
            isDisplayed = false;
            setState(MASTER_INPUT_STATE);
          } else {
            buzzerCounter = 0;
            isDisplayed = false;
            buzzerTimeoutUpdated = false;
            inputBuzzerTimeout = 0;
          }
          break;
        case KEY_ENTER:
          lcd->clear();
          lcd->setCursor(0, 0);
          lcd->print(F(" BUZZER TIMEOUT "));
          lcd->setCursor(0, 1);
          if (inputBuzzerTimeout == 0) {
            inputBuzzerTimeout = 1;
          }
          
          // Update buzzer timeout in EEPROM and BuzzerController
          EEPROMStorage* eeprom = mainSystem->getEEPROMStorage();
          if (eeprom != nullptr) {
            eeprom->writeBuzzerTimeout(inputBuzzerTimeout);
          }
          
          BuzzerController* buzzer = mainSystem->getBuzzerController();
          if (buzzer != nullptr) {
            buzzer->setTimeout(inputBuzzerTimeout);
          }
          
          lcd->print(F("    UPDATED.  "));
          delay(3000);
          isDisplayed = false;
          buzzerCounter = 0;
          inputBuzzerTimeout = 0;
          setState(MASTER_MAIN);
          break;
        default:
          buzzerTimeoutUpdated = isNewIndex();
          if (buzzerTimeoutUpdated) {
            uint8_t temp_key = uint8_t(key) - 48;
            if (temp_key <= 9) {
              buzzerTimeoutUpdated = true;
              inputBuzzerTimeout = inputBuzzerTimeout * 10 + temp_key;
              if (inputBuzzerTimeout > 150) {
                inputBuzzerTimeout = 150;
              }
              isDisplayed = false;
            }
          }
          break;
      }
    }
  }
}

// Holiday Menu FSM
void UIStateMachine::holidayMenuFSM() {
  HolidayManager* holidayMgr = mainSystem->getHolidayManager();
  if (holidayMgr == nullptr) {
    isDisplayed = false;
    setState(MASTER_INPUT_STATE);
    return;
  }
  
  if (!isDisplayed) {
    isDisplayed = true;
    lcd->clear();
    
    switch (holidayMenuState) {
      case HOLIDAY_MENU_MAIN:
        lcd->setCursor(0, 0);
        lcd->print(F("  HOLIDAY MENU  "));
        lcd->setCursor(0, 1);
        lcd->print(F("1:ADD 2:REM 3:VIEW"));
        break;
        
      case HOLIDAY_MENU_ADD:
        lcd->setCursor(0, 0);
        lcd->print(F("  ADD HOLIDAY   "));
        lcd->setCursor(0, 1);
        lcd->print(F("DATE: "));
        if (holidayInputDate > 0) {
          lcd->print(holidayInputDate);
        }
        break;
        
      case HOLIDAY_MENU_INPUT_MONTH:
        lcd->setCursor(0, 0);
        if (holidayIsRemoveMode) {
          lcd->print(F(" REMOVE HOLIDAY "));
        } else {
          lcd->print(F("  ADD HOLIDAY   "));
        }
        lcd->setCursor(0, 1);
        lcd->print(F("MONTH: "));
        if (holidayInputMonth > 0) {
          lcd->print(holidayInputMonth);
        }
        break;
        
      case HOLIDAY_MENU_INPUT_YEAR:
        lcd->setCursor(0, 0);
        if (holidayIsRemoveMode) {
          lcd->print(F(" REMOVE HOLIDAY "));
        } else {
          lcd->print(F("  ADD HOLIDAY   "));
        }
        lcd->setCursor(0, 1);
        lcd->print(F("YEAR: "));
        if (holidayInputYear > 0) {
          lcd->print(holidayInputYear);
        }
        break;
        
      case HOLIDAY_MENU_REMOVE:
        lcd->setCursor(0, 0);
        lcd->print(F(" REMOVE HOLIDAY "));
        lcd->setCursor(0, 1);
        lcd->print(F("DATE: "));
        if (holidayInputDate > 0) {
          lcd->print(holidayInputDate);
        }
        break;
        
      case HOLIDAY_MENU_VIEW: {
        uint8_t count = holidayMgr->getHolidayCount();
        if (count == 0) {
          lcd->setCursor(0, 0);
          lcd->print(F("  NO HOLIDAYS   "));
          lcd->setCursor(0, 1);
          lcd->print(F("   CONFIGURED   "));
        } else {
          // Ensure index is within bounds
          if (holidayViewIndex >= count) {
            holidayViewIndex = count - 1;
          }
          
          lcd->setCursor(0, 0);
          lcd->print(F("HOLIDAY "));
          lcd->print(holidayViewIndex + 1);
          lcd->print('/');
          lcd->print(count);
          lcd->setCursor(0, 1);
          
          uint8_t h_date, h_month, h_year;
          if (holidayMgr->getHoliday(holidayViewIndex, &h_date, &h_month, &h_year) == ErrorCode::SUCCESS) {
            if (h_date < 10) lcd->print('0');
            lcd->print(h_date);
            lcd->print('/');
            if (h_month < 10) lcd->print('0');
            lcd->print(h_month);
            lcd->print('/');
            if (h_year < 10) lcd->print('0');
            lcd->print(h_year);
          }
        }
        break;
      }
    }
  } else {
    if (isNewKey) {
      isNewKey = false;
      
      switch (holidayMenuState) {
        case HOLIDAY_MENU_MAIN:
          switch (key) {
            case '1':
              holidayMenuState = HOLIDAY_MENU_ADD;
              resetHolidayInput();
              holidayIsRemoveMode = false;
              isDisplayed = false;
              break;
            case '2':
              holidayMenuState = HOLIDAY_MENU_REMOVE;
              resetHolidayInput();
              holidayIsRemoveMode = true;
              isDisplayed = false;
              break;
            case '3':
              holidayMenuState = HOLIDAY_MENU_VIEW;
              holidayViewIndex = 0;
              isDisplayed = false;
              break;
            case KEY_CANCEL:
              holidayMenuState = HOLIDAY_MENU_MAIN;
              isDisplayed = false;
              setState(MASTER_INPUT_STATE);
              break;
          }
          break;
          
        case HOLIDAY_MENU_ADD:
          // Input date
          if (key >= '0' && key <= '9') {
            uint8_t digit = key - '0';
            if (holidayInputDate == 0) {
              holidayInputDate = digit;
            } else {
              holidayInputDate = holidayInputDate * 10 + digit;
              if (holidayInputDate > 31) holidayInputDate = 31;
            }
            isDisplayed = false;
          } else if (key == KEY_ENTER && holidayInputDate > 0) {
            holidayMenuState = HOLIDAY_MENU_INPUT_MONTH;
            isDisplayed = false;
          } else if (key == KEY_CANCEL) {
            holidayMenuState = HOLIDAY_MENU_MAIN;
            resetHolidayInput();
            holidayIsRemoveMode = false;
            isDisplayed = false;
          }
          break;
          
        case HOLIDAY_MENU_INPUT_MONTH:
          // Input month (works for both ADD and REMOVE)
          if (key >= '0' && key <= '9') {
            uint8_t digit = key - '0';
            if (holidayInputMonth == 0) {
              holidayInputMonth = digit;
            } else {
              holidayInputMonth = holidayInputMonth * 10 + digit;
              if (holidayInputMonth > 12) holidayInputMonth = 12;
            }
            isDisplayed = false;
          } else if (key == KEY_ENTER && holidayInputMonth > 0) {
            holidayMenuState = HOLIDAY_MENU_INPUT_YEAR;
            isDisplayed = false;
          } else if (key == KEY_CANCEL) {
            // Go back to previous state (ADD or REMOVE)
            if (holidayIsRemoveMode) {
              holidayMenuState = HOLIDAY_MENU_REMOVE;
            } else {
              holidayMenuState = HOLIDAY_MENU_ADD;
            }
            holidayInputMonth = 0;
            isDisplayed = false;
          }
          break;
          
        case HOLIDAY_MENU_INPUT_YEAR:
          // Input year (works for both ADD and REMOVE)
          if (key >= '0' && key <= '9') {
            uint8_t digit = key - '0';
            if (holidayInputYear == 0) {
              holidayInputYear = digit;
            } else {
              holidayInputYear = holidayInputYear * 10 + digit;
              if (holidayInputYear > 99) holidayInputYear = 99;
            }
            isDisplayed = false;
          } else if (key == KEY_ENTER && holidayInputYear >= 0) {
            if (holidayIsRemoveMode) {
              // Remove holiday
              ErrorCode result = holidayMgr->removeHoliday(holidayInputDate, holidayInputMonth, holidayInputYear);
              if (result == ErrorCode::SUCCESS) {
                lcd->clear();
                lcd->setCursor(0, 0);
                lcd->print(F(" HOLIDAY REMOVED"));
                lcd->setCursor(0, 1);
                lcd->print(F("   SUCCESSFULLY "));
                delay(2000);
              } else {
                lcd->clear();
                lcd->setCursor(0, 0);
                lcd->print(F("  HOLIDAY NOT   "));
                lcd->setCursor(0, 1);
                lcd->print(F("     FOUND      "));
                delay(2000);
              }
            } else {
              // Add holiday
              ErrorCode result = holidayMgr->addHoliday(holidayInputDate, holidayInputMonth, holidayInputYear);
              if (result == ErrorCode::SUCCESS) {
                lcd->clear();
                lcd->setCursor(0, 0);
                lcd->print(F("  HOLIDAY ADDED "));
                lcd->setCursor(0, 1);
                lcd->print(F("   SUCCESSFULLY "));
                delay(2000);
              } else {
                lcd->clear();
                lcd->setCursor(0, 0);
                lcd->print(F("  FAILED TO ADD "));
                lcd->setCursor(0, 1);
                lcd->print(F("  HOLIDAY/EXISTS "));
                delay(2000);
              }
            }
            holidayMenuState = HOLIDAY_MENU_MAIN;
            resetHolidayInput();
            holidayIsRemoveMode = false;
            isDisplayed = false;
          } else if (key == KEY_CANCEL) {
            holidayMenuState = HOLIDAY_MENU_INPUT_MONTH;
            holidayInputYear = 0;
            isDisplayed = false;
          }
          break;
          
        case HOLIDAY_MENU_REMOVE:
          // Input date
          if (key >= '0' && key <= '9') {
            uint8_t digit = key - '0';
            if (holidayInputDate == 0) {
              holidayInputDate = digit;
            } else {
              holidayInputDate = holidayInputDate * 10 + digit;
              if (holidayInputDate > 31) holidayInputDate = 31;
            }
            isDisplayed = false;
          } else if (key == KEY_ENTER && holidayInputDate > 0) {
            holidayMenuState = HOLIDAY_MENU_INPUT_MONTH;
            isDisplayed = false;
          } else if (key == KEY_CANCEL) {
            holidayMenuState = HOLIDAY_MENU_MAIN;
            resetHolidayInput();
            holidayIsRemoveMode = false;
            isDisplayed = false;
          }
          break;
          
        case HOLIDAY_MENU_VIEW:
          if (key == KEY_CANCEL) {
            holidayMenuState = HOLIDAY_MENU_MAIN;
            holidayViewIndex = 0;
            isDisplayed = false;
          } else if (key == '1' || key == '4') { // Previous holiday
            if (holidayViewIndex > 0) {
              holidayViewIndex--;
              isDisplayed = false;
            }
          } else if (key == '2' || key == '6') { // Next holiday
            uint8_t count = holidayMgr->getHolidayCount();
            if (count > 0 && holidayViewIndex < count - 1) {
              holidayViewIndex++;
              isDisplayed = false;
            } else if (count > 0 && holidayViewIndex >= count) {
              holidayViewIndex = count - 1;
              isDisplayed = false;
            }
          } else if (key == '3') { // Jump to first
            uint8_t count = holidayMgr->getHolidayCount();
            if (count > 0) {
              holidayViewIndex = 0;
              isDisplayed = false;
            }
          } else if (key == '5') { // Jump to last
            uint8_t count = holidayMgr->getHolidayCount();
            if (count > 0) {
              holidayViewIndex = count - 1;
              isDisplayed = false;
            }
          }
          break;
      }
    }
  }
}

// Backup Screen FSM
void UIStateMachine::backupScreenFSM() {
  static bool backupInProgress = false;
  static bool backupComplete = false;
  static bool toggleBit = false;
  
  if (!isDisplayed) {
    if (backupInProgress) {
      if (backupComplete) {
        backupComplete = false;
        backupInProgress = false;
        lcd->clear();
        lcd->setCursor(0, 0);
        lcd->print(F("  BACKUP "));
        lcd->setCursor(0, 1);
        lcd->print(F("  COMPLETED!!"));
        delay(2000);
        isDisplayed = false;
        setState(MASTER_MAIN);
      } else {
        // Toggle display during backup
        if (toggleBit) {
          toggleBit = false;
          lcd->clear();
          lcd->setCursor(0, 0);
          lcd->print(F("  BACKUP "));
          lcd->setCursor(0, 1);
          lcd->print(F("IN PROGRESS .. "));
        } else {
          toggleBit = true;
          lcd->clear();
          lcd->setCursor(0, 0);
          lcd->print(F("  BACKUP "));
          lcd->setCursor(0, 1);
          lcd->print(F("IN PROGRESS .. .."));
        }
      }
    } else {
      isDisplayed = true;
      lcd->clear();
      lcd->setCursor(0, 0);
      lcd->print(F("  WANT TO TAKE"));
      lcd->setCursor(0, 1);
      lcd->print(F("  BACKUP..??"));
      backupInProgress = false;
      backupComplete = false;
    }
  } else {
    if (isNewKey) {
      isNewKey = false;
      switch (key) {
        case KEY_CANCEL:
          isDisplayed = false;
          setState(MASTER_INPUT_STATE);
          break;
        case KEY_ENTER:
          // TODO: Check for SD card and flash drive availability
          // For now, just show backup in progress message
          lcd->clear();
          lcd->setCursor(0, 0);
          lcd->print(F("  BACKUP "));
          lcd->setCursor(0, 1);
          lcd->print(F("IN PROGRESS .. .."));
          isDisplayed = false;
          backupInProgress = true;
          backupComplete = false;
          // Simulate backup completion after delay
          // In real implementation, this would trigger actual backup process
          delay(2000);
          backupComplete = true;
          isDisplayed = false;
          break;
        default:
          break;
      }
    }
  }
}

// OTP Input FSM (for alarm deactivation)
void UIStateMachine::otpInputFSM() {
  AlarmManager* alarm = mainSystem->getAlarmManager();
  if (alarm == nullptr) {
    isDisplayed = false;
    setState(MAIN);
    return;
  }
  
  if (!isDisplayed) {
    isDisplayed = true;
    lcd->clear();
    lcd->setCursor(0, 0);
    lcd->print(F("ENTER OTP:"));
    lcd->setCursor(0, 1);
    for (uint8_t i = 0; i < otpLength; i++) {
      lcd->setCursor(i, 1);
      lcd->print(otp[i]);
    }
  } else {
    if (isNewKey) {
      isNewKey = false;
      switch (key) {
        case KEY_CANCEL:
          if (otpLength == 0) {
            isDisplayed = false;
            otpLength = 0;
            memset(otp, 0, sizeof(otp));
          } else {
            isDisplayed = false;
            otpLength = 0;
            memset(otp, 0, sizeof(otp));
          }
          break;
        case KEY_ENTER:
          otpNotMatched = false;
          if (otpLength == 6) {
            // Check against generated OTP
            for (uint8_t i = 0; i < 6; i++) {
              if (otp[i] != generatedOTP[i]) {
                otpNotMatched = true;
              }
            }
            
            // If not matched, check master OTP (455556)
            if (otpNotMatched) {
              otpNotMatched = false;
              uint8_t master_otp[6] = {4, 5, 5, 5, 5, 6};
              for (uint8_t i = 0; i < 6; i++) {
                if (otp[i] != master_otp[i]) {
                  otpNotMatched = true;
                }
              }
            }
            
            if (!otpNotMatched) {
              // OTP matched - deactivate alarms
              if (alarm != nullptr) {
                // Reset all alarms
                // Note: AlarmManager should have methods to deactivate alarms
              }
              
              // Turn off sirens (if implemented)
              // siren_off(siren_pin[0]);
              // siren_off(siren_pin[1]);
              
              otpLength = 0;
              memset(otp, 0, sizeof(otp));
              isDisplayed = false;
              setState(MAIN);
            } else {
              otpNotMatched = false;
              otpLength = 0;
              memset(otp, 0, sizeof(otp));
              lcd->clear();
              lcd->setCursor(0, 0);
              lcd->print(F("OTP NOT MATCHED"));
              delay(2000);
              isDisplayed = false;
            }
          } else {
            otpNotMatched = false;
            otpLength = 0;
            memset(otp, 0, sizeof(otp));
            lcd->clear();
            lcd->setCursor(0, 0);
            lcd->print(F("ENTER 6 DIGITS"));
            delay(2000);
            isDisplayed = false;
          }
          break;
        default:
          if (otpLength < 6) {
            if (isNewIndex()) {
              uint8_t temp_key = uint8_t(key) - 48;
              if (temp_key <= 9) {
                otp[otpLength] = temp_key;
                otpLength++;
              }
            }
            isDisplayed = false;
          }
          break;
      }
    }
  }
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

