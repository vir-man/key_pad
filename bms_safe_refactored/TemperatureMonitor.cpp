#include "TemperatureMonitor.h"

TemperatureMonitor::TemperatureMonitor() 
  : oneWire(nullptr), sensors(nullptr), last_read_time(0),
    temperature_value(0), prev_temperature_value(0), alarm_triggered(false) {
}

bool TemperatureMonitor::initialize() {
  oneWire = new OneWire(SystemConfig::ONE_WIRE_BUS);
  sensors = new DallasTemperature(oneWire);
  
  if (sensors == nullptr) {
    return false;
  }
  
  sensors->begin();
  last_read_time = millis();
  
  return true;
}

ErrorCode TemperatureMonitor::readTemperature() {
  if (sensors == nullptr) {
    return ErrorCode::SYSTEM_NOT_INITIALIZED;
  }
  
  sensors->requestTemperatures();
  temperature_value = (int)sensors->getTempCByIndex(0);
  
  // Check for alarm
  if (temperature_value > (int)SystemConfig::TEMPERATURE_THRESHOLD) {
    alarm_triggered = true;
  } else {
    alarm_triggered = false;
  }
  
  prev_temperature_value = temperature_value;
  
  return ErrorCode::SUCCESS;
}

int TemperatureMonitor::getTemperature() {
  return temperature_value;
}

bool TemperatureMonitor::isAlarmTriggered() {
  return alarm_triggered;
}

void TemperatureMonitor::resetAlarm() {
  alarm_triggered = false;
}

void TemperatureMonitor::task() {
  if (millis() - last_read_time > SystemConfig::TEMPERATURE_READ_INTERVAL) {
    readTemperature();
    last_read_time = millis();
  }
}

