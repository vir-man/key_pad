#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>

// Debug Levels
#define LOG_LEVEL_NONE    0
#define LOG_LEVEL_ERROR   1
#define LOG_LEVEL_WARN    2
#define LOG_LEVEL_INFO    3
#define LOG_LEVEL_DEBUG   4

// Set the global debug level here
#ifndef DEBUG_LEVEL
#define DEBUG_LEVEL 4
#endif

// Module Definitions
#define MOD_MAIN   "MAN"
#define MOD_AUTH   "AUT"
#define MOD_DOOR   "DOR"
#define MOD_GSM    "GSM"
#define MOD_MEM    "MEM"
#define MOD_UI     "UI "
#define MOD_SENS   "SNS"
#define MOD_RTC    "RTC"
#define MOD_BUZZ   "BUZ"

void log_print(uint8_t level, const char* module, const char* msg);
void log_print(uint8_t level, const char* module, const __FlashStringHelper* msg);
void log_print_val(uint8_t level, const char* module, const char* msg, int val);

#if DEBUG_LEVEL >= LOG_LEVEL_ERROR
  #define LOG_ERROR(mod, msg) log_print(LOG_LEVEL_ERROR, mod, msg)
#else
  #define LOG_ERROR(mod, msg)
#endif

#if DEBUG_LEVEL >= LOG_LEVEL_WARN
  #define LOG_WARN(mod, msg) log_print(LOG_LEVEL_WARN, mod, msg)
#else
  #define LOG_WARN(mod, msg)
#endif

#if DEBUG_LEVEL >= LOG_LEVEL_INFO
  #define LOG_INFO(mod, msg) log_print(LOG_LEVEL_INFO, mod, msg)
#else
  #define LOG_INFO(mod, msg)
#endif

#if DEBUG_LEVEL >= LOG_LEVEL_DEBUG
  #define LOG_DEBUG(mod, msg) log_print(LOG_LEVEL_DEBUG, mod, msg)
  #define LOG_DEBUG_VAL(mod, msg, val) log_print_val(LOG_LEVEL_DEBUG, mod, msg, val)
#else
  #define LOG_DEBUG(mod, msg)
  #define LOG_DEBUG_VAL(mod, msg, val)
#endif

// Legacy macros mapping to MAIN module for backward compatibility during transition
#define DBG_L1(...) Serial.print(__VA_ARGS__)
#define DBG_L1_PRINT(...) Serial.print(__VA_ARGS__)
#define DBG_L1_PRINTLN(...) Serial.println(__VA_ARGS__)
#define DBG_L2(...) Serial.print(__VA_ARGS__)
#define DBG_L2_PRINT(...) Serial.print(__VA_ARGS__)
#define DBG_L2_PRINTLN(...) Serial.println(__VA_ARGS__)
#define DBG_L3(...) Serial.print(__VA_ARGS__)
#define DBG_L3_PRINT(...) Serial.print(__VA_ARGS__)
#define DBG_L3_PRINTLN(...) Serial.println(__VA_ARGS__)
#define DBG_L4(...) Serial.print(__VA_ARGS__)
#define DBG_L4_PRINT(...) Serial.print(__VA_ARGS__)
#define DBG_L4_PRINTLN(...) Serial.println(__VA_ARGS__)

#endif // LOGGER_H
