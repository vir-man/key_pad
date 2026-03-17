/*
 * Rewritten KeyPad System
 * Architecture: Modular Object-Oriented
 * Platform: Arduino Mega 2560
 */

#include <Arduino.h>
#include <SPI.h> // For SD
#include <Wire.h> // For RTC, I2C

#include "Config.h"
#include "Types.h"
#include "Logger.h"
#include "DoorController.h"
#include "Sensors.h"
#include "Buzzer.h"
#include "AccessManager.h"
#include "InputManager.h"
#include "DisplayManager.h"
#include "GSMManager.h"

void setup() {
    // 1. Core Init
    Serial.begin(SERIAL_DEBUG_BAUD);
    Wire.begin();
    
    Logger::getInstance().logSystem("SYS: Booting...");

    // 2. Hardware Abstraction Init
    Logger::getInstance().begin();
    Sensors::getInstance().begin();
    Buzzer::getInstance().begin();
    DoorController::getInstance().begin();
    
    // 3. Logic & Input Init
    AccessManager::getInstance().begin();
    InputManager::getInstance().begin();
    GSMManager::getInstance().begin();
    
    // 4. UI Init
    DisplayManager::getInstance().begin();
    
    Logger::getInstance().logSystem("SYS: Ready");
}

void loop() {
    // Cooperative Scheduler - Call update() on all managers
    
    // 1. Hardware Updates
    Sensors::getInstance().update();
    Buzzer::getInstance().update();
    DoorController::getInstance().update();
    Logger::getInstance().update(); // Handles backups
    
    // 2. Input Processing
    InputManager::getInstance().update();
    GSMManager::getInstance().update();
    
    // 3. Logic (AccessManager updates often minimal, mostly event driven)
    AccessManager::getInstance().update();
    
    // 4. UI Update (Renders current state)
    DisplayManager::getInstance().update();
    
    // 5. Watchdog reset if needed
    // wdt_reset(); 
}
