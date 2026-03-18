#ifndef BUZZER_MODULE_H
#define BUZZER_MODULE_H

#include <Arduino.h>
#include <avr/pgmspace.h>
#include "config.h"
#include "pitches.h"

extern bool b_buzzer_on;
extern bool b_sub_buzzer_on;
extern unsigned long buzzer_timer;
extern const int buzzer_pin;

// Reference to melody array defined in main
extern const int melody[] PROGMEM;

void buzzer_task();

#endif // BUZZER_MODULE_H
