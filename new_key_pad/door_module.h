#ifndef DOOR_MODULE_H
#define DOOR_MODULE_H

#include <Arduino.h>
#include "config.h"

// Door states and flags
extern bool is_door_closing;
extern bool is_door_opening;

extern bool b_command_open_door;
extern bool b_command_close_door;

extern bool b_error_in_door_open;
extern bool b_error_in_door_close;

// Functions
void init_dc_motor();
void dc_motor_task();
void dc_motor_stop();
void dc_motor_on(int direction);

bool is_door_open();
bool is_door_close();
bool is_door_aligned_by_ir();

void open_door();
void close_door();

#endif // DOOR_MODULE_H
