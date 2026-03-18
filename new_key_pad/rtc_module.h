#ifndef RTC_MODULE_H
#define RTC_MODULE_H

#include <Arduino.h>
#include <Wire.h>
#include "uRTCLib.h"
#include "config.h"

// External references to time variables to avoid breakage
extern uint8_t second, minute, hour, day_of_week, date, month, year;

// Holiday limits
#define MAX_HOLIDAYS 20
#define HOLIDAY_DATA_SIZE 3
#define HOLIDAY_COUNT_SIZE 1

extern uint8_t holiday_count;
extern size_t holiday_count_start_address;
extern size_t holiday_data_start_address;

// RTC setup and tasks
void rtc_begin();
void rtc_task();
void update_date_time_from_rtc();
bool validate_date_and_time(uint8_t _second, uint8_t _minute, uint8_t _hour, uint8_t _date, uint8_t _month, uint8_t _year);
void set_rtc();

const char* DayAsString_P(uint8_t day);
void printDayName(uint8_t day);

// Holiday Management
void write_holiday_to_eeprom(uint8_t index, uint8_t _date, uint8_t _month, uint8_t _year);
void write_holidays_to_eeprom();
void read_holidays_from_eeprom();
bool is_holiday(uint8_t _date, uint8_t _month, uint8_t _year);
bool add_holiday(uint8_t _date, uint8_t _month, uint8_t _year);
bool remove_holiday(uint8_t _date, uint8_t _month, uint8_t _year);
void print_holidays();

// Helper
void update_rtc_timer();

#endif // RTC_MODULE_H
