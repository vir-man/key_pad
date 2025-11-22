#ifndef TEMPERATURE_MONITOR_H
#define TEMPERATURE_MONITOR_H

#include <OneWire.h>
#include <DallasTemperature.h>
#include "SystemConfig.h"
#include "ErrorCodes.h"

/**
 * Temperature Monitor Class
 * Monitors temperature sensor and triggers alarms
 */
class TemperatureMonitor {
private:
  OneWire* oneWire;
  DallasTemperature* sensors;
  unsigned long last_read_time;
  
  int temperature_value;
  int prev_temperature_value;
  bool alarm_triggered;
  
public:
  TemperatureMonitor();
  bool initialize();
  
  // Temperature operations
  ErrorCode readTemperature();
  int getTemperature();
  bool isAlarmTriggered();
  void resetAlarm();
  
  // Task function (call in loop)
  void task();
};

#endif // TEMPERATURE_MONITOR_H

