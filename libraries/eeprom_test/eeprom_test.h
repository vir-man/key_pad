#include <Arduino.h>
#include <EEPROM.h>


void set_eeprom_addresses();
void update_data_from_eeprom();
bool update_eeprom_data_at_index(uint8_t  index, char *mobile_number_to_add, char* password_to_add, uint8_t  len);
bool check_if_password_is_configured(uint8_t  index);
uint8_t  update_length_of_password(uint8_t  index);
bool update_password_from_eeprom(uint8_t  index);
bool update_password_to_eeprom(uint8_t  index);
void clear_password_in_eeprom(uint8_t  index);
int check_if_mobile_number_exists(char *arr);
bool is_password_matched_with_any_user(char *arr,uint8_t len);
bool is_password_matched(uint8_t index, char *arr,uint8_t len);
void WriteEepromArray(size_t address,char *arr,size_t len);
void clearEepromArray(size_t start_address,size_t end_address);
void LoadFromEeprom(size_t address,char *arr,size_t len);
bool check_is_configured_byte_address(size_t address);
bool configure_byte_address(size_t address);
bool clear_byte_address(size_t address);

void WriteEepromArray(size_t address,char *arr,size_t len);
void clearEepromArray(size_t start_address,size_t end_address);
void LoadFromEeprom(size_t address,char *arr,size_t len);
bool check_is_configured_byte_address(size_t address);
bool configure_byte_address(size_t address);
bool clear_byte_address(size_t address);