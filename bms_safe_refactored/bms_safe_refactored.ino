/*
 * BMS SAFE Security System
 * Complete class-based refactored version
 * For Arduino Mega 2560
 */

#include "MainSystem.h"

// Global system instance (renamed from 'system' to avoid conflict with stdlib)
MainSystem* mainSystem = nullptr;

void setup() {
  // Initialize Serial for debugging
  Serial.begin(115200);
  while (!Serial) {
    ; // Wait for serial port to connect
  }
  
  Serial.println(F("BMS SAFE Security System"));
  Serial.println(F("Initializing..."));
  
  // Create and initialize main system
  mainSystem = new MainSystem();
  
  if (!mainSystem->initialize()) {
    Serial.println(F("ERROR: System initialization failed!"));
    // System may still be partially functional
  } else {
    Serial.println(F("System initialized successfully!"));
  }
  
  Serial.println(F("System ready!"));
}

void loop() {
  if (mainSystem != nullptr && mainSystem->isInitialized()) {
    // Main system task - coordinates all subsystems
    mainSystem->task();
  } else {
    // System not initialized - try to recover
    delay(1000);
    if (mainSystem != nullptr) {
      mainSystem->initialize();
    }
  }
  
  // Small delay to prevent watchdog issues
  delay(10);
}

