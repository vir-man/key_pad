#ifndef TYPES_H
#define TYPES_H

#include <Arduino.h>

// ==========================================
// ENUMS
// ==========================================

enum class SystemState {
    IDLE,
    AUTHENTICATING,
    DOOR_OPENING,
    DOOR_OPENED,
    DOOR_CLOSING,
    ERROR,
    ALARM
};

enum class ScreenState : uint8_t {
    MAIN_OFF = 0,
    MAIN_IDLE = 1,
    MASTER_MENU = 2,
    MASTER_ADD_USER = 3,
    MASTER_REMOVE_USER = 4,
    MASTER_CHANGE_PASS = 5,
    USER_INPUT = 15,
    BUZZER_ALERT = 16,
    FINGERPRINT_ENROLL = 17,
    FINGERPRINT_WAIT = 18,
    HOLIDAY_CONFIG = 9,
    BACKUP_MENU = 10,
    // Add other states as needed mapping to original defines
};

enum class AccessResult {
    GRANTED,
    DENIED_PIN,
    DENIED_FINGERPRINT,
    DENIED_TIME_SLOT,
    DENIED_HOLIDAY,
    ERROR_SYSTEM
};

enum class LogLevel {
    NONE = 0,
    CRITICAL = 1,
    WARNING = 2,
    INFO = 3,
    VERBOSE = 4
};

// ==========================================
// STRUCTS
// ==========================================

struct UserInfo {
    uint8_t id;
    bool hasPassword;
    char mobileNumber[11]; // 10 digits + null
    char password[16];     // 15 chars + null
    uint8_t passLength;
    bool hasTimeRestriction;
    uint8_t startHour;
    uint8_t startMin;
    uint8_t endHour;
    uint8_t endMin;
};

struct DateTime {
    uint8_t second;
    uint8_t minute;
    uint8_t hour;
    uint8_t dayOfWeek;
    uint8_t day;
    uint8_t month;
    uint8_t year;
    
    // Helper to format as string
    void toString(char* buffer) const {
        sprintf(buffer, "%02d/%02d/%02d %02d:%02d:%02d", day, month, year, hour, minute, second);
    }
};

#endif // TYPES_H
