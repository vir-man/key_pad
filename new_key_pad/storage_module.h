#ifndef STORAGE_MODULE_H
#define STORAGE_MODULE_H

#include <Arduino.h>
#include <EEPROM.h>
#include <SD.h>
#include "Ch376msc.h"
#include "config.h"

#define MOBILE_NUMBER_LENGTH 10
#define PASSWORD_STORE_COUNT 15

// External globals from EEPROM management
extern bool is_password_configured[MAX_NUM_OF_USERS];
extern char mobile_number[MAX_NUM_OF_USERS][MOBILE_NUMBER_LENGTH];
extern uint8_t password_length[MAX_NUM_OF_USERS];
extern char password_value[MAX_NUM_OF_USERS][PASSWORD_STORE_COUNT];

extern bool is_in_out_time_configured[MAX_NUM_OF_USERS];
extern uint8_t in_time_hour[MAX_NUM_OF_USERS];
extern uint8_t in_time_minute[MAX_NUM_OF_USERS];
extern uint8_t out_time_hour[MAX_NUM_OF_USERS];
extern uint8_t out_time_minute[MAX_NUM_OF_USERS];

extern uint16_t alpha_speed;
extern uint16_t buzzer_timeout;
extern uint16_t door_open_count;

extern char _mobile_number[10];
extern char _password[15];
extern char _password1[15];

// USB/SD status flags
extern bool b_flash_drive_attached;
extern bool b_backup_in_progress;
extern bool b_backup_complete;
extern bool b_sd_card_not_initiated;

extern HardwareSerial *port;

// Time references needed for SD
extern uint8_t second;
extern uint8_t minute;
extern uint8_t hour;
extern uint8_t date;
extern uint8_t month;
extern uint8_t year;

// Init Methods
void init_eeprom();
void init_flash_drive();
void sd_init();

// Storage Task
void flash_drive_task();
void copy_data_from_sd_card_to_usb_flash_drive();
void printInfo(const char info[]);

// EEPROM Data Methods
void clear_eeprom();
void clear_eeprom_data();
void print_eeprom_data(HardwareSerial *serial1);
void update_data_from_eeprom();
bool update_eeprom_data_at_index(uint8_t index, char *mobile_number_to_add, char *password_to_add, uint8_t len);
bool check_if_password_is_configured(uint8_t index);
uint8_t update_length_of_password(uint8_t index);
bool update_in_out_time_to_eeprom(uint8_t index, uint8_t _in_time_hour, uint8_t _in_time_minute, uint8_t _out_time_hour, uint8_t _out_time_minute);
bool clear_in_out_time_to_eeprom(uint8_t index);
bool update_password_from_eeprom(uint8_t index);
bool read_in_out_time_from_eeprom(uint8_t index);
bool save_password_to_eeprom(uint8_t index, char *password_to_add, uint8_t len);
bool update_password_to_eeprom(uint8_t index);
void clear_password_in_eeprom(uint8_t index);
int8_t check_if_mobile_number_exists(char *arr);

bool is_password_matched_with_any_user(char *arr, uint8_t len);
bool is_password_matched(uint8_t index, char *arr, uint8_t len);

void update_log_entry(uint16_t counter, uint8_t id, bool state); // SD Card logging

// Core Read/Write Eeprom
void WriteEepromArray(size_t address, char *arr, size_t len);
void clearEepromArray(size_t start_address, size_t end_address);
void LoadFromEeprom(size_t address, char *arr, size_t len);
bool check_is_configured_byte_address(size_t address);
bool configure_byte_address(size_t address);
bool clear_byte_address(size_t address);

// Specific Values Configuration Helpers
void set_eeprom_addresses();
void write_alpha_speed_to_eeprom(uint16_t _alpha_speed);
void write_buzzer_timeout_to_eeprom(uint16_t _buzzer_timeout);
uint16_t read_buzzer_timeout_to_eeprom();
uint16_t read_alpha_speed_to_eeprom();
void write_door_open_count_to_eeprom(uint16_t _door_open_count);
uint16_t read_door_open_count_to_eeprom();

#endif // STORAGE_MODULE_H
