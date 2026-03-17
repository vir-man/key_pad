#include "DisplayManager.h"

// PROGMEM Strings for UI
const char STR_MAIN_TITLE[] PROGMEM = "BMS SAFE";
const char STR_INPUT_PASS[] PROGMEM = "ENTER PASS:";
const char STR_ACCESS_GRANTED[] PROGMEM = "ACCESS GRANTED";
const char STR_ACCESS_DENIED[] PROGMEM = "ACCESS DENIED";

DisplayManager& DisplayManager::getInstance() {
    static DisplayManager instance;
    return instance;
}

DisplayManager::DisplayManager() 
    : _lcd(PIN_LCD_RS, PIN_LCD_EN, PIN_LCD_D4, PIN_LCD_D5, PIN_LCD_D6, PIN_LCD_D7),
      _currentScreen(ScreenState::MAIN_IDLE), _displayOn(true), _displayTimer(0), _inputIndex(0)
{
}

void DisplayManager::begin() {
    pinMode(PIN_LCD_VCC, OUTPUT);
    pinMode(PIN_LCD_GND, OUTPUT);
    digitalWrite(PIN_LCD_GND, LOW); // GND
    
    setDisplayPower(true); // Default On
    _lcd.begin(16, 2);
    _lcd.clear();
    setScreen(ScreenState::MAIN_IDLE);
}

void DisplayManager::setDisplayPower(bool on) {
    _displayOn = on;
    digitalWrite(PIN_LCD_VCC, on ? HIGH : LOW);
    if (on) {
        _lcd.display();
        _displayTimer = millis(); 
    } else {
        _lcd.noDisplay(); // Optional if power is cut
    }
}

void DisplayManager::update() {
    // Auto timeout for display
    if (_displayOn && (millis() - _displayTimer > TIMEOUT_DISPLAY_ON)) {
        setDisplayPower(false);
        setScreen(ScreenState::MAIN_OFF);
    }
    
    // Wake up on key press
    if (InputManager::getInstance().isKeyPressed()) {
        if (!_displayOn) {
            setDisplayPower(true);
            setScreen(ScreenState::MAIN_IDLE); // Reset to main
            return; // Consume first key for wake? Or process it? 
                    // Original usually wakes up.
        }
        _displayTimer = millis(); // Reset timer
    }

    if (!_displayOn) return;

    // Dispatch based on screen
    switch (_currentScreen) {
        case ScreenState::MAIN_IDLE:
            handleMain();
            break;
        case ScreenState::USER_INPUT:
            handleUserInput();
            break;
        case ScreenState::MASTER_MENU:
            handleMasterMenu();
            break;
        default:
            // Handle other states or ignore
            break;
    }
}

void DisplayManager::setScreen(ScreenState screen) {
    _currentScreen = screen;
    _lcd.clear();
    clearInput();
    
    // Initial Render for static text
    if (screen == ScreenState::MAIN_IDLE) {
        // Show Date/Time or Title
        printLineP(0, STR_MAIN_TITLE);
        // Date/Time update is dynamic in loop
    } else if (screen == ScreenState::USER_INPUT) {
        printLineP(0, STR_INPUT_PASS);
    }
}

void DisplayManager::handleMain() {
    // Check inputs for specific actions
    char key = InputManager::getInstance().getKey();
    if (key == '#') { // Enter -> Go to Input
        setScreen(ScreenState::USER_INPUT);
    } else if (key == '!') { // Power -> Off
        setDisplayPower(false);
    }
    // Update Time on line 1?
}

void DisplayManager::handleUserInput() {
    char key = InputManager::getInstance().getKey();
    if (key) {
        if (key == '*') { // Cancel/Back
            setScreen(ScreenState::MAIN_IDLE);
        } else if (key == '#') { // Enter
            // Verify
            _inputBuffer[_inputIndex] = '\0';
            // Simple check for Master (ID 0) or others
            // For now, testing logic:
            if (AccessManager::getInstance().verifyPassword(0, _inputBuffer)) {
                 _lcd.clear();
                 printLineP(0, STR_ACCESS_GRANTED);
                 delay(2000);
                 // Door Open
                 setScreen(ScreenState::MAIN_IDLE);
            } else {
                 _lcd.clear();
                 printLineP(0, STR_ACCESS_DENIED);
                 delay(2000);
                 setScreen(ScreenState::MAIN_IDLE);
            }
        } else {
             // Append
             appendInput(key);
             _lcd.setCursor(_inputIndex-1, 1);
             _lcd.print('*'); // Masked
        }
    }
}

void DisplayManager::clearInput() {
    _inputIndex = 0;
    memset(_inputBuffer, 0, 20);
}

void DisplayManager::appendInput(char key) {
    if (_inputIndex < 15) {
        _inputBuffer[_inputIndex++] = key;
    }
}

void DisplayManager::printLine(uint8_t line, const char* str) {
    _lcd.setCursor(0, line);
    _lcd.print(str);
}

void DisplayManager::printLineP(uint8_t line, const char* str_P) {
    _lcd.setCursor(0, line);
    char buffer[17];
    strncpy_P(buffer, str_P, 16);
    buffer[16] = 0;
    _lcd.print(buffer);
}

// Stub handlers
void DisplayManager::handleMasterMenu() {}
void DisplayManager::handleAddUser() {}
