#include "Sensors.h"

Sensors& Sensors::getInstance() {
    static Sensors instance;
    return instance;
}

Sensors::Sensors()
    : _rtc(0x68), _oneWire(PIN_ONE_WIRE_BUS), _sensors(&_oneWire),
      _currentTemp(0), _tempHighCounter(0), _tempAlarm(false), _lastTempRead(0),
      _vibrationAlarm(false), _vibrationActive(false), _vibrationCount(0), _vibrationTimer(0), _prevVibState(0)
{
}

void Sensors::begin() {
    // Temp Init
    _sensors.begin();
    
    // RTC Init - assuming Wire.begin() handled elsewhere or by lib
    // uRTCLib might need Wire.begin()
    
    // Vibration Pin
    pinMode(PIN_VIBRATION_SENSOR, INPUT); 
    // Need to verify pin mode from original code (INPUT/INPUT_PULLUP?)
    // Analysis says: pinMode(vibration_sensor_pin, INPUT) likely, needs check if pullup used
}

void Sensors::update() {
    updateRTC();
    updateTemp();
    updateVibration();
}

void Sensors::updateRTC() {
    if (millis() - _lastRtcUpdate > 1000) { // Update internal cache every second for display
        _rtc.refresh();
        _currentDateTime.second = _rtc.second();
        _currentDateTime.minute = _rtc.minute();
        _currentDateTime.hour = _rtc.hour();
        _currentDateTime.day = _rtc.day();
        _currentDateTime.month = _rtc.month();
        _currentDateTime.year = _rtc.year();
        _currentDateTime.dayOfWeek = _rtc.dayOfWeek();
        _lastRtcUpdate = millis();
    }
}

void Sensors::updateTemp() {
    // Check every 10 seconds
    if (millis() - _lastTempRead > 10000) {
        _sensors.requestTemperatures();
        int newTemp = _sensors.getTempCByIndex(0);
        
        if (newTemp != _currentTemp) {
            _currentTemp = newTemp;
            
            // Alarm Logic
            if (_currentTemp > 60) { // Threshold from original
                _tempHighCounter++;
                if (_tempHighCounter > 3) {
                    _tempAlarm = true;
                    Logger::getInstance().logSystem("ALARM: High Temperature!", LogLevel::CRITICAL);
                }
            } else {
                if (!_tempAlarm) _tempHighCounter = 0;
            }
        }
        _lastTempRead = millis();
    }
}

void Sensors::updateVibration() {
    bool currentState = digitalRead(PIN_VIBRATION_SENSOR);
    
    if (currentState != _prevVibState) {
        _prevVibState = currentState;
        
        if (currentState) { // Rising edge (or active state)
             if (!_vibrationActive) {
                 _vibrationActive = true;
                 _vibrationCount = 0;
             }
             _vibrationTimer = millis();
             _vibrationCount++;
             
             if (_vibrationCount > 15) {
                 _vibrationAlarm = true;
                 _vibrationActive = false; // Reset to avoid continuous re-trigger immediately?
                 Logger::getInstance().logSystem("ALARM: Vibration Detected!", LogLevel::CRITICAL);
             }
        }
    }
    
    // Timeout reset
    if (_vibrationActive && (millis() - _vibrationTimer > 10000)) {
        _vibrationActive = false;
        _vibrationCount = 0;
    }
}

DateTime Sensors::getDateTime() {
    // Return cached value
    return _currentDateTime;
}

void Sensors::setDateTime(const DateTime& dt) {
    _rtc.set(dt.second, dt.minute, dt.hour, dt.dayOfWeek, dt.day, dt.month, dt.year);
    // Force refresh
    _lastRtcUpdate = 0; 
}

void Sensors::resetAlarms() {
    _tempAlarm = false;
    _vibrationAlarm = false;
    _tempHighCounter = 0;
    _vibrationCount = 0;
}
