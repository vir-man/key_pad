#ifndef SENSOR_MODULE_H
#define SENSOR_MODULE_H

#include <Arduino.h>
#include <DallasTemperature.h>
#include <OneWire.h>
#include "config.h"

// External references for Status Flags
extern bool b_temperature_alarm_triggerd;
extern bool b_vibration_alarm_triggered;
extern bool b_gun_point_activation_triggerd;

#define GPA_DO_NOTHING 0
#define GPA_SEND_MESSAGE 1
#define GPA_CALL 2

extern uint8_t gpa_state;
extern uint8_t current_gpa_user_id;

// For OTP generation and validation
extern uint8_t generated_otp[6];
extern char char_generated_otp[6];
extern uint8_t master_otp[6];

extern const int siren_pin[2];

// Core sensor objects
extern OneWire oneWire;
extern DallasTemperature sensors;

// Sensor setup & tasks
void temp_sen_init();
void read_temperature();
void temp_task();

// Gun point FSM
void gun_point_activation_fsm();

// Siren functions
void siren_on(uint8_t i);
void siren_off(uint8_t i);

// OTP
void generate_random_otp();

#endif // SENSOR_MODULE_H
