#ifndef BATTERY_MONITOR_H
#define BATTERY_MONITOR_H

#include "SystemConfig.h"

/**
 * Battery Monitor Class
 * Monitors battery voltage and calculates percentage
 */
class BatteryMonitor {
private:
  uint8_t battery_percentage;
  unsigned long last_read_time;
  
  uint8_t calculatePercentage(int analog_value);
  
public:
  BatteryMonitor();
  bool initialize();
  
  // Battery operations
  uint8_t readBatteryPercentage();
  uint8_t getBatteryPercentage();
  
  // Task function (call in loop)
  void task();
};

#endif // BATTERY_MONITOR_H

