#include "InputManager.h"

// Define Keypad map
char keys[KEYPAD_ROWS][KEYPAD_COLS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};
// Check Pins (from Config.h)
byte rowPins[KEYPAD_ROWS] = {PIN_KEYPAD_ROWS[0], PIN_KEYPAD_ROWS[1], PIN_KEYPAD_ROWS[2], PIN_KEYPAD_ROWS[3]}; 
byte colPins[KEYPAD_COLS] = {PIN_KEYPAD_COLS[0], PIN_KEYPAD_COLS[1], PIN_KEYPAD_COLS[2], PIN_KEYPAD_COLS[3]}; 


// Special Keys
#define KEY_CANCEL '@' // If supported
#define KEY_ENTER  '#' 
#define KEY_POWER  '!' // Mapped? 
#define KEY_LOCK   '*' 
#define KEY_MUTE   '^' 

InputManager& InputManager::getInstance() {
    static InputManager instance;
    return instance;
}

InputManager::InputManager() 
    : _customKeypad(makeKeymap(keys), rowPins, colPins, KEYPAD_ROWS, KEYPAD_COLS),
      _lastKey(0), _lastPressTime(0), _newKeyPress(false),
      _powerPressed(false), _lockPressed(false), _mutePressed(false)
{
}

void InputManager::begin() {
    _customKeypad.begin();
    Logger::getInstance().logSystem("INPUT: Keypad Init");
}

void InputManager::update() {
    _customKeypad.tick();
    
    _newKeyPress = false;
    _powerPressed = false;
    _lockPressed = false;
    _mutePressed = false;
    
    if (_customKeypad.available()) {
        keypadEvent e = _customKeypad.read();
        
        if (e.bit.EVENT == KEY_JUST_PRESSED) {
             Buzzer::getInstance().playTone(2000, 50); // Beep on press
             _lastKey = (char)e.bit.KEY;
             _lastPressTime = millis();
             _newKeyPress = true;
             
             handleKey(_lastKey);
        }
    }
}

void InputManager::handleKey(char key) {
    if (key == KEY_POWER) _powerPressed = true;
    else if (key == KEY_LOCK) _lockPressed = true;
    else if (key == KEY_MUTE) _mutePressed = true;
}

char InputManager::getKey() {
    if (_newKeyPress) {
        _newKeyPress = false; // Consume
        return _lastKey;
    }
    return 0;
}

bool InputManager::isKeyPressed() {
    return _newKeyPress;
}

bool InputManager::isPowerPressed() { return _powerPressed; }
bool InputManager::isLockPressed() { return _lockPressed; }
bool InputManager::isMutePressed() { return _mutePressed; }
