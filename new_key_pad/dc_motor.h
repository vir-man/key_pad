
#include "Arduino.h"
#include "stdlib.h"
#include "stdio.h"

void init_dc_motor();
void dc_motor_task();

bool is_door_open();
bool is_door_close();

void open_door();
void close_door();