#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Arduino.h>
#include <LiquidCrystal.h>
#include "Config.h"
#include "Types.h"
#include "InputManager.h"
#include "AccessManager.h"
#include "Sensors.h"

class DisplayManager {
public:
    static DisplayManager& getInstance();
    
    void begin();
    void update();
    
    // Power Management
    void setDisplayPower(bool on);
    
    // State Control
    void setScreen(ScreenState screen);
    
private:
    DisplayManager();
    
    LiquidCrystal _lcd;
    ScreenState _currentScreen;
    bool _displayOn;
    unsigned long _displayTimer;
    
    // UI Helpers
    void printLine(uint8_t line, const char* str);
    void printLineP(uint8_t line, const char* str_P);
    
    // Screen Handlers
    void handleMain();
    void handleUserInput();
    void handleMasterMenu();
    void handleAddUser();
    
    // Inputs
    char _inputBuffer[20];
    uint8_t _inputIndex;
    
    // Helper to clear input buffer
    void clearInput();
    void appendInput(char key);
};

#endif // DISPLAY_MANAGER_H
