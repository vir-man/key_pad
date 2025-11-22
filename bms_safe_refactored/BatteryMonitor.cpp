#include "BatteryMonitor.h"

BatteryMonitor::BatteryMonitor() 
  : battery_percentage(100), last_read_time(0) {
}

bool BatteryMonitor::initialize() {
  pinMode(SystemConfig::BATTERY_ANALOG_PIN, INPUT);
  battery_percentage = readBatteryPercentage();
  return true;
}

uint8_t BatteryMonitor::calculatePercentage(int analog_value) {
  // Adjust these values based on your battery characteristics
  const int MIN_VOLTAGE = 0;    // Minimum ADC value (0V)
  const int MAX_VOLTAGE = 1023; // Maximum ADC value (5V)
  
  // Map analog value to percentage
  int percentage = map(analog_value, MIN_VOLTAGE, MAX_VOLTAGE, 0, 100);
  
  if (percentage < 0) percentage = 0;
  if (percentage > 100) percentage = 100;
  
  return (uint8_t)percentage;
}

uint8_t BatteryMonitor::readBatteryPercentage() {
  int analog_value = analogRead(SystemConfig::BATTERY_ANALOG_PIN);
  battery_percentage = calculatePercentage(analog_value);
  return battery_percentage;
}

uint8_t BatteryMonitor::getBatteryPercentage() {
  return battery_percentage;
}

void BatteryMonitor::task() {
  // Read battery every 10 seconds
  if (millis() - last_read_time > 10000) {
    readBatteryPercentage();
    last_read_time = millis();
  }
}

