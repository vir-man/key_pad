#ifndef ACCESS_MANAGER_H
#define ACCESS_MANAGER_H

#include <Arduino.h>
// #include <EEPROM.h> // Moved to cpp to avoid unused warning in other modules
#include <Adafruit_Fingerprint.h>
#include "Config.h"
#include "Types.h"
#include "Logger.h"

class AccessManager {
public:
    static AccessManager& getInstance();
    
    void begin();
    void update();
    
    // User Management
    bool getUser(uint8_t id, UserInfo& user);
    bool addUser(const UserInfo& user);
    bool removeUser(uint8_t id);
    bool verifyPassword(uint8_t id, const char* inputPass);
    bool verifyFingerprint(uint8_t& matchedId);
    
    // Admin
    void factoryReset();
    
    // Setters for Global Settings stored in EEPROM
    void setAlphaSpeed(uint16_t speed);
    void setDoorOpenCount(uint16_t count);
    void setBuzzerTimeout(uint16_t timeout); // in minutes
    
    uint16_t getAlphaSpeed();
    
private:
    AccessManager();
    
    Adafruit_Fingerprint _finger;
    
    // EEPROM Helpers
    uint16_t getAddrUserBase(uint8_t id);
    // ... specific field accessors implemented in cpp
    
    // Cache or direct read? Direct read reduces RAM usage.
};

#endif // ACCESS_MANAGER_H
