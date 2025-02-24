#include <LiquidCrystal.h>
#include "Arduino.h"
#include "stdlib.h"
#include "stdio.h"
#define MAX_NUM_OF_USERS 5 // Number including master user

void lcd_task();
void lcd_init();
bool is_password_valid(char user_id, char *password, uint8_t pass_len);
bool is_new_index();
char get_pressed_character();

#define CANCEL '@'
#define ENTER '#' // UNLOCK key
#define POWER '!'
#define LOCK '*'
#define MUTE '^'
#define ALPHA_NUM '&'

#define MAIN 0
#define MASTER_MAIN 1
#define MASTER_INPUT_STATE 11
#define LOCK_DOOR_STATE 12

#define INPUT_MOBILE_NUMBER 13
#define MASTER_ADD_USER_MOBILE_NUMBER 14

#define USER_INPUT_STATE        15
#define BUZZER_SCREEN           16
#define FINGERPRINT_SCREEN      17
#define ADD_FINGERPRINT_SCREEN  18

#define USER_LOCK_DOOR_STATE 30

#define MASTER_ADD_USER 2
#define MASTER_REMOVE_USER 3
#define MASTER_PASSWORD 4
#define MASTER_DAT_TIM 5
#define MASTER_BACKUP 6
#define USER 7
#define USER_PASSWORD 8

#define ALPHA_SCREEN 9
#define BACKUP_SCREEN 10

// extern bool is_password_matched(uint8_t index, char *arr, uint8_t len);
// extern void open_door();
// extern void close_door();
