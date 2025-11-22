#ifndef BUZZER_CONTROLLER_H
#define BUZZER_CONTROLLER_H

#include <avr/pgmspace.h>
#include "SystemConfig.h"
#include "pitches.h"

/**
 * Buzzer Controller Class
 * Handles buzzer control and melodies
 */
class BuzzerController {
private:
  unsigned long buzzer_timer;
  bool buzzer_on;
  bool sub_buzzer_on;
  uint16_t buzzer_timeout;
  
  // Melody stored in PROGMEM
  static const uint16_t melody[];
  static const int noteDurations[];
  
public:
  BuzzerController();
  bool initialize(uint16_t timeout);
  
  // Buzzer operations
  void turnOn();
  void turnOff();
  void playMelody();
  
  // Task function (call in loop)
  void task();
  
  // Configuration
  void setTimeout(uint16_t timeout);
  uint16_t getTimeout();
};

#endif // BUZZER_CONTROLLER_H

