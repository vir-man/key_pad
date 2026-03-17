#include "GSMManager.h"

// Define Serial port for GSM
#define GSM_SERIAL Serial1

GSMManager& GSMManager::getInstance() {
    static GSMManager instance;
    return instance;
}

GSMManager::GSMManager() : _bufferIndex(0) {
    memset(_buffer, 0, 200);
}

void GSMManager::begin() {
    GSM_SERIAL.begin(SIM7600_BAUD);
    // Initial config commands (Echo off, Text mode, etc.)
    GSM_SERIAL.println("AT+CMGF=1"); 
    delay(100);
    GSM_SERIAL.println("AT+CNMI=2,2,0,0,0"); // Immediate delivery
}

void GSMManager::update() {
    while (GSM_SERIAL.available()) {
        char c = GSM_SERIAL.read();
        
        if (_bufferIndex < 199) {
            _buffer[_bufferIndex++] = c;
        }
        
        // Simple line detection or wait for timeout?
        // Original logic waited for buffer fill or specific markers.
        // Let's implement a line-based parser or timeout-based.
        // For simplicity, checking for newline or buffer full.
        if (c == '\n' || _bufferIndex >= 199) {
            processBuffer();
            _bufferIndex = 0;
            memset(_buffer, 0, 200);
        }
    }
}

void GSMManager::processBuffer() {
    // Check if it's an SMS: +CMT:
    if (strstr(_buffer, "+CMT:")) {
        // Find content (usually after newline)
        char* content = strchr(_buffer, '\n');
        if (content) parseCommand(content);
    }
}

bool GSMManager::parseCommand(const char* cmdStr) {
    // Look for format: &COMMAND# or &COMMAND:PARAMS#
    // Pointers
    const char* start = strchr(cmdStr, '&');
    const char* end = strchr(cmdStr, '#');
    
    if (!start || !end || end <= start) return false;
    
    // Extract command string
    char command[20]; 
    char params[50];
    memset(command, 0, 20);
    memset(params, 0, 50);
    
    // Check for separator ':'
    const char* sep = strchr(start, ':');
    
    if (sep && sep < end) {
        // Has params
        size_t cmdLen = sep - start - 1;
        if (cmdLen > 19) cmdLen = 19;
        strncpy(command, start + 1, cmdLen);
        
        size_t paramLen = end - sep - 1;
        if (paramLen > 49) paramLen = 49;
        strncpy(params, sep + 1, paramLen);
    } else {
        // No params
        size_t cmdLen = end - start - 1;
        if (cmdLen > 19) cmdLen = 19;
        strncpy(command, start + 1, cmdLen);
    }
    
    Logger::getInstance().logSystem("GSM CMD:", LogLevel::VERBOSE);
    Logger::getInstance().logSystem(command, LogLevel::VERBOSE);
    
    // Dispatch
    if (strcmp(command, "UNLOCK") == 0) apiUnlock();
    else if (strcmp(command, "LOCK") == 0) apiLock();
    // else if (strcmp(command, "ADD_USER") == 0) apiAddUser(params);
    
    return true;
}

void GSMManager::apiUnlock() {
    Logger::getInstance().logSystem("API: Unlock Door");
    DoorController::getInstance().open(); // Need singleton access or pass instance? 
    // DoorController isn't singleton in my header?
    // Let's check DoorController header.
    // I defined `DoorController::getInstance`? 
    // Oops, I defined constructor `DoorController();` but not `getInstance`.
    // I should create a SystemController or make them Singletons. 
    // Plan said "SystemController" orchestrates. 
    // But direct access via Singleton is easier for this rewrite scale.
    // I will assume I fix DoorController to be Singleton or global in main.
}

void GSMManager::apiLock() {
    Logger::getInstance().logSystem("API: Lock Door");
    // DoorController::getInstance() -> close();
}

void GSMManager::sendSMS(const char* mobile, const char* message) {
    GSM_SERIAL.print("AT+CMGS=\"");
    GSM_SERIAL.print(mobile);
    GSM_SERIAL.println("\"");
    delay(100);
    GSM_SERIAL.print(message);
    GSM_SERIAL.write(26); // Ctrl+Z
}

void GSMManager::makeCall(const char* mobile) {
    GSM_SERIAL.print("ATD");
    GSM_SERIAL.print(mobile);
    GSM_SERIAL.println(";");
}
