#include "eeprom_test.h"
#include <Arduino.h>
#include <EEPROM.h>


#define MAX_USER_TO_BE_STORED               5

#define EEPROM_STARTING_ADDRESS             0

#define IS_CONFIGURED_BYTE_ADDRESS_COUNT    1
#define MOBILE_NUMBER_LENGTH                10
#define PASSWORD_LEN_COUNT                  1
#define PASSWORD_STORE_COUNT                15


size_t is_pw_configured_address[MAX_USER_TO_BE_STORED]    = {};
size_t mobile_number_start_address[MAX_USER_TO_BE_STORED] = {};
size_t password_length_address[MAX_USER_TO_BE_STORED]     = {};
size_t password_start_address[MAX_USER_TO_BE_STORED]      = {};

bool is_password_configured[MAX_USER_TO_BE_STORED]        = {0,0,0,0,0};
char mobile_number[MAX_USER_TO_BE_STORED][MOBILE_NUMBER_LENGTH];
uint8_t  password_length[MAX_USER_TO_BE_STORED] = {0};
char password_value[MAX_USER_TO_BE_STORED][PASSWORD_STORE_COUNT];
/*** EEPROM function [START] **/

void set_eeprom_addresses(){
  for(uint8_t  i=0;i<MAX_USER_TO_BE_STORED;i++){
    if(i){
      is_pw_configured_address[i] = password_start_address[i-1]+PASSWORD_STORE_COUNT+1;
    }else{
      is_pw_configured_address[i] = EEPROM_STARTING_ADDRESS;      
    }
    mobile_number_start_address[i] = is_pw_configured_address[i] + 1;
    password_length_address[i] = mobile_number_start_address[i] + MOBILE_NUMBER_LENGTH + 1;
    password_start_address[i] = password_length_address[i] + 1;
  }
}

void update_data_from_eeprom(){
  for(uint8_t  i=0;i<MAX_USER_TO_BE_STORED;i++){
    update_password_from_eeprom(i);
  }
}

bool update_eeprom_data_at_index(uint8_t  index, char *mobile_number_to_add, char* password_to_add, uint8_t  len){
  if(len > 0){
    password_length[index] =  len;
    for(uint8_t  i=0;i<MOBILE_NUMBER_LENGTH;i++){
      mobile_number[index][i] = mobile_number_to_add[i];
    }
    for(uint8_t  j=0;j<len;j++){
      password_value[index][j] = password_to_add[j];
    }
    if(update_password_to_eeprom(index)){
      return 1;
    }
  }
  return 0;
}

bool check_if_password_is_configured(uint8_t  index){
  bool is_configured = check_is_configured_byte_address(is_pw_configured_address[index]);
  return is_configured;
}

uint8_t  update_length_of_password(uint8_t  index){
  password_length[index] = (uint8_t)EEPROM.read(password_length_address[index]);
  return password_length[index];
}

bool update_password_from_eeprom(uint8_t  index){
  is_password_configured[index] = check_is_configured_byte_address(is_pw_configured_address[index]);
  if(is_password_configured[index]){
    if(update_length_of_password(index)>0){
      LoadFromEeprom(mobile_number_start_address[index],mobile_number[index],MOBILE_NUMBER_LENGTH);
      LoadFromEeprom(password_start_address[index],password_value[index],password_length[index]);
      return 1;
    }
    return 0;
  }
  return 0;
}

bool update_password_to_eeprom(uint8_t  index){
  if(password_length[index]>0){
    EEPROM.write(password_length_address[index],password_length[index]);
    WriteEepromArray(mobile_number_start_address[index],mobile_number[index],MOBILE_NUMBER_LENGTH);
    WriteEepromArray(password_start_address[index],password_value[index],password_length[index]);
    is_password_configured[index] = 1;
    configure_byte_address(is_pw_configured_address[index]);
    return 1;
  }
  return 0;
}

void clear_password_in_eeprom(uint8_t  index){
  clear_byte_address(is_pw_configured_address[index]);
  clear_byte_address(password_length_address[index]);
  clearEepromArray(mobile_number_start_address[index],mobile_number_start_address[index]+MOBILE_NUMBER_LENGTH);
  clearEepromArray(password_start_address[index], password_start_address[index]+PASSWORD_LEN_COUNT);
}

int check_if_mobile_number_exists(char *arr){
  bool b_array_matched = 0;
  for(uint8_t  i=0;i<MAX_USER_TO_BE_STORED;i++){
    if(is_password_configured[i]){
      b_array_matched = 1;
      for(uint8_t  j=0;j<MOBILE_NUMBER_LENGTH;j++){
        if(arr[j] != mobile_number[i][j]){
          b_array_matched = 0;
          break;
        }
      }
      if(b_array_matched){
        Serial.print(i);
        Serial.println(" - Mobile Number exists!!");
        return i;
      }
    }
  }
  Serial.println("Mobile Number doesn't exists!!");
  return -1;
}

bool is_password_matched_with_any_user(char *arr,uint8_t len){
  for(uint8_t  i=0;i<MAX_USER_TO_BE_STORED;i++){
    if(is_password_matched(i,arr,len)){
      Serial.println("Password matched!!");
      return 1;
    }
  }
  Serial.println("PW not matched!!");
  return 0;
}

bool is_password_matched(uint8_t index, char *arr,uint8_t len){
  if(is_password_configured[index]){
    for(uint8_t  i=0;i< len /*password_length[index]*/;i++){
      if(arr[i] != password_value[index][i]){
        Serial.println("PW not matched!!");
        return 0;
      }
    }
    Serial.println("PW matched!!");
    return 1;
  }
  Serial.println("PW not matched!!");
  return 0;
}


void WriteEepromArray(size_t address,char *arr,size_t len){
  for (size_t x = 0; x < len; x++){
    EEPROM.write(address+x,arr[x]);
  }
}

void clearEepromArray(size_t start_address,size_t end_address){
    for (size_t x = 0; x < end_address; x++){
      EEPROM.write(start_address+x,0);
    }
}

void LoadFromEeprom(size_t address,char *arr,size_t len){  //Be aware that this function modifys the data pointed to by "arr"
  for (size_t x = 0; x < len; x++){
    arr[x] = EEPROM.read(address+x);
  }
}

bool check_is_configured_byte_address(size_t address){
    bool is_configured_byte_address_check = (bool)(EEPROM.read(address));
    return is_configured_byte_address_check;
}

bool configure_byte_address(size_t address){
  EEPROM.write(address, true);
  return true;
}

bool clear_byte_address(size_t address){
  EEPROM.write(address, false);
  return true;  
}
