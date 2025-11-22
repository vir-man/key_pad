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
          if (lcdState == LCD_STATE_ON) {
            Serial.println(F("pressed default case"));
            rels_time = millis();  // Set rels_time for regular keys (like original line 1474)
            isNewKey = true;
            prev_rels_time_for_alpha = rels_time;  // Initialize for alpha input timing
            break;
          }
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
    
    if (prev_key != key || time_since_last > 400) {
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

// Handle main state (password entry)
void UIStateMachine::handleMainState() {
  Serial.println(F("handleMainState() called - entering passwordInputFSM"));
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

