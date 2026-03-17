#ifndef DOOR_CONTROLLER_H
#define DOOR_CONTROLLER_H

#include <Arduino.h>
#include "Config.h"
#include "Types.h"
#include "Logger.h"

class DoorController {
public:
    static DoorController& getInstance(); // Added Singleton Accessor
    
    void begin();
    void update(); // Main task to be called in loop
    
    // Command Interface
    void open();
    void close();
    void stop();
    
    // Status Interface
    SystemState getState() const { return _currentState; }
    bool isOpen() const;
    bool isClosed() const;
    bool isAligned(); // IR Check
    
private:
    DoorController();
    
    SystemState _currentState;
    unsigned long _motorStartTime;
    
    // Internal helpers
    void setMotorForward();
    void setMotorReverse();
    void setMotorStop();
    
    void handleOpening();
    void handleClosing();
    
    // Pins (cached or macro) - using direct digitalReads for speed
};

#endif // DOOR_CONTROLLER_H
