#ifndef GSM_MANAGER_H
#define GSM_MANAGER_H

#include <Arduino.h>
#include "Config.h"
#include "Types.h"
#include "AccessManager.h"
#include "DoorController.h"
#include "Logger.h"

class GSMManager {
public:
    static GSMManager& getInstance();
    
    void begin();
    void update();
    
    // Outbound
    void sendSMS(const char* mobile, const char* message);
    void makeCall(const char* mobile);
    
private:
    GSMManager();
    
    // Buffer for serial data
    char _buffer[200]; 
    uint16_t _bufferIndex;
    
    // Command Parsing
    void processBuffer();
    bool parseCommand(const char* cmdStr);
    
    // API Implementations
    void apiUnlock();
    void apiLock();
    void apiAddUser(char* params);
    void apiRemoveUser(char* params);
    // ... other APIs
    
    // Helpers
    bool findMarker(const char* input, char marker, uint16_t& index);
};

#endif // GSM_MANAGER_H
