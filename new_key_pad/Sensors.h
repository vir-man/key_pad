#ifndef SENSORS_H
#define SENSORS_H

#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "uRTCLib.h" // Keeping original RTC library
#include "Config.h"
#include "Types.h"
#include "Logger.h"

class Sensors {
public:
    static Sensors& getInstance();
    
    void begin();
    void update(); // Main task
    
    // RTC Interface
    DateTime getDateTime();
    void setDateTime(const DateTime& dt);
    
    // Status
    int getTemperature() const { return _currentTemp; }
    bool isVibrationAlarm() const { return _vibrationAlarm; }
    bool isTempAlarm() const { return _tempAlarm; }
    
    void resetAlarms();

private:
    Sensors();
    
    // RTC Objects
    uRTCLib _rtc;
    unsigned long _lastRtcUpdate;
    DateTime _currentDateTime;
    
    // Temp Objects
    OneWire _oneWire;
    DallasTemperature _sensors;
    int _currentTemp;
    uint8_t _tempHighCounter;
    bool _tempAlarm;
    unsigned long _lastTempRead;
    
    // Vibration Objects
    bool _vibrationAlarm;
    bool _vibrationActive;
    uint8_t _vibrationCount;
    unsigned long _vibrationTimer;
    bool _prevVibState;
    
    void updateRTC();
    void updateTemp();
    void updateVibration();
};

#endif // SENSORS_H
