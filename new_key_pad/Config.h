#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ==========================================
// PIN DEFINITIONS
// ==========================================

// Communications
#define PIN_ONE_WIRE_BUS        42
#define PIN_SD_CHIP_SELECT      53
// Serial1: SIM7600 (Tx=18, Rx=19) - Hardware Serial
// Serial2: Flash Drive (Tx=16, Rx=17) - Hardware Serial
// Serial3: Fingerprint (Tx=14, Rx=15) - Hardware Serial

// LCD Pins
#define PIN_LCD_RS              A12
#define PIN_LCD_EN              22
#define PIN_LCD_D4              23
#define PIN_LCD_D5              24
#define PIN_LCD_D6              25
#define PIN_LCD_D7              26
#define PIN_LCD_VCC             A10
#define PIN_LCD_GND             A11

// Sensors & Inputs
#define PIN_BUZZER              45
// Motor & Door Pins
#define PIN_MOTOR_A             2   
#define PIN_MOTOR_B             5 
#define PIN_DOOR_SENSOR_OPEN    48
#define PIN_DOOR_SENSOR_CLOSED  47
#define PIN_IR_RX               A0  
#define PIN_VIBRATION_SENSOR    30 // Verify this one remains 30 or find typical usage
#define PIN_TEMP_DS18B20        42 // Defined as ONE_WIRE_BUS

// Siren
#define PIN_SIREN_1             27
#define PIN_SIREN_2             28

// Keypad Pins
// Rows: 36, 34, 32, 30. Cols: 37, 35, 33, 31
static const byte PIN_KEYPAD_ROWS[4] = {36, 34, 32, 30};
static const byte PIN_KEYPAD_COLS[4] = {37, 35, 33, 31};



// ==========================================
// SYSTEM SETTINGS
// ==========================================

// Debug Level (0=None, 1=Critical, 2=Warn, 3=Info, 4=Verbose)
#define DEBUG_LEVEL             4

// Timeouts (Milliseconds)
#define TIMEOUT_DOOR_OPEN       10000UL // Default, can be overridden by EEPROM
#define TIMEOUT_DISPLAY_ON      30000UL
#define TIMEOUT_RTC_UPDATE      60000UL
#define TIMEOUT_Buzzer_Interval 1000UL

// Access Control
#define MAX_NUM_OF_USERS        10
#define EEPROM_START_ADDR       0
#define MOBILE_NUM_LEN          10
#define MAX_PASSWORD_LEN        15

// Hardware Constants
#define SIM7600_BAUD            115200
#define CH376_BAUD              115200
#define SERIAL_DEBUG_BAUD       115200

// EEPROM Memory Map Offsets
// Defined relative to previous blocks to serve as the unified source of truth
// (Implementation detailed in AccessManager.h)

#endif // CONFIG_H
