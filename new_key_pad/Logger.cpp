#include "Logger.h"

// Define the file header in PROGMEM to save RAM
const char FILE_HEADER[] PROGMEM = "SR.    USER      DATE         TIME      REMARKS\n---------------------------------------------\n";
const char LOG_FILENAME[] = "BMS-LOG1.TXT";

// Singleton instance
Logger& Logger::getInstance() {
    static Logger instance;
    return instance;
}

// Constructor: Initialize Flash Drive with Hardware Serial
Logger::Logger() : _flashDrive(Serial2, CH376_BAUD), _sdInitialized(false), _flashAttached(false), _backupInProgress(false) {
}

void Logger::begin() {
    // Initialize SD Card
    if (SD.begin(PIN_SD_CHIP_SELECT)) {
        _sdInitialized = true;
    }
    
    // Initialize Flash Drive Module
    _flashDrive.init();
}

void Logger::update() {
    // Check Flash Drive status
    if (_flashDrive.checkIntMessage()) {
        if (_flashDrive.getDeviceStatus()) {
            if (!_flashAttached) {
                // Drive just inserted
                _flashAttached = true;
                // Auto-trigger backup on insertion? Configurable.
            }
        } else {
            _flashAttached = false;
        }
    }
    
    // Process async backup if active
    if (_backupInProgress) {
        copySdToFlash();
    }
}

void Logger::formatLogEntry(char* buffer, uint16_t srNo, uint8_t userId, const DateTime& dt, bool isDoorOpen) {
    // Buffer size must be sufficient (approx 60 chars)
    // SR(3) + Tab + User(3) + Tab + Date(10) + Tab + Time(8) + Tab + Dir(1) + CR/LF
    sprintf(buffer, "%03d     %03d     %02d/%02d/%02d     %02d:%02d:%02d     %c",
            srNo, userId, dt.day, dt.month, dt.year, dt.hour, dt.minute, dt.second,
            isDoorOpen ? 'O' : 'C');
}

bool Logger::logAccess(uint16_t srNo, uint8_t userId, const DateTime& dt, bool isDoorOpen) {
    if (!_sdInitialized) return false;
    
    File dataFile = SD.open(LOG_FILENAME, FILE_WRITE);
    if (dataFile) {
        char buffer[80];
        formatLogEntry(buffer, srNo, userId, dt, isDoorOpen);
        dataFile.println(buffer);
        dataFile.close();
        
        // Also print to Serial for debug
        #if DEBUG_LEVEL >= 3
        Serial.println(buffer);
        #endif
        return true;
    }
    return false;
}

void Logger::triggerBackup() {
    if (_sdInitialized && _flashAttached) {
        _backupInProgress = true;
    }
}

// Simplified blocking backup for now (refining to async requires more state management)
// TODO: Make this non-blocking state machine in Phase 6
void Logger::copySdToFlash() {
    if (!_sdInitialized || !_flashAttached) {
        _backupInProgress = false;
        return;
    }
    
    File dataFile = SD.open(LOG_FILENAME);
    if (!dataFile) {
        _backupInProgress = false;
        return;
    }
    
    _flashDrive.setFileName(LOG_FILENAME);
    _flashDrive.openFile();
    
    // Write Header
    char pgmBuffer[80];
    strcpy_P(pgmBuffer, FILE_HEADER);
    _flashDrive.writeFile(pgmBuffer, strlen(pgmBuffer));
    
    // Copy data
    char buffer[100]; // Read buffer
    while (dataFile.available()) {
        // Read line by line is safer for text files
        // but block read is faster. Sticking to line for safety as per original code.
        int bytesRead = dataFile.readBytesUntil('\n', buffer, 99);
        buffer[bytesRead] = '\n'; // Restore newline
        buffer[bytesRead + 1] = '\0';
        _flashDrive.writeFile(buffer, bytesRead + 1);
    }
    
    _flashDrive.closeFile();
    dataFile.close();
    
    _backupInProgress = false; // Done
}

void Logger::logSystem(const char* msg, LogLevel level) {
    // Simple verification log
    if ((int)level <= DEBUG_LEVEL) {
        Serial.print(F("[SYS] "));
        Serial.println(msg);
    }
}
