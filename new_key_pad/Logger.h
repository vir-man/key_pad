#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>
#include <SD.h>
#include <Ch376msc.h>
#include "Config.h"
#include "Types.h"

class Logger {
public:
    static Logger& getInstance();
    
    void begin();
    void update(); // Handle periodic tasks like backup
    
    // Core logging function
    bool logAccess(uint16_t srNo, uint8_t userId, const DateTime& dt, bool isDoorOpen);
    
    // System logging
    void logSystem(const char* msg, LogLevel level = LogLevel::INFO);
    
    // Backup control
    void triggerBackup();
    bool isBackupInProgress() const { return _backupInProgress; }

private:
    Logger(); // Singleton
    
    Ch376msc _flashDrive; // Hardware Serial object - Initialized first
    bool _sdInitialized;
    bool _flashAttached;
    bool _backupInProgress;
    
    // Helper to format log entry into buffer
    void formatLogEntry(char* buffer, uint16_t srNo, uint8_t userId, const DateTime& dt, bool isDoorOpen);
    
    // Disk operations
    void copySdToFlash();
};

#endif // LOGGER_H
