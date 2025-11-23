#include "RTCHandler.h"

RTCHandler::RTCHandler() : rtc(0x68), last_update_time(0),
  second(0), minute(0), hour(0), day_of_week(0), date(0), month(0), year(0) {
}

bool RTCHandler::initialize() {
  Wire.begin();
  
  // Uncomment to set RTC for first time, then comment out
  // rtc.set(0, 39, 0, 2, 27, 12, 22);
  // Format: Seconds, Minute, Hour, Day of Week, Day, Month, Year
  
  updateFromRTC();
  last_update_time = millis();
  
  return true;
}

void RTCHandler::updateFromRTC() {
  rtc.refresh();
  second = rtc.second();
  minute = rtc.minute();
  hour = rtc.hour();
  day_of_week = rtc.dayOfWeek();
  date = rtc.day();
  month = rtc.month();
  year = rtc.year();
}

void RTCHandler::getTime(uint8_t* hour_out, uint8_t* minute_out) {
  if (hour_out != nullptr) {
    *hour_out = hour;
  }
  if (minute_out != nullptr) {
    *minute_out = minute;
  }
}

void RTCHandler::getDate(uint8_t* date_out, uint8_t* month_out, uint8_t* year_out) {
  if (date_out != nullptr) {
    *date_out = date;
  }
  if (month_out != nullptr) {
    *month_out = month;
  }
  if (year_out != nullptr) {
    *year_out = year;
  }
}

void RTCHandler::getDateTime(uint8_t* date_out, uint8_t* month_out, uint8_t* year_out,
                             uint8_t* hour_out, uint8_t* minute_out, uint8_t* second_out) {
  getDate(date_out, month_out, year_out);
  getTime(hour_out, minute_out);
  if (second_out != nullptr) {
    *second_out = second;
  }
}

ErrorCode RTCHandler::setDateTime(uint8_t date_val, uint8_t month_val, uint8_t year_val,
                                  uint8_t hour_val, uint8_t minute_val, uint8_t second_val) {
  if (date_val == 0 || date_val > 31 || month_val == 0 || month_val > 12 || year_val > 99 ||
      hour_val > 23 || minute_val > 59 || second_val > 59) {
    return ErrorCode::SMS_INVALID_PARAMETERS;
  }
  
  // Calculate day of week (simplified - may need proper calculation)
  uint8_t dow = 1;  // Default to Sunday
  
  rtc.set(second_val, minute_val, hour_val, dow, date_val, month_val, year_val);
  updateFromRTC();
  
  return ErrorCode::SUCCESS;
}

void RTCHandler::task() {
  // Update from RTC periodically
  if (millis() - last_update_time > SystemConfig::RTC_UPDATE_INTERVAL) {
    updateFromRTC();
    last_update_time = millis();
  }
}

uint8_t RTCHandler::getSecond() {
  return second;
}

uint8_t RTCHandler::getMinute() {
  return minute;
}

uint8_t RTCHandler::getHour() {
  return hour;
}

uint8_t RTCHandler::getDate() {
  return date;
}

uint8_t RTCHandler::getMonth() {
  return month;
}

uint8_t RTCHandler::getYear() {
  return year;
}

