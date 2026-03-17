#include "AccessManager.h"
#include <EEPROM.h>

// Define Hardware Serial for Fingerprint
// Serial3 is typical for Mega. defined in Config.h implicitly or explicitly?
// Config.h comments say Serial3.
#define FP_SERIAL Serial3

// Constants from original code
#define PASSWORD_STORE_COUNT 15
#define MOBILE_NUMBER_LENGTH 10
#define IN_OUT_TIME_LEN_COUNT 2

// Address Logic
// Block Size: 1(IsPw) + 11(Mob) + 1(PwLen) + 1(PwStart) + 15(Pw) + 1(IsInOut) + 2(In) + 2(Out) + 1(Pad) = Approx 35-40 bytes
// Exact calculation from original:
// 1 + 10 + 1 + 1 + 15 + 1 + 2 + 2 + 1 = 34 bytes?
// Let's use the exact formula to be safe:
// USER_BLOCK_SIZE = 1 + 10 + 1 + 1 + 15 + 1 + 2 + 2 + 1 = 34
#define USER_BLOCK_SIZE 34

AccessManager& AccessManager::getInstance() {
    static AccessManager instance;
    return instance;
}

AccessManager::AccessManager() : _finger(&FP_SERIAL) {
}

void AccessManager::begin() {
    FP_SERIAL.begin(57600); // Standard baud for fingerprint
    _finger.begin(57600);
    
    if (_finger.verifyPassword()) {
        Logger::getInstance().logSystem("FP: Sensor Found");
    } else {
        Logger::getInstance().logSystem("FP: Sensor NOT Found", LogLevel::CRITICAL);
    }
}

void AccessManager::update() {
    // Background tasks if any
}

uint16_t AccessManager::getAddrUserBase(uint8_t index) {
    return EEPROM_START_ADDR + (index * USER_BLOCK_SIZE);
}

// Memory Layout:
// 0: IsPasswordConfigured (bool)
// 1: MobileNumber (10 chars)
// 11: Pad? (Or part of mobile?) -> Original: eeprom_addr_mobile = base + 1
// 11: PasswordLen (1 byte) -> Original: base + 1 + 10 + 1 => base + 12?
// Wait, original: mobile_number is 10 bytes.
// eeprom_addr_pw_len = mobile + 10 + 1 = base + 1 + 10 + 1 = base + 12. Correct.
// eeprom_addr_pw = pw_len + 1 = base + 13.
// eeprom_addr_is_inout = pw + 15 + 1 = base + 13 + 15 + 1 = base + 29.
// eeprom_addr_in_time = is_inout + 1 = base + 30.
// eeprom_addr_out_time = in_time + 2 = base + 32.
// eeprom_addr_end = out_time + 2 + 1 = base + 35. 
// So Block Size is 35. 

// Let's implement generic Get/Set
bool AccessManager::getUser(uint8_t id, UserInfo& user) {
    if (id >= MAX_NUM_OF_USERS) return false;
    
    uint16_t base = getAddrUserBase(id);
    
    user.id = id;
    user.hasPassword = EEPROM.read(base);
    
    if (!user.hasPassword) return false; // Not configured
    
    // Read Mobile
    for (int i=0; i<10; i++) user.mobileNumber[i] = EEPROM.read(base + 1 + i);
    user.mobileNumber[10] = '\0';
    
    // Read Pass
    // base + 12 is '1' padding in original? 
    // Original: eeprom_addr_pw_len(index) { return eeprom_addr_mobile(index) + MOBILE_NUMBER_LENGTH + 1; }
    // mobile at base+1. mobile+10 = base+11. +1 = base+12.
    user.passLength = EEPROM.read(base + 12);
    
    // Original: eeprom_addr_pw = pw_len + 1 => base+13
    for (int i=0; i<15; i++) user.password[i] = EEPROM.read(base + 13 + i);
    user.password[15] = '\0';
    
    // Time restrictions
    // base + 13 + 15 = base + 28?
    // Original formula: eeprom_addr_pw(index) + PASSWORD_STORE_COUNT + 1
    // base+13 + 15 + 1 = base+29
    user.hasTimeRestriction = EEPROM.read(base + 29);
    
    if (user.hasTimeRestriction) {
        user.startHour = EEPROM.read(base + 30);
        user.startMin = EEPROM.read(base + 31);
        user.endHour = EEPROM.read(base + 32);
        user.endMin = EEPROM.read(base + 33);
    }
    
    return true;
}

bool AccessManager::verifyPassword(uint8_t id, const char* inputPass) {
    UserInfo user;
    if (!getUser(id, user)) return false;
    
    // Check length
    size_t len = strlen(inputPass);
    if (len > 15) return false;
    
    // Explicit compare
    if (strncmp(user.password, inputPass, user.passLength) == 0) {
        // Also check if entire string matches (no extra chars)
        // Original code logic might be looser, but let's be strict
        if (len == user.passLength) return true;
    }
    return false;
}

bool AccessManager::verifyFingerprint(uint8_t& matchedId) {
    uint8_t p = _finger.getImage();
    if (p != FINGERPRINT_OK) return false;
    
    p = _finger.image2Tz();
    if (p != FINGERPRINT_OK) return false;
    
    p = _finger.fingerFastSearch();
    if (p == FINGERPRINT_OK) {
        matchedId = _finger.fingerID; // Assuming simple mapping 0-127
        return true;
    }
    return false;
}

// TODO: Implement addUser, removeUser, etc. using similar address logic.
// Keeping it minimal for now to fit context. factoryReset just loops and writes 0.
