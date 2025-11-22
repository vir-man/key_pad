#ifndef RTC_HANDLER_H
#define RTC_HANDLER_H

#include <uRTCLib.h>
#include "SystemConfig.h"
#include "ErrorCodes.h"

/**
 * RTC Handler Class
 * Handles Real-Time Clock operations
 */
class RTCHandler {
private:
  uRTCLib rtc;
  unsigned long last_update_time;
  
  // Current date/time
  uint8_t second;
  uint8_t minute;
  uint8_t hour;
  uint8_t day_of_week;
  uint8_t date;
  uint8_t month;
  uint8_t year;
  
  void updateFromRTC();
  
public:
  RTCHandler();
  bool initialize();
  
  // Get current date/time
  void getTime(uint8_t* hour_out, uint8_t* minute_out);
  void getDate(uint8_t* date_out, uint8_t* month_out, uint8_t* year_out);
  void getDateTime(uint8_t* date_out, uint8_t* month_out, uint8_t* year_out,
                   uint8_t* hour_out, uint8_t* minute_out, uint8_t* second_out);
  
  // Set date/time
  ErrorCode setDateTime(uint8_t date, uint8_t month, uint8_t year,
                        uint8_t hour, uint8_t minute, uint8_t second);
  
  // Task function (call periodically)
  void task();
  
  // Getters
  uint8_t getSecond();
  uint8_t getMinute();
  uint8_t getHour();
  uint8_t getDate();
  uint8_t getMonth();
  uint8_t getYear();
};

#endif // RTC_HANDLER_H

