#ifndef SYSTEM_CONFIG_H
#define SYSTEM_CONFIG_H

#include <Arduino.h>

/**
 * System Configuration Class
 * Contains all constants, pin definitions, and system limits
 * Optimized for Arduino Mega 2560
 */
class SystemConfig {
public:
  // User Limits
  static constexpr uint8_t MAX_USER_TO_BE_STORED = 10;
  static constexpr uint8_t MAX_NUM_OF_USERS = MAX_USER_TO_BE_STORED;
  static constexpr uint8_t MASTER_USER_ID = 1;
  static constexpr uint8_t MASTER_USER_INDEX = 0;
  
  // Password Limits
  static constexpr uint8_t MIN_PASSWORD_LEN = 4;
  static constexpr uint8_t MAX_PASSWORD_LEN = 15;
  static constexpr uint8_t PASSWORD_STORE_COUNT = 15;
  static constexpr uint8_t MOBILE_NUMBER_LENGTH = 10;
  static constexpr uint8_t MASTER_PW_LEN = 10;
  
  // Holiday Limits
  static constexpr uint8_t MAX_HOLIDAYS = 20;
  static constexpr uint8_t HOLIDAY_DATA_SIZE = 3;  // date + month + year
  
  // EEPROM Configuration
  static constexpr uint16_t EEPROM_STARTING_ADDRESS = 0;
  static constexpr uint8_t IS_CONFIGURED_BYTE_ADDRESS_COUNT = 1;
  static constexpr uint8_t PASSWORD_LEN_COUNT = 1;
  static constexpr uint8_t IN_OUT_TIME_LEN_COUNT = 2;
  static constexpr uint8_t ALPHA_SPEED_LEN_COUNT = 4;
  static constexpr uint8_t BUZZER_TIMEOUT_LEN_COUNT = 4;
  static constexpr uint8_t DOOR_OPEN_COUNT_LEN_COUNT = 4;
  static constexpr uint8_t HOLIDAY_COUNT_SIZE = 1;
  
  // Pin Definitions
  static constexpr uint8_t DC_MOTOR_PIN_0 = 2;
  static constexpr uint8_t DC_MOTOR_PIN_1 = 5;
  static constexpr uint8_t EM_LOCK_CONTROL_PIN = 6;
  static constexpr uint8_t SENSOR_PIN_OPEN = 48;
  static constexpr uint8_t SENSOR_PIN_CLOSE = 47;
  static constexpr uint8_t IR_RX_PIN = A0;
  static constexpr uint8_t BUZZER_PIN = 45;
  static constexpr uint8_t ONE_WIRE_BUS = 42;
  static constexpr uint8_t SIREN_PIN_0 = 27;
  static constexpr uint8_t SIREN_PIN_1 = 28;
  static constexpr uint8_t LCD_GND_PIN = A11;
  static constexpr uint8_t LCD_VCC_PIN = A10;
  static constexpr uint8_t LCD_RS_PIN = A12;
  static constexpr uint8_t LCD_EN_PIN = 22;
  static constexpr uint8_t LCD_D4_PIN = 23;
  static constexpr uint8_t LCD_D5_PIN = 24;
  static constexpr uint8_t LCD_D6_PIN = 25;
  static constexpr uint8_t LCD_D7_PIN = 26;
  static constexpr uint8_t BATTERY_ANALOG_PIN = A14;
  static constexpr uint8_t ON_SWITCH_PIN = 40;
  
  // Keypad row pins
  static constexpr uint8_t KEYPAD_ROW_0 = 36;
  static constexpr uint8_t KEYPAD_ROW_1 = 34;
  static constexpr uint8_t KEYPAD_ROW_2 = 32;
  static constexpr uint8_t KEYPAD_ROW_3 = 30;
  
  // Keypad column pins
  static constexpr uint8_t KEYPAD_COL_0 = 37;
  static constexpr uint8_t KEYPAD_COL_1 = 35;
  static constexpr uint8_t KEYPAD_COL_2 = 33;
  static constexpr uint8_t KEYPAD_COL_3 = 31;
  
  // Serial Ports
  static constexpr uint8_t FINGERPRINT_SERIAL = 3;  // Serial3
  static constexpr uint8_t GSM_SERIAL = 1;  // Serial1
  static constexpr uint32_t GSM_BAUD_RATE = 115200;
  
  // Timeouts (milliseconds)
  static constexpr uint16_t TEMPERATURE_THRESHOLD = 60;  // Celsius
  static constexpr uint32_t DOOR_OPEN_TIMEOUT = 10000;
  static constexpr uint32_t DOOR_OPEN_ERROR_TIMEOUT = 5000;
  static constexpr uint32_t DOOR_CLOSE_TIMEOUT = 5000;
  static constexpr uint32_t GUN_POINT_PRESS_TIMEOUT = 3000;
  static constexpr uint32_t DISPLAY_ON_TIMEOUT = 3600000UL; // 60 minutes in milliseconds (60 * 60 * 1000)
  static constexpr uint32_t SMS_MASTER_VERIFY_TIMEOUT = 60000;
  static constexpr uint32_t TEMPERATURE_READ_INTERVAL = 10000;
  static constexpr uint32_t RTC_UPDATE_INTERVAL = 60000;  // 1 minute
  static constexpr uint32_t KEY_DEBOUNCE_DELAY = 5;
  
  // Message Queue
  static constexpr uint8_t MAX_QUEUE_SIZE = 10;
  static constexpr uint8_t MAX_CMD_LEN = 15;
  static constexpr uint8_t MAX_PARA_LEN = 12;
  static constexpr uint8_t MAX_PARAMETER = 5;
  static constexpr uint8_t SMS_SERIAL_BUFFER_SIZE = 100;
  static constexpr uint8_t CHAR_ARRAY_SIZE = 60;
  
  // SMS Command Parsing
  static constexpr char MSG_START_CHAR = '&';
  static constexpr char MSG_END_CHAR = '#';
  static constexpr char CMD_SEPARATOR = ',';
  
  // Message Types
  static constexpr uint8_t OPEN_DOOR_MSG = 1;
  static constexpr uint8_t CLOSE_DOOR_MSG = 2;
  static constexpr uint8_t GUN_POINT_MSG = 3;
  static constexpr uint8_t GUN_POINT_CALL = 4;
  static constexpr uint8_t TEMP_ALARM_MSG = 5;
  static constexpr uint8_t VIBRATION_ALARM_MSG = 6;
  static constexpr uint8_t AUTH_FAIL_MSG = 7;
  
  // Command Response Codes
  static constexpr uint8_t CMD_NOT_FOUND = 0;
  static constexpr uint8_t CMD_EXECUTED = 1;
  
  // Default Values
  static constexpr uint16_t DEFAULT_ALPHA_SPEED = 200;
  static constexpr uint16_t DEFAULT_BUZZER_TIMEOUT = 10;
  
  // Master Reset Password (stored in PROGMEM)
  static const char* getMasterResetPassword() {
    static const char master_pw[] PROGMEM = "9925366111";
    return master_pw;
  }
  
  // Master OTP (for emergency)
  static constexpr uint8_t MASTER_OTP[6] = {4, 5, 5, 5, 5, 6};
  
  // Calculate EEPROM block size
  static constexpr uint16_t USER_BLOCK_SIZE = 
    1 +  // is_pw configured flag
    MOBILE_NUMBER_LENGTH + 
    1 +  // password length
    1 +  // password start marker
    PASSWORD_STORE_COUNT + 
    1 +  // is_in_out configured flag
    IN_OUT_TIME_LEN_COUNT +  // in time
    IN_OUT_TIME_LEN_COUNT +  // out time
    1;   // padding
};

#endif // SYSTEM_CONFIG_H

