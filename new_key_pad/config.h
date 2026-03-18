#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// Version
#define BMS_SAFE_VERSION "1.0.0"

// Users and Hardware Limits
#define MAX_NUM_OF_USERS 28
#define MOBILE_NUMBER_LENGTH 10
#define PASSWORD_STORE_COUNT 15

// EEPROM Configurations
#define EEPROM_STARTING_ADDRESS 0
#define IN_OUT_TIME_LEN_COUNT 2

// PIN DEFINITIONS
#define ONE_WIRE_BUS 42
#define BUZZER_PIN 45
#define ON_SWITCH_PIN 40
#define IR_INPUT_PIN A0
#define ANALOG_IN_PIN A14 // Battery
#define EM_LOCK_CONTROL_PIN 6
#define SD_CHIP_SELECT 53

// Display Pins
#define LCD_RS A12
#define LCD_EN 22
#define LCD_D4 23
#define LCD_D5 24
#define LCD_D6 25
#define LCD_D7 26

// Message Types
#define OPEN_DOOR_MSG 1
#define CLOSE_DOOR_MSG 2
#define GUN_POINT_MSG 3
#define GUN_POINT_CALL 4
#define TEMP_ALARM_MSG 5
#define VIBRATION_ALARM_MSG 6
#define AUTH_FAIL_MSG 7
#define DOOR_TIMEOUT_MSG 8

// FSM states are defined in lcd.h as macros

// Missing Extracted Globals
#include <LiquidCrystal.h>
#include <uRTCLib.h>
extern LiquidCrystal lcd;
extern uRTCLib rtc;
#define LCD_PRINT lcd.print
#define MAX_USER_TO_BE_STORED 5
extern uint8_t pass_length;

#endif // CONFIG_H
