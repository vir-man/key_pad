#ifndef INPUT_MANAGER_H
#define INPUT_MANAGER_H

#include <Arduino.h>
#include "Adafruit_Keypad.h"
#include "Config.h"
#include "Types.h"
#include "Buzzer.h"
#include "Logger.h"

// Keypad Configuration
// ROWS/COLS must match original
const byte KEYPAD_ROWS = 4;
const byte KEYPAD_COLS = 4;

class InputManager {
public:
    static InputManager& getInstance();
    
    void begin();
    void update();
    
    // Polling interface
    char getKey(); // Returns key char if pressed, else 0
    bool isKeyPressed();
    
    // Special key status
    bool isPowerPressed();
    bool isLockPressed();
    bool isMutePressed();
    
    // For T9 or alphanum input handling in UI layers
    char getLastKey() const { return _lastKey; }
    unsigned long getLastPressTime() const { return _lastPressTime; }
    
private:
    InputManager();
    
    Adafruit_Keypad _customKeypad;
    
    char _lastKey;
    unsigned long _lastPressTime;
    bool _newKeyPress;
    
    // Special states
    bool _powerPressed;
    bool _lockPressed;
    bool _mutePressed;
    
    void handleKey(char key);
};

#endif // INPUT_MANAGER_H
