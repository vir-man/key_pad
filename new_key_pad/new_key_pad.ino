#include "Adafruit_Keypad.h"
#include "lcd.h"
#include "dc_motor.h"
#include <EEPROM.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <avr/wdt.h>
#include <Adafruit_Fingerprint.h>
#include <SD.h>
#include "pitches.h"
#include "uRTCLib.h"
#include <Ch376msc.h>
#include "Adafruit_Keypad.h"

#define mySerial Serial3
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

#define ONE_WIRE_BUS 42

// #define DEBUG

#ifdef DEBUG
#define DEBUG_PRINT(x) Serial.print(x)
#define DEBUG_PRINTLN(x) Serial.println(x)
#else
#define DEBUG_PRINT(x)
#define DEBUG_PRINTLN(x)
#endif
#define SIM7600 sim7600Serial

#define LCD_PRINT(s) lcd.print(F(s))
#define SERIAL_PRINT(s) Serial.print(F(s))
#define SERIAL_PRINTLN(s) Serial.println(F(s))

/********************************************************************/
// Setup a oneWire instance to communicate with any OneWire devices
// (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);
/********************************************************************/
// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);
SoftwareSerial sim7600Serial(19, 18); // RX, TX pins for SIM7600
/********************************************************************/
unsigned long temperature_read_timeout;
unsigned int temperature_read_time_interval = 10000;

int temperature_value = 0;
int prev_temperature_value = 0;

bool b_temperature_alarm_triggerd = 0;
#define TEMPERATURE_THRESHOLD 60

uint16_t input_alpha_speed = 0;
bool b_alpha_speed_updated = 0;
uint16_t alpha_counter = 0;
/***** Buzzer Vars [START] *****/

// notes in the melody (store in flash to save SRAM)
const uint16_t melody[] PROGMEM = {
    NOTE_C4, NOTE_G3, NOTE_G3, NOTE_A3, NOTE_G3, 0, NOTE_B3, NOTE_C4};

// note durations: 4 = quarter note, 8 = eighth note, etc.:
int noteDurations[] = {
    4, 8, 8, 4, 4, 4, 4, 4};
const int buzzer_pin = 45;
bool b_buzzer_on = 0;
unsigned long buzzer_timer = millis();
bool b_sub_buzzer_on = 0;

unsigned long door_open_time;

/***** Buzzer Vars [END] *****/
/***** RTC [START] *****/
#define RTC_UPDATE_TIME_INTERVAL_IN_MINUTE 10
uint32_t rtc_update_time_interval_in_minute = 60000;
unsigned long rtc_timer = 0;
// RTC vars
uint8_t second, minute, hour, day_of_week, date, month, year;
uRTCLib rtc(0x68); // Create objects and assign module I2c Addresses
void update_rtc_timer()
{
  rtc_timer = millis();
}
void rtc_begin()
{
  Wire.begin();
  /*
   * Set Date and Time if you want to set it for the first time.
   *
   */
  // Use following command once to set current day/time, then disable by commenting it out.
  // rtc.set(0, 39, 0, 2, 27, 12, 22);
  // Format:  Seconds(0-59), Minute(0-59), Hour(0-23), Day of Week - Sun thru Sat (1-7),
  // Day of Month(1-31), Month(1-12), Year(00-99)
  update_date_time_from_rtc();
  update_rtc_timer();
}
void set_rtc()
{
  rtc.set(second, minute, hour, 1, date, month, year);
}
bool validate_date_and_time(uint8_t _second, uint8_t _minute, uint8_t _hour,
                            uint8_t _date, uint8_t _month, uint8_t _year)
{
  if (_second < 60 && _minute < 60 && _hour < 24 && (_date > 0 && _date < 32) && (_month > 0 && _month < 13) && _year < 100)
  {
    second = _second;
    minute = _minute;
    hour = _hour;
    date = _date;
    month = _month;
    year = _year;
    set_rtc();
    update_date_time_from_rtc();
    return 1;
  }
  return 0;
}

void update_date_time_from_rtc()
{
  rtc.refresh();
  month = rtc.month();
  date = rtc.day();
  year = rtc.year();
  hour = rtc.hour();
  minute = rtc.minute();
  second = rtc.second();
  day_of_week = rtc.dayOfWeek();

  Serial.print(month);
  Serial.print('/');
  Serial.print(date);
  Serial.print('/');
  Serial.print(year);

  SERIAL_PRINT("  Time: ");
  Serial.print(hour);
  Serial.print(':');
  Serial.print(minute);
  Serial.print(':');
  Serial.print(second);
  SERIAL_PRINT(" ");

  Serial.print(DayAsString(day_of_week));
  Serial.println();
}
String DayAsString(int day)
{
  switch (day)
  {
  case 1:
    return "Sunday";
  case 2:
    return "Monday";
  case 3:
    return "Tuesday";
  case 4:
    return "Wednesday";
  case 5:
    return "Thursday";
  case 6:
    return "Friday";
  case 7:
    return "Saturday";
  }
  return "(Incorrect Day)";
}
void rtc_task()
{
  // Check time every 30 minute
  if (millis() - rtc_timer > (rtc_update_time_interval_in_minute))
  {
    update_date_time_from_rtc();
    update_rtc_timer();
  }
}
/***** RTC [END] *****/
/***** SD CARD [START] *****/

// include the SD library:
// set up variables using the SD utility library functions:
Sd2Card card;
SdVolume volume;
SdFile root;

const int chipSelect = 53;
bool b_sd_card_not_initiated = 0;
File myFile;
void sd_init()
{
  SERIAL_PRINT("Initializing SD card...");
  // see if the card is present and can be initialized:
  if (!SD.begin(chipSelect))
  {
    SERIAL_PRINTLN("Card failed, or not present");
    // don't do anything more:
    b_sd_card_not_initiated = 1;
    // while (1)
    //   ;
  }
  else
  {
    SERIAL_PRINTLN("card initialized.");
  }

  // we'll use the initialization code from the utility libraries
  // since we're just testing if the card is working!
  // if (!card.init(SPI_HALF_SPEED, chipSelect))
  // {
  //   Serial.println("initialization failed. Things to check:");
  //   Serial.println("* is a card inserted?");
  //   Serial.println("* is your wiring correct?");
  //   Serial.println("* did you change the chipSelect pin to match your shield or module?");
  //   b_sd_card_not_initiated = 1;
  // }
  // else
  // {
  //   Serial.println("Wiring is correct and a card is present.");
  // } // Now we will try to open the 'volume'/'partition' - it should be FAT16 or FAT32
  // if (!volume.init(card))
  // {
  //   Serial.println("Could not find FAT16/FAT32 partition.\nMake sure you've formatted the card");
  //   b_sd_card_not_initiated = 1;
  // }

  // Serial.print("Clusters:          ");
  // Serial.println(volume.clusterCount());
  // Serial.print("Blocks x Cluster:  ");
  // Serial.println(volume.blocksPerCluster());

  // Serial.print("Total Blocks:      ");
  // Serial.println(volume.blocksPerCluster() * volume.clusterCount());
  // Serial.println();

  // // print the type and size of the first FAT-type volume
  // uint32_t volumesize;
  // Serial.print("Volume type is:    FAT");
  // Serial.println(volume.fatType(), DEC);

  // volumesize = volume.blocksPerCluster(); // clusters are collections of blocks
  // volumesize *= volume.clusterCount();    // we'll have a lot of clusters
  // volumesize /= 2;                        // SD card blocks are always 512 bytes (2 blocks are 1KB)
  // Serial.print("Volume size (Kb):  ");
  // Serial.println(volumesize);
  // Serial.print("Volume size (Mb):  ");
  // volumesize /= 1024;
  // Serial.println(volumesize);
  // Serial.print("Volume size (Gb):  ");
  // Serial.println((float)volumesize / 1024.0);

  // Serial.println("\nFiles found on the card (name, date and size in bytes): ");
  // root.openRoot(volume);

  // // list all files in the card with date and size
  // root.ls(LS_R | LS_DATE | LS_SIZE);

  // if (!SD.begin(chipSelect))
  // {
  //   Serial.println("Card failed, or not present");
  //   // don't do anything more:
  //   while (1)
  //     ;
  // }
  // Serial.println("card initialized.");
  // myFile = SD.open("BMS-LOG1.TXT");
  // if (myFile)
  // {
  //   // myFile.print("LOGGING DATE " + date + "/" + month + "/" + year + "\r\r" + hour + ":" + minute + ":" + second + "\n");
  //   // myFile.println("****************************************");
  //   // myFile.print("SR.\rUSER\rDATE\r\rTIME\r\rREMARKS\n");
  //   Serial.println("BMS-LOG1.TXT Opened!");
  // }
  // else
  // {
  //   // if the file didn't open, print an error:
  //   Serial.println("error opening BMS-LOG1.TXT");
  // }
}
void sd_card_task()
{
}
#define TAB_STRING "     "
#define CLOSE 0
#define OPEN 1
void update_log_entry(uint16_t sr_no, uint8_t _user_id, bool dir)
{
  String dataString = "";
  // int date, month, year, hour, minute, second;
  if (sr_no < 10)
  {
    dataString = dataString + "00" + sr_no;
  }
  else if (sr_no < 100)
  {
    dataString = dataString + "0" + sr_no;
  }
  else
  {
    dataString = dataString + sr_no;
  }
  dataString = dataString + TAB_STRING + "0" + _user_id;
  dataString = dataString + TAB_STRING;
  if (date < 10)
    dataString = dataString + "0";
  dataString = dataString + date + "/";

  if (month < 10)
    dataString = dataString + "0";
  dataString = dataString + month + "/" + year;

  dataString = dataString + TAB_STRING;
  if (hour < 10)
    dataString = dataString + "0";
  dataString = dataString + hour + ":";

  if (minute < 10)
    dataString = dataString + "0";
  dataString = dataString + minute + ":";

  if (second < 10)
    dataString = dataString + "0";
  dataString = dataString + second;

  if (dir == CLOSE)
  {
    dataString = dataString + TAB_STRING + "C";
  }
  else if (dir == OPEN)
  {
    dataString = dataString + TAB_STRING + "O";
  }

  // Serial.println(dataString);
  // return;
  // myFile.println(dataString);
  // myFile.print("LOGGING DATE " + date + "/" + month + "/" + year + "\r\r" + hour + ":" + minute + ":" + second + "\n");
  // myFile.println("****************************************");
  // myFile.print("SR.\rUSER\rDATE\r\rTIME\r\rREMARKS\n");

  // open the file. note that only one file can be open at a time,
  // so you have to close this one before opening another.
  File dataFile = SD.open("BMS-LOG1.TXT", FILE_WRITE);

  // if the file is available, write to it:
  if (dataFile)
  {
    dataFile.println(dataString);
    dataFile.close();
    // print to the serial port too:
    Serial.println(dataString);
    Serial.println("BMS-LOG1.TXT Updated!");
  }
  // if the file isn't open, pop up an error:
  else
  {
    Serial.println("error opening BMS-LOG1.TXT");
  }
}
/***** SD CARD [END] *****/
/*************** USB HARDWARE SERIAL CODE [START] ****************/
bool b_backup_in_progress = 0;
bool b_backup_complete = 0;

bool b_flash_drive_attached = 0;

// File header string stored in PROGMEM to save RAM
const char file_start_str[] PROGMEM = "SR.    USER      DATE         TIME      REMARKS\n---------------------------------------------\n";
//..............................................................................................................................
// Leave the default jumper settings for the baud rate (9600) on the CH376, the library will set it up the chosen speed(HW serial only)
Ch376msc flashDrive(Serial2, 115200); // Ch376 object with hardware Serial1 on arduino mega baudrate: 9600, 19200, 57600, 115200

// static char helpString[] = {"h:Print this help\n\n1:Create\n2:Append\n3:Read\n4:Read date/time\n"
//                             "5:Modify date/time\n6:Delete\n7:List dir\n8:Print free space"
//                             "\n9:Open/Create folder(s)/subfolder(s)"};

void init_flash_drive()
{
  flashDrive.init();
  // printInfo(helpString);
}
void flash_drive_task()
{
  if (flashDrive.checkIntMessage())
  {
    if (flashDrive.getDeviceStatus())
    {
      Serial.println(F("Flash drive attached!"));
      b_flash_drive_attached = 1;
    }
    else
    {
      Serial.println(F("Flash drive detached!"));
      b_flash_drive_attached = 0;
    }
  }
  if (b_backup_in_progress)
  {
    copy_data_from_sd_card_to_usb_flash_drive();
  }
}
void copy_data_from_sd_card_to_usb_flash_drive()
{
  //  static File dataFile;// = SD.open("BMS-LOG1.TXT", FILE_WRITE);
  bool b_flash_drive_file_available = 0;
  String input_string_from_sd_card;
  char input_string_char_array[80];  // Reduced from 100 to save 20 bytes RAM
  // Serial.println("Coming 1");

  SERIAL_PRINT("File opened!");
  File dataFile = SD.open("BMS-LOG1.TXT");
  //  flashDrive.init();
  flashDrive.setFileName("BMS-LOG1.TXT");
  flashDrive.openFile();
  char pgm_buffer[80];
  strcpy_P(pgm_buffer, file_start_str);
  flashDrive.writeFile(pgm_buffer, strlen(pgm_buffer));
  if (dataFile)
  {
    uint8_t cursor_index = 0;
    while (dataFile.available())
    {
      //      Serial.println("Coming 4");
      String input_string_from_sd_card = dataFile.readStringUntil('\n');
      Serial.println(input_string_from_sd_card);
      memset(input_string_char_array, '\0', sizeof(input_string_char_array));
      input_string_from_sd_card.toCharArray(input_string_char_array, input_string_from_sd_card.length());
      input_string_char_array[strlen(input_string_char_array)] = '\n';
      //      Serial.println(input_string_char_array);
      //      Serial.println(input_string_from_sd_card.length()+1);
      //      Serial.println(strlen(input_string_char_array));
      flashDrive.writeFile(input_string_char_array, strlen(input_string_char_array));
      memset(input_string_char_array, '\0', sizeof(input_string_char_array));
    }
    flashDrive.closeFile();
    dataFile.close();
    SERIAL_PRINTLN("Reading recently written file");
    flashDrive.setFileName("BMS-LOG1.TXT"); // set the file name
    flashDrive.openFile();                  // open the file
    bool readMore = true;
    char char_array[43];
    // read data from flash drive until we reach EOF
    while (readMore)
    { // our temporary buffer where we read data from flash drive and the size of that buffer
      readMore = flashDrive.readFile(char_array, sizeof(char_array));
      Serial.print(char_array); // print the contents of the temporary buffer
    }
    flashDrive.closeFile(); // at the end, close the file
    printInfo("Done!");
    b_backup_complete = 1;
    //      b_backup_in_progress = 0;
  }
  else
  {
    b_backup_in_progress = 0;
    SERIAL_PRINTLN("error opening BMS-LOG1.TXT");
  }
}
// Print information
void printInfo(const char info[])
{

  int infoLength = strlen(info);
  if (infoLength > 40)
  {
    infoLength = 40;
  }
  Serial.print(F("\n\n"));
  for (int a = 0; a < infoLength; a++)
  {
    Serial.print('*');
  }
  Serial.println();
  Serial.println(info);
  for (int a = 0; a < infoLength; a++)
  {
    Serial.print('*');
  }
  Serial.print(F("\n\n"));
}
/*************** USB HARDWARE SERIAL CODE [END] ****************/
/***** EEPROM SECTION [START] **/

#define MAX_USER_TO_BE_STORED 28

#define EEPROM_STARTING_ADDRESS 0

#define IS_CONFIGURED_BYTE_ADDRESS_COUNT 1
#define MOBILE_NUMBER_LENGTH 10
#define PASSWORD_LEN_COUNT 1
#define PASSWORD_STORE_COUNT 15

#define IN_OUT_TIME_LEN_COUNT 2

#define ALPHA_SPEED_LEN_COUNT 4
#define BUZZER_TIMEOUT_LEN_COUNT 4
#define DOOR_OPEN_COUND_LEN_COUNT 4

// Compute EEPROM addresses on-the-fly to save RAM
#define USER_BLOCK_SIZE (1 /*is_pw*/ + MOBILE_NUMBER_LENGTH + 1 /*pw len*/ + 1 /*pw start*/ + PASSWORD_STORE_COUNT + 1 /*is_in_out*/ + IN_OUT_TIME_LEN_COUNT /*in*/ + IN_OUT_TIME_LEN_COUNT /*out*/ + 1 /*padding*/)

static inline uint16_t eeprom_addr_user_base(uint8_t index) { return (uint16_t)(EEPROM_STARTING_ADDRESS + (uint16_t)index * (uint16_t)USER_BLOCK_SIZE); }
static inline uint16_t eeprom_addr_is_pw(uint8_t index) { return eeprom_addr_user_base(index) + 0; }
static inline uint16_t eeprom_addr_mobile(uint8_t index) { return eeprom_addr_is_pw(index) + 1; }
static inline uint16_t eeprom_addr_pw_len(uint8_t index) { return eeprom_addr_mobile(index) + MOBILE_NUMBER_LENGTH + 1; }
static inline uint16_t eeprom_addr_pw(uint8_t index) { return eeprom_addr_pw_len(index) + 1; }
static inline uint16_t eeprom_addr_is_inout(uint8_t index) { return eeprom_addr_pw(index) + PASSWORD_STORE_COUNT + 1; }
static inline uint16_t eeprom_addr_in_time(uint8_t index) { return eeprom_addr_is_inout(index) + 1; }
static inline uint16_t eeprom_addr_out_time(uint8_t index) { return eeprom_addr_in_time(index) + IN_OUT_TIME_LEN_COUNT; }
static inline uint16_t eeprom_addr_after_users(void) { return eeprom_addr_out_time(MAX_USER_TO_BE_STORED - 1) + IN_OUT_TIME_LEN_COUNT + 1; }

size_t alpha_speed_start_address;
size_t door_open_count_start_address;
size_t buzzer_timeout_start_address;

bool is_password_configured[MAX_USER_TO_BE_STORED] = {0};
char mobile_number[MAX_USER_TO_BE_STORED][MOBILE_NUMBER_LENGTH];
uint8_t password_length[MAX_USER_TO_BE_STORED] = {0};
char password_value[MAX_USER_TO_BE_STORED][PASSWORD_STORE_COUNT];

bool is_in_out_time_configured[MAX_USER_TO_BE_STORED] = {0};
uint8_t in_time_hour[MAX_USER_TO_BE_STORED] = {0};
uint8_t in_time_minute[MAX_USER_TO_BE_STORED] = {0};
uint8_t out_time_hour[MAX_USER_TO_BE_STORED] = {0};
uint8_t out_time_minute[MAX_USER_TO_BE_STORED] = {0};

uint16_t alpha_speed;
uint16_t buzzer_timeout;
uint16_t door_open_count;

char _mobile_number[10] = {'0', '0', '0', '0', '0', '0', '0', '0', '0', '0'};
char _password[15] = {/*'A', 'B',*/ '1', '2', '3', '4', '5', '6', '7', '8', '9', '3', '1', '2', 'Z', 'A', 'B'};
char _password1[15];
HardwareSerial *port;

// String str_mobile_number[5];
// void convert_mobile_numbers_to_string()
// {
//   char _1mobile_number[10];
//   for (uint8_t i = 0; i < MAX_NUM_OF_USERS; i++)
//   {
//     Serial.println(i);
//     if (is_password_configured[i])
//     {
//       memset(_1mobile_number, '\0', sizeof(_1mobile_number));
//       for (uint8_t j = 0; j < 10; j++)
//       {
//         _1mobile_number[j] = mobile_number[i][j];
//         // printf("%c", _1mobile_number[j]);
//       }
//       str_mobile_number[i] = String(_1mobile_number);
//       Serial.println(str_mobile_number[i]);
//     }
//   }
// }

void init_eeprom()
{
  set_eeprom_addresses();
  SERIAL_PRINTLN("Address assigned");
  update_data_from_eeprom();
  // convert_mobile_numbers_to_string();
}
void clear_eeprom()
{
  for (int i = 0; i < EEPROM.length(); i++)
  {
    SERIAL_PRINT("clearing eeprom at address");
    Serial.println(i);
    EEPROM.write(i, 0);
  }
  SERIAL_PRINT("EEPROM Cleared!");
}
void clear_eeprom_data()
{
  // Use computed addresses instead of large address arrays
  clearEepromArray(EEPROM_STARTING_ADDRESS, eeprom_addr_after_users());
}
void print_eeprom_data(HardwareSerial *serial1)
{
  // update_data_from_eeprom();
  serial1->print("\n|CONFIGURED?|MOBILE NUMBER|PW LENGTH|PASSWORD|IS_INOUT_CONFIG|IN_TIME|OUT_TIME\n");
  for (uint8_t j = 0; j < MAX_USER_TO_BE_STORED; j++)
  {
    if (!is_password_configured[j])
    {
      serial1->print("|NO|NA|NA|NA|NA|NA|NA\n");
    }
    else
    {
      serial1->print("|YES|");
      for (uint8_t i = 0; i < 10; i++)
      {
        serial1->print(mobile_number[j][i]);
      }
      serial1->print("|");
      serial1->print(password_length[j]);
      serial1->print("|");
      for (uint8_t i = 0; i < password_length[j]; i++)
      {
        serial1->print(password_value[j][i]);
      }
      if (!is_in_out_time_configured[j])
      {
        serial1->print("|NO|NA|NA|\n");
      }
      else
      {
        serial1->print("|YES|");
        serial1->print(in_time_hour[j]);
        serial1->print(":");
        serial1->print(in_time_minute[j]);
        serial1->print("|");
        serial1->print(out_time_hour[j]);
        serial1->print(":");
        serial1->print(out_time_minute[j]);
        serial1->print("|\n");
      }
      // serial1->print("|\n");
    }
  }
  serial1->print("Alpha Speed :");
  serial1->print(alpha_speed);
  serial1->print(" | Door Open Count :");
  serial1->print(door_open_count);
  serial1->print(" | Buzzer Timeout :");
  serial1->println(buzzer_timeout);
}

void set_eeprom_addresses()
{
  // Derive global parameter addresses once; user addresses are computed on demand
  alpha_speed_start_address = eeprom_addr_after_users();
  door_open_count_start_address = alpha_speed_start_address + ALPHA_SPEED_LEN_COUNT;
  buzzer_timeout_start_address = door_open_count_start_address + BUZZER_TIMEOUT_LEN_COUNT;
  /*
  for (uint8_t i = 0; i < MAX_USER_TO_BE_STORED; i++)
  {
    Serial.print("Index ------------------------------------->");
    Serial.println(i);
    Serial.print("is_pw_configured_address ");
    Serial.println(is_pw_configured_address[i]);
    Serial.print("mobile_number_start_address ");
    Serial.println(mobile_number_start_address[i]);
    Serial.print("password_length_address ");
    Serial.println(password_length_address[i]);
    // Serial.print("password_length_address ");
    // Serial.println(password_length_address[i]);
    Serial.print("password_start_address ");
    Serial.println(password_start_address[i]);
    Serial.print("in_out_bit_configuratino_address");
    Serial.println(is_in_out_time_configured_address[i]);
    Serial.print("in_time_start_address");
    Serial.println(in_time_start_address[i]);
    Serial.print("out_time_start_address");
    Serial.println(out_time_start_address[i]);
  }
    Serial.print("alpha_speed_start_address ");
    Serial.println(alpha_speed_start_address);
    Serial.print("door_open_count_start_address ");
    Serial.println(door_open_count_start_address);
    Serial.print("buzzer_timeout_start_address ");
    Serial.println(buzzer_timeout_start_address);
*/
}
#define DEFAULT_ALPHA_SPEED 200
#define DEFAULT_BUZZER_TIMEOUT 10
void write_alpha_speed_to_eeprom(uint16_t _alpha_speed)
{
  alpha_speed = _alpha_speed;
  // Serial.println("Writing Alpha Count");
  // Serial.println(alpha_speed);
  byte b1 = alpha_speed >> 8;
  byte b2 = alpha_speed & 0xFF;
  EEPROM.write(alpha_speed_start_address, b1);
  EEPROM.write(alpha_speed_start_address + 1, b2);
  read_alpha_speed_to_eeprom();
}

void write_buzzer_timeout_to_eeprom(uint16_t _buzzer_timeout)
{
  buzzer_timeout = _buzzer_timeout;
  // Serial.println("Writing Buzzer Timeout");
  // Serial.println(buzzer_timeout);
  byte b1 = buzzer_timeout >> 8;
  byte b2 = buzzer_timeout & 0xFF;
  EEPROM.write(buzzer_timeout_start_address, b1);
  EEPROM.write(buzzer_timeout_start_address + 1, b2);
  read_buzzer_timeout_to_eeprom();
}

uint16_t read_buzzer_timeout_to_eeprom()
{
  byte b1 = EEPROM.read(buzzer_timeout_start_address);
  byte b2 = EEPROM.read(buzzer_timeout_start_address + 1);
  buzzer_timeout = (b1 << 8) + b2;
  // Serial.println("Reading Buzzer Timeout Count");
  // Serial.println(buzzer_timeout);
  if (buzzer_timeout == 0)
  {
    write_buzzer_timeout_to_eeprom(DEFAULT_BUZZER_TIMEOUT);
  }
  return buzzer_timeout;
}

uint16_t read_alpha_speed_to_eeprom()
{
  byte b1 = EEPROM.read(alpha_speed_start_address);
  byte b2 = EEPROM.read(alpha_speed_start_address + 1);
  alpha_speed = (b1 << 8) + b2;
  // Serial.println("Reading Alpha Count");
  // Serial.println(alpha_speed);
  if (alpha_speed == 0)
  {
    write_alpha_speed_to_eeprom(DEFAULT_ALPHA_SPEED);
  }
  return alpha_speed;
}
void write_door_open_count_to_eeprom(uint16_t _door_open_count)
{
  door_open_count = _door_open_count;
  // Serial.println("Writing Door Count");
  // Serial.println(door_open_count);

  byte b1 = door_open_count >> 8;
  byte b2 = door_open_count & 0xFF;
  EEPROM.write(door_open_count_start_address, b1);
  EEPROM.write(door_open_count_start_address + 1, b2);
  read_door_open_count_to_eeprom();
}

uint16_t read_door_open_count_to_eeprom()
{
  byte b1 = EEPROM.read(door_open_count_start_address);
  byte b2 = EEPROM.read(door_open_count_start_address + 1);
  door_open_count = (b1 << 8) + b2;
  // Serial.println("Reading Door Count");
  // Serial.println(door_open_count);
  return door_open_count;
}
void update_data_from_eeprom()
{
  for (uint8_t i = 0; i < MAX_USER_TO_BE_STORED; i++)
  {
    update_password_from_eeprom(i);
  }
  read_alpha_speed_to_eeprom();
  read_buzzer_timeout_to_eeprom();
  read_door_open_count_to_eeprom();
}

bool update_eeprom_data_at_index(uint8_t index, char *mobile_number_to_add, char *password_to_add, uint8_t len)
{
  if (len > 0)
  {
    password_length[index] = len;
    for (uint8_t i = 0; i < MOBILE_NUMBER_LENGTH; i++)
    {
      mobile_number[index][i] = mobile_number_to_add[i];
    }
    for (uint8_t j = 0; j < len; j++)
    {
      password_value[index][j] = password_to_add[j];
    }
    if (update_password_to_eeprom(index))
    {
      // convert_mobile_numbers_to_string();
      return 1;
    }
  }
  return 0;
}

bool check_if_password_is_configured(uint8_t index)
{
  bool is_configured = check_is_configured_byte_address(eeprom_addr_is_pw(index));
  return is_configured;
}

uint8_t update_length_of_password(uint8_t index)
{
  password_length[index] = (uint8_t)EEPROM.read(eeprom_addr_pw_len(index));
  return password_length[index];
}
bool update_in_out_time_to_eeprom(uint8_t index, uint8_t _in_time_hour, uint8_t _in_time_minute,
                                  uint8_t _out_time_hour, uint8_t _out_time_minute)
{
  in_time_hour[index] = _in_time_hour;
  in_time_minute[index] = _in_time_minute;
  out_time_hour[index] = _out_time_hour;
  out_time_minute[index] = _out_time_minute;
  is_in_out_time_configured[index] = true;
  EEPROM.write(eeprom_addr_in_time(index), _in_time_hour);
  EEPROM.write(eeprom_addr_in_time(index) + 1, _in_time_minute);
  EEPROM.write(eeprom_addr_out_time(index), _out_time_hour);
  EEPROM.write(eeprom_addr_out_time(index) + 1, _out_time_minute);
  configure_byte_address(eeprom_addr_is_inout(index));
}
bool clear_in_out_time_to_eeprom(uint8_t index)
{
  clear_byte_address(eeprom_addr_is_inout(index));
  return 1;
}

bool update_password_from_eeprom(uint8_t index)
{
start_again:
  is_password_configured[index] = check_is_configured_byte_address(eeprom_addr_is_pw(index));
  if (is_password_configured[index])
  {
    if (update_length_of_password(index) > 0)
    {
      if (update_length_of_password(index) > PASSWORD_STORE_COUNT)
      {
        clear_eeprom();
        goto start_again;
      }
      LoadFromEeprom(eeprom_addr_mobile(index), mobile_number[index], MOBILE_NUMBER_LENGTH);
      LoadFromEeprom(eeprom_addr_pw(index), password_value[index], password_length[index]);
      read_in_out_time_from_eeprom(index);
      return 1;
    }
    return 0;
  }
  else if (index == 0)
  {
    update_eeprom_data_at_index(0, _mobile_number, _password, 4);
    goto start_again;
  }
  return 0;
}
bool read_in_out_time_from_eeprom(uint8_t index)
{
  is_in_out_time_configured[index] = (bool)(EEPROM.read(eeprom_addr_is_inout(index)));
  in_time_hour[index] = (uint8_t)EEPROM.read(eeprom_addr_in_time(index));
  in_time_minute[index] = (uint8_t)EEPROM.read(eeprom_addr_in_time(index) + 1);
  out_time_hour[index] = (uint8_t)EEPROM.read(eeprom_addr_out_time(index));
  out_time_minute[index] = (uint8_t)EEPROM.read(eeprom_addr_out_time(index) + 1);
}

bool save_password_to_eeprom(uint8_t index, char *password_to_add, uint8_t len)
{
  for (uint8_t j = 0; j < len; j++)
  {
    password_value[index][j] = password_to_add[j];
  }
  password_length[index] = len;
  update_password_to_eeprom(index);
}

bool update_password_to_eeprom(uint8_t index)
{
  if (password_length[index] > 0)
  {
    // wdt_reset();
    EEPROM.write(eeprom_addr_pw_len(index), password_length[index]);
    // wdt_reset();
    WriteEepromArray(eeprom_addr_mobile(index), mobile_number[index], MOBILE_NUMBER_LENGTH);
    // wdt_reset();
    WriteEepromArray(eeprom_addr_pw(index), password_value[index], password_length[index]);
    // wdt_reset();
    is_password_configured[index] = 1;
    // wdt_reset();
    configure_byte_address(eeprom_addr_is_pw(index));
    if (is_in_out_time_configured[index])
    {
      update_in_out_time_to_eeprom(index, in_time_hour[index], in_time_minute[index], out_time_hour[index], out_time_minute[index]);
    }
    return 1;
  }
  return 0;
}

void clear_password_in_eeprom(uint8_t index)
{
  is_password_configured[index] = 0;
  password_length[index] = 0;
  memset(mobile_number[index], '\0', sizeof(mobile_number[index]));
  memset(password_value[index], '\0', sizeof(password_value[index]));
  // wdt_reset();
  clear_byte_address(eeprom_addr_is_pw(index));
  // wdt_reset();
  clear_byte_address(eeprom_addr_pw_len(index));
  // wdt_reset();
  // Serial.println(mobile_number_start_address[index]);
  // Serial.println(mobile_number_start_address[index]+MOBILE_NUMBER_LENGTH);
  clearEepromArray(eeprom_addr_mobile(index), eeprom_addr_mobile(index) + MOBILE_NUMBER_LENGTH);
  // wdt_reset();
  // Serial.println(password_start_address[index]);
  // Serial.println(password_start_address[index]+PASSWORD_STORE_COUNT);
  clearEepromArray(eeprom_addr_pw(index), eeprom_addr_pw(index) + PASSWORD_STORE_COUNT);
  clear_in_out_time_to_eeprom(index);
  // for (uint8_t i = 0; i < MAX_USER_TO_BE_STORED; i++)
  // {
  //   if (index != i)
  //     update_eeprom_data_at_index(i, mobile_number[i], password_value[i], password_length[i]);
  // }
  // write_alpha_speed_to_eeprom(alpha_speed);
  // write_door_open_count_to_eeprom(door_open_count);
  // write_buzzer_timeout_to_eeprom(buzzer_timeout);
  // wdt_reset();
}

int check_if_mobile_number_exists(char *arr)
{
  bool b_array_matched = 0;
  for (uint8_t i = 0; i < MAX_USER_TO_BE_STORED; i++)
  {
    if (is_password_configured[i])
    {
      b_array_matched = 1;
      Serial.println(i);

      for (uint8_t j = 0; j < MOBILE_NUMBER_LENGTH; j++)
      {
        Serial.print(mobile_number[i]);
        if (arr[j] != mobile_number[i][j])
        {
          b_array_matched = 0;
          break;
        }
        Serial.println();
      }
      if (b_array_matched)
      {
        Serial.print(i);
        Serial.println(" - Mobile Number exists!!");
        return i;
      }
    }
  }
  Serial.println("Mobile Number doesn't exists!!");
  return -1;
}

bool is_password_matched_with_any_user(char *arr, uint8_t len)
{
  for (uint8_t i = 0; i < MAX_USER_TO_BE_STORED; i++)
  {
    if (is_password_matched(i, arr, len))
    {
      Serial.println("Password matched!!");
      return 1;
    }
  }
  Serial.println("PW not matched!!");

  return 0;
}
bool is_password_matched(uint8_t index, char *arr, uint8_t len)
{
  Serial.println(index);
  if (is_password_configured[index])
  {
    for (uint8_t i = 0; i < len /*password_length[index]*/; i++)
    {
      if (arr[i] != password_value[index][i])
      {
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
/*** EEPROM function [START] **/

void WriteEepromArray(size_t address, char *arr, size_t len)
{
  for (size_t x = 0; x < len; x++)
  {
    EEPROM.write(address + x, arr[x]);
  }
}

void clearEepromArray(size_t start_address, size_t end_address)
{
  // Serial.print("start Add --");
  // Serial.println(start_address);
  // Serial.print("End Add --");
  // Serial.println(end_address);
  for (size_t x = start_address; x <= end_address; x++)
  {
    EEPROM.write(x, 0);
    // Serial.print("clearing Address -- ");
    // Serial.println(x);
  }
}

void LoadFromEeprom(size_t address, char *arr, size_t len)
{ // Be aware that this function modifys the data pointed to by "arr"
  for (size_t x = 0; x < len; x++)
  {
    arr[x] = EEPROM.read(address + x);
  }
}

bool check_is_configured_byte_address(size_t address)
{
  bool is_configured_byte_address_check = (bool)(EEPROM.read(address));
  return is_configured_byte_address_check;
}

bool configure_byte_address(size_t address)
{
  EEPROM.write(address, true);
  return true;
}

bool clear_byte_address(size_t address)
{
  EEPROM.write(address, false);
  return true;
}

/***** EEPROM SECTION [END] **/
uint32_t start, stop;

const byte ROWS = 4; // rows
const byte COLS = 4; // columns
// define the symbols on the buttons of the keypads

uint8_t times_prssd = 0;
char num_to_alpha[10][3] = {{'Y', 'Z'},
                            {'A', 'B', 'C'},
                            {'D', 'E', 'F'},
                            {'G', 'H', 'I'},
                            {'J', 'K', 'L'},
                            {'M', 'N', '0'},
                            {'P', 'Q', 'R'},
                            {'S', 'T'},
                            {'U', 'V'},
                            {'W', 'X'}};
extern bool is_num;
extern bool is_new_key;
char prev_key;

// char keys[ROWS][COLS] = {
//     {'1', '2', '3', 'x', 'x'}, // 4
//     {'4', '5', '6', '^', 'x'}, // 5
//     {'7', '8', '9', '&', 'x'}, // 6
//     {'@', '0', '#', '*', 'x'}, // 7
//     {'x', 'x', 'x', 'x', '!'}  // 2
// };
char keys[ROWS][COLS] = {
    {'1', '2', '3', 'x'}, // 4
    {'4', '5', '6', '^'}, // 5
    {'7', '8', '9', '&'}, // 6
    {'@', '0', '#', '*'}  //,      //7
};
//  11    12   13   14   3

extern char key;
// byte rowPins[ROWS] = {40, 37, 31, 30, 32}; // connect to the row pinouts of the keypad
// byte colPins[COLS] = {33, 34, 35, 36, 32}; // connect to the column pinouts of the keypad

// byte rowPins[ROWS] = {36, 34, 32, 30, 40}; // connect to the row pinouts of the keypad
// byte colPins[COLS] = {37, 35, 33, 31, 40}; // connect to the column pinouts of the keypad

byte rowPins[ROWS] = {36, 34, 32, 30}; // connect to the row pinouts of the keypad
byte colPins[COLS] = {37, 35, 33, 31}; // connect to the column pinouts of the keypad

// initialize an instance of class NewKeypad
Adafruit_Keypad customKeypad = Adafruit_Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

bool cancel_counter;
bool display_idle_screen = 1;

float temp;
float t;

const int on_switch_pin = 40;
bool on_prev_state, on_current_state;
#define LCD_STATE_ON 1
// #define LCD_STATE_OFF 0
bool lcd_state = LCD_STATE_ON;

/** LCD VARS [END] ***/
const int rs = A12, en = 22, d4 = 23, d5 = 24, d6 = 25, d7 = 26;
bool is_new_key = 0;
bool is_displayed = 0;
bool is_num = 1;
char key;
uint8_t user_id = 0;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

char password[15];
char phone_number[10];

uint8_t ph_len = 0;
uint8_t pass_length = 0;
#ifndef MAX_NUM_OF_USERS
#define MAX_NUM_OF_USERS MAX_USER_TO_BE_STORED
#endif
// bool does_user_exist[MAX_NUM_OF_USERS] = {0};  // Removed: redundant with is_password_configured, saves 18 bytes RAM

extern bool check_if_password_is_configured(uint8_t index);
uint8_t display_screen = 0;

extern bool b_command_open_door;
extern bool b_command_close_door;
/** LCD VARS [END] ***/

uint32_t prss_time, rels_time, prev_rels_time, time_difference;
char chararr[50];  // Reduced from 100 to save 50 bytes RAM (appears unused)
uint16_t lcd_press_counter = 0;
unsigned long lastDebounceTime = 0; // the last time the output pin was toggled
unsigned long debounceDelay = 100;  // the debounce time; increase if the output flickers

unsigned long door_open_start_time;
unsigned long door_error_start_time;
#define DOOR_OPEN_TIMEOUT 10000
#define DOOR_OPEN_ERROR_TIMEOUT 5000

#define GUN_POINT_PRESS_TIMEOUT 3000

bool b_vibration_alarm_triggered = 0;
bool b_gun_point_activation_triggerd = 0;

bool b_error_in_door_open = 0;
bool b_error_in_door_close = 0;

const int siren_pin[2] = {27, 28};
bool current_ir_value = 0;
bool previous_ir_value = 0;

const int ir_input_pin = A0;
bool b_siren_on = 0;


// #define BYPASS_ALL_SENSOR_AND_DOOR_INPUTS 1


bool is_door_aligned_by_ir()
{
#ifdef BYPASS_ALL_SENSOR_AND_DOOR_INPUTS
 return 1;
#else
  return !current_ir_value;
#endif
}

void siren_on(uint8_t i)
{
  digitalWrite(i, 1);
  b_siren_on = 1;
}
void siren_off(uint8_t i)
{
  digitalWrite(i, 0);
  b_siren_on = 0;
}
const int analogInPin = A14; // Analog input pin that the potentiometer is attached to
uint16_t battery_analog_input;
uint16_t battery_percentage;
// const int siren_pin = 28;
void gpio_init()
{
  //  keypad_setup(4, 4, rows, cols, values);     // Setup dimensions, keypad arrays
  customKeypad.begin();
  pinMode(on_switch_pin, INPUT_PULLUP);
  pinMode(siren_pin[0], OUTPUT);
  pinMode(siren_pin[1], OUTPUT);
  pinMode(ir_input_pin, INPUT_PULLUP);

  current_ir_value = digitalRead(ir_input_pin);
  previous_ir_value = current_ir_value;
  // pinMode(14,OUTPUT);
  // digitalWrite(14,HIGH);
  siren_off(siren_pin[0]);
  // siren_off(siren_pin[1]);
  on_prev_state = on_current_state = digitalRead(on_switch_pin);
  battery_analog_input = analogRead(analogInPin);
  battery_percentage = map(battery_analog_input, 0, 900, 0, 100);
  if (battery_percentage > 100)
  {
    battery_percentage = 100;
  }
  // for (uint8_t i = 0; i < 2; i++)
  // {
  //   pinMode(siren_pin[i], OUTPUT);
  //   siren_off(i);
  //   // digitalWrite(siren_pin[i], 1);
  // }
}
uint16_t on_off_counter = 0;
uint16_t display_on_timeout = 30000;
unsigned long display_on_timer;
void gpio_task()
{
  on_current_state = digitalRead(on_switch_pin);
  on_current_state = 0;
  // if (on_current_state != on_prev_state)
  // {
  //   // reset the debouncing timer
  //   lastDebounceTime = millis();
  // }
  current_ir_value = digitalRead(ir_input_pin);
  if (previous_ir_value != current_ir_value)
  {
    previous_ir_value = current_ir_value;
    is_displayed = 0;
    Serial.print("IR value changed to ");
    Serial.println(current_ir_value);
  }

  if ((millis() - lastDebounceTime) > debounceDelay)
  {
    battery_analog_input = analogRead(analogInPin);
    battery_percentage = map(battery_analog_input, 0, 900, 0, 100);
    if (battery_percentage > 100)
    {
      battery_percentage = 100;
    }
    if (on_prev_state != on_current_state)
    {
      on_prev_state = on_current_state;
      // Serial.println("LCD_PIN_STATE - ");
      // Serial.println(on_prev_state);
      if (on_current_state)
      {
        if (on_off_counter > 0)
        {
          if (lcd_state == LCD_STATE_ON)
          {
            // lcd_state = LCD_STATE_OFF;
            lcd_power_off();
            Serial.println("LCD_OFF");

            // SIM7600.end();
            //       // is_displayed = 1;
          }
          else
          {
            lcd_state = LCD_STATE_ON;
            lcd_power_on();
            if (b_gun_point_activation_triggerd || b_temperature_alarm_triggerd || b_vibration_alarm_triggered)
            {
            }
            else
            {
              // digitalWrite(14,HIGH);
            }

            display_on_timer = millis();
            Serial.println("LCD_ON");
            // gsm_module_init();

            //       // is_displayed = 0;
            //       // display_screen = MAIN;
          }
        }
        on_off_counter = 0;
      }
      else
      {
        on_off_counter++;
      }
    }
  }

  // if (b_gun_point_activation_triggerd)
  // {
  //   if (!b_siren_on)
  //   {
  //     Serial.println("Siren On");
  //     siren_on(1);
  //     siren_on(0);
  //   }
  // }
  customKeypad.tick();
  while (customKeypad.available())
  {
    keypadEvent e = customKeypad.read();
    Serial.print((char)e.bit.KEY);
    if (e.bit.EVENT == KEY_JUST_PRESSED)
    {
      prss_time = millis();
      Serial.println(F(" pressed"));
      if (lcd_state == LCD_STATE_ON)
      {
        tone(buzzer_pin, pgm_read_word(&melody[0]), 200);
        delay(100);
        noTone(buzzer_pin);
      }
    }
    else if (e.bit.EVENT == KEY_JUST_RELEASED)
    {
      time_difference = millis() - prss_time;
      // Serial.print(F("Difference "));
      // Serial.println(time_difference);
      // Serial.println(F(" released"));
      if (time_difference < 5)
      {
        // debounce: ignore very short presses
        continue;
      }
      key = (char)e.bit.KEY;
      Serial.print(F("key: "));
      Serial.println(key);
      // if (lcd_state == LCD_STATE_OFF)
      // {
      //   // return;
      //   lcd_power_on();
      //   lcd_init();
      //   lcd_state = LCD_STATE_ON;
      //   is_displayed = 0;
      //   display_screen = MAIN;
      // }
      display_on_timer = millis();
      switch (key)
      {
      case POWER:
        // LOCK the safe and off the display
        break;
      case MUTE:
        // toggle the mute setting
        Serial.println(F("MUTE PRESSED!"));
        b_buzzer_on = 0;
        door_open_time = millis();
        break;
      case LOCK:
        // LOCK the safe and off the display
        rels_time = millis();
        // is_new_key = 1;
        is_displayed = 0;
        b_command_close_door = 1;
        Serial.println(F("4442"));
        b_error_in_door_close = 0;
        display_screen = LOCK_DOOR_STATE;
        break;
      default:
        if (lcd_state == LCD_STATE_ON)
        {
          Serial.println(F("pressed default case"));
          rels_time = millis();
          is_new_key = 1;
          break;
        }
      }
    }
  }
  //  while (Serial.available()) {
  //    is_new_key = 1;
  //    String str = Serial.readString();
  //    Serial.print("Entered key is:");
  //    Serial.print(str);
  //    str.toCharArray(chararr, str.length());
  //    key = chararr[0];
  //    Serial.println(key);
  //  }
}
char get_character;
bool is_new_index()
{
  // Serial.print("prev_key: ");
  // Serial.println(prev_key);
  // Serial.print("key: ");
  // Serial.println(key);
  // Serial.print("is_num: ");
  // Serial.println(is_num);
  // Serial.print("key in uint8_t : ");
  // Serial.println(uint8_t(key));
  // Serial.print("rels_time: ");
  // Serial.println(rels_time);
  // Serial.print("prev_rels_time: ");
  // Serial.println(prev_rels_time);
  // Serial.print("time difference: ");
  // Serial.println(rels_time - prev_rels_time);
  uint8_t temp_key = uint8_t(key) - 48;
  if (!is_num)
  {
    if (prev_key != key || (rels_time - prev_rels_time) > alpha_speed)
    {
      prev_rels_time = rels_time;
      times_prssd = 0;
      prev_key = key;
      get_character = num_to_alpha[temp_key][times_prssd];
      return true;
    }
    else if ((rels_time - prev_rels_time) < alpha_speed)
    {
      prev_rels_time = rels_time;
      times_prssd++;
      if (temp_key && temp_key <= 6)
      {
        get_character = num_to_alpha[temp_key][times_prssd % 3];
      }
      else
      {
        get_character = num_to_alpha[temp_key][times_prssd % 2];
      }
      return false;
    }
  }
  times_prssd = 0;
  get_character = key;
  return true;
}
char get_pressed_character()
{
  return get_character;
}

bool b_send_open_door_message = 0;
bool b_send_close_door_message = 0;

#define OPEN_DOOR_MSG 1
#define CLOSE_DOOR_MSG 2
#define GUN_POINT_MSG 3
#define GUN_POINT_CALL 4
#define TEMP_ALARM_MSG 5
#define VIBRATION_ALARM_MSG 6
#define AUTH_FAIL_MSG 7

uint8_t type_list[10];
uint8_t message_details[10];
uint8_t queue_index = 0;

/***** DC MOTOR SECTION [START] **/
int dc_motor_pin[2] = {2, 5};
int sensor_pin[2] = {48, 47};
int em_lock_control_pin = 6;

int ir_rx_pin = A0;

#define CW 0
#define CCW 1

bool is_door_closing = 0;
bool is_door_opening = 0;

bool door_sensor_state[2] = {0, 0};
bool prev_door_sensor_state[2] = {0, 0};

bool ir_state = 0;
bool prev_ir_state = 0;

bool b_command_open_door = 0;
bool b_command_close_door = 0;

void init_dc_motor()
{
  // put your setup code here, to run once:
  pinMode(dc_motor_pin[0], OUTPUT);
  pinMode(dc_motor_pin[1], OUTPUT);
  pinMode(sensor_pin[0], INPUT_PULLUP);
  pinMode(sensor_pin[1], INPUT_PULLUP);
//  pinMode(em_lock_control_pin, OUTPUT);
//  digitalWrite(em_lock_control_pin, 1);

  pinMode(ir_rx_pin, INPUT);

  dc_motor_stop();
  // close_door();
  // b_command_close_door = 1;
}
void dc_motor_task()
{
  ir_state = digitalRead(ir_rx_pin);
  if (prev_ir_state != ir_state)
  {
    prev_ir_state = ir_state;
  }

  for (char i = 0; i < 2; i++)
  {
    door_sensor_state[i] = digitalRead(sensor_pin[i]);
    if (door_sensor_state[i] != prev_door_sensor_state[i])
    {
      prev_door_sensor_state[i] = door_sensor_state[i];
      if (i == 0)
      {
        if (is_door_open())
        {
          Serial.println(F("DOOR OPEN"));
          // b_command_close_door = 1;
        }
        else
        {
          Serial.println(F("DOOR OPEN SENSOR UNHIT"));
        }
      }
      else if (i == 1)
      {
        if (is_door_close())
        {
          Serial.println(F("DOOR CLOSE"));
        }
        else
        {
          Serial.println(F("DOOR CLOSE SENSOR UNHIT"));
        }
      }
    }
  }
  if (is_door_opening && is_door_open())
  {
    dc_motor_stop();
    is_door_opening = 0;
    if (b_error_in_door_open)
    {
      b_error_in_door_open = 0;
      is_displayed = 0;
    }
  }
  if (is_door_closing && is_door_close())
  {
    dc_motor_stop();
    is_door_closing = 0;
    if (b_error_in_door_close)
    {
      b_error_in_door_close = 0;
      is_displayed = 0;
    }
  }
  if (b_command_close_door and is_door_aligned_by_ir())
  {
    b_command_close_door = 0;
    dc_motor_stop();
    close_door();
  }
  if (b_command_open_door)
  {
    b_command_open_door = 0;
    dc_motor_stop();
    open_door();
  }
}
void dc_motor_stop()
{
   digitalWrite(dc_motor_pin[0], 1);
   digitalWrite(dc_motor_pin[1], 1);
}
void dc_motor_on(int direction)
{
  if (direction == CW)
  {
//    digitalWrite(em_lock_control_pin, 0);
//    delay(300);
//    digitalWrite(em_lock_control_pin, 1);
     Serial.println(F("Moving CW"));
     digitalWrite(dc_motor_pin[0], 0);
     digitalWrite(dc_motor_pin[1], 1);
  }
  else if (direction == CCW)
  {
     Serial.println(F("Moving CCW"));
     digitalWrite(dc_motor_pin[0], 1);
     digitalWrite(dc_motor_pin[1], 0);
  }
}

bool is_door_open()
{
#ifdef BYPASS_ALL_SENSOR_AND_DOOR_INPUTS
  return 1;
#else
  return !door_sensor_state[0];
#endif
}
bool is_door_close()
{
#ifdef BYPASS_ALL_SENSOR_AND_DOOR_INPUTS
  return 1;
#else
  return !door_sensor_state[1]; // !door_sensor_state[1];
#endif
}

void open_door()
{
  is_door_opening = 1;
  dc_motor_on(CW);
}
void close_door()
{
  is_door_closing = 1;
  dc_motor_on(CCW);
}
/***** DC MOTOR SECTION [END] **/
/*************** LCD CODE [START] ******************/
void lcd_init()
{
  lcd.noDisplay(); // Turn off the display
  lcd.begin(16, 2);
  lcd.display(); // Turn on the display
  lcd.setCursor(1, 0);
  // lcd.print("BMS SECURITIES");
  LCD_PRINT("BMS SAFE (");
  lcd.print(battery_percentage);
  LCD_PRINT("%)");
  lcd.setCursor(0, 1);
  if (date < 10)
    lcd.print("0");
  lcd.print(date);
  LCD_PRINT("/");

  if (month < 10)
    lcd.print("0");
  lcd.print(month);
  lcd.print("/");

  lcd.print(year);
  // lcd.print(":");

  // if (second < 10)
  //   lcd.print("0");
  // lcd.print(second);

  LCD_PRINT("  ");

  if (hour < 10)
    lcd.print("0");
  lcd.print(hour);
  LCD_PRINT(":");

  if (minute < 10)
    lcd.print("0");
  lcd.print(minute);

  // delay(500);

  // TODO: PRINT TIME AND DATE
  delay(500);
  lcd.setCursor(0, 1);
}
void lcd_init_screen()
{
  lcd.noDisplay(); // Turn off the display
  lcd.begin(16, 2);
  lcd.display(); // Turn on the display
  lcd.setCursor(1, 0);
  LCD_PRINT("   BMS SAFE");
  lcd.setCursor(0, 1);
  LCD_PRINT("....WELCOME....");
  if (b_sd_card_not_initiated)
  {
    delay(1000);
    lcd.setCursor(0, 0);
    LCD_PRINT("  NO SD CARD ");
    lcd.setCursor(0, 1);
    LCD_PRINT("  ATTACHED!!");
  }

  /*
    if (date < 10)
      lcd.print("0");
    lcd.print(date);
    lcd.print("/");

    if (month < 10)
      lcd.print("0");
    lcd.print(month);
    lcd.print("/");

    lcd.print(year);
    // lcd.print(":");

    // if (second < 10)
    //   lcd.print("0");
    // lcd.print(second);

    lcd.print("  ");

    if (hour < 10)
      lcd.print("0");
    lcd.print(hour);
    lcd.print(":");

    if (minute < 10)
      lcd.print("0");
    lcd.print(minute);
  */
  delay(1000);
}
void my_delay(uint8_t unit)
{
  delay(unit * 1000);
}
void jump_to_master_main()
{
  my_delay(3);
  is_displayed = 0;
  Serial.println("MASTER_MAIN");
  display_screen = MASTER_MAIN;
}
void jump_to_user_main()
{
  my_delay(3);
  is_displayed = 0;
  Serial.println("USER_MAIN");
  display_screen = USER;
}
void door_open_close_fsm()
{
  // @TODO: Write the logic for Open Close Door FSM
  // @TODO: Door OPEN count to be stored in FSM
}
void temperature_sensor_fsm()
{
  // @TODO:
}
void vibration_sensor_fsm()
{
  // @TODO:
}

#define USER_NO_REGISTERED 1              //"User is not registered!"
#define PW_LENGH_IS_NOT_IN_LIMIT 2        //"Password length is greater than 15 or less than 4"
#define PW_CHANGED 3                      //"Password Changed!!"
#define PW_IS_NO_VALID 4                  //"Password is not valid!!"
#define PARA_MISSING 5                    //"Parameters are missing!!"
#define USER_REMOVED 6                    //"User Removed!"
#define USER_CREATED 7                    //"User Created!"
#define USER_IS_ALREADY_REGISTERD 8       //"User is already registered"
#define MOBILE_NUM_LEN_IS_INVALID 9       //"Mobile number length is less or more"
#define PW_IS_NOT_CONFIGURED 10           //"Password is not configured!"
#define MOBILE_NUMBER_IS_NOT_REGISTERD 11 //"Mobile Number is not registered"
#define MASTER_RESET_DONE 12
#define PARA_INVALID 13
#define IN_OUT_TIME_UPDATED 14
#define NO_ACCESS_ALLOWED 15
#define DOOR_UNLOCK_CMD_ACCEPTED 16
#define DOOR_LOCK_CMD_ACCEPTED 17
#define USER_CREATED_ACK 18
#define OTP_MATCHED 19
#define OTP_NOT_MATCHED 20

#define RECEIVED_MOBILE_NUMBER_INDEX (MAX_NUM_OF_USERS + 1)
#define MASTER_USER_ID 0



#define GPA_DO_NOTHING 0
#define GPA_SEND_MESSAGE 1
#define GPA_CALL 2
uint8_t gpa_state = GPA_SEND_MESSAGE;

uint8_t current_gpa_user_id = 0;

const int vibration_sensor_pin = 43;

bool vibration_sensor_pin_status = 0;
bool prev_vibration_sensor_pin_status = 0;
bool vibration_started = 0;

unsigned long vibration_read_timer;
unsigned int vibration_read_time_interval = 10000;
uint16_t vibration_change_counter = 0;

uint8_t temperature_counter = 0;

void temp_sen_init()
{
  sensors.begin();
  pinMode(vibration_sensor_pin, OUTPUT);
}
void read_temperature()
{
  sensors.requestTemperatures(); // Send the command to get temperature readings
  temperature_value = sensors.getTempCByIndex(0);
  temperature_read_timeout = millis();
}

void temp_task()
{
  vibration_sensor_pin_status = digitalRead(vibration_sensor_pin);
  if (prev_vibration_sensor_pin_status != vibration_sensor_pin_status)
  {
    prev_vibration_sensor_pin_status = vibration_sensor_pin_status;
    //    Serial.print("Vibration sensor status :");
    //    Serial.println(prev_vibration_sensor_pin_status);
    if (vibration_sensor_pin_status)
    {

      if (vibration_change_counter < 2)
      {
        vibration_started = 1;
        Serial.println("Vibration Started!------------------------------>");
      }
      if (vibration_started)
      {
        vibration_read_timer = millis();
      }
      vibration_change_counter++;
      Serial.print("Vibration_counter -->>");
      Serial.println(vibration_change_counter);
      if (vibration_change_counter > 15)
      {
        vibration_started = 0;
        b_vibration_alarm_triggered = 1;
        siren_on(siren_pin[0]);
        generate_random_otp();
        vibration_change_counter = 0;
        Serial.println("Vibration alarm Triggered!!------------------------------>");
        lcd_power_off();
        // lcd_state = LCD_STATE_OFF;
      }
    }
    if (vibration_started)
    {
      if (millis() - vibration_read_timer > vibration_read_time_interval)
      {
        vibration_started = 0;
        vibration_change_counter = 0;
        Serial.println("Vibration Stopped!------------------------------>");
      }
    }
  }

  if (millis() - temperature_read_timeout > temperature_read_time_interval)
  {
    read_temperature();
    if (prev_temperature_value != temperature_value)
    {
      prev_temperature_value = temperature_value;
      if (temperature_value > TEMPERATURE_THRESHOLD)
      {
        if (temperature_counter < 200)
          temperature_counter++;

        if (temperature_counter > 3)
        {
          b_temperature_alarm_triggerd = 1;
          generate_random_otp();
          lcd_power_off();
          siren_on(siren_pin[0]);
          // lcd_state = LCD_STATE_OFF;
          if (gpa_state == GPA_DO_NOTHING)
          {
            gpa_state = GPA_SEND_MESSAGE;
          }
        }
      }
      else
      {
        if (!b_temperature_alarm_triggerd)
          temperature_counter = 0;
      }
    }
    Serial.print("Temperature value is : ");
    Serial.println(temperature_value);
  }
}
void gun_point_activation_fsm()
{
  //@TODO

  if (b_gun_point_activation_triggerd || b_temperature_alarm_triggerd || b_vibration_alarm_triggered)
  {
    switch (gpa_state)
    {
    case GPA_DO_NOTHING:
      break;
    case GPA_SEND_MESSAGE:
      if (queue_index < 1)
      {
        for (uint8_t i = 0; i < MAX_NUM_OF_USERS; i++)
        {
          if (b_temperature_alarm_triggerd)
          {
            if (is_password_configured[i])
            {
              update_queue(TEMP_ALARM_MSG, i);
            }
          }
          else if (b_vibration_alarm_triggered)
          {
            if (is_password_configured[i])
            {
              update_queue(VIBRATION_ALARM_MSG, i);
            }
          }
          else
          {
            if (is_password_configured[i] && (user_id - 1) != i)
            {
              update_queue(GUN_POINT_MSG, i);
            }
          }
        }
        gpa_state = GPA_CALL;
        break;
      }

      // if (current_gpa_user_id > MAX_NUM_OF_USERS)
      // {
      //   current_gpa_user_id = 0;
      //   gpa_state = GPA_CALL;
      //   break;
      // }
      // if (user_id == current_gpa_user_id)
      // {
      //   current_gpa_user_id++;
      //   if (!is_password_configured[current_gpa_user_id])
      //   {
      //     current_gpa_user_id++;
      //   }
      // }
      // else
      // {
      //   update_queue(GUN_POINT_MSG, current_gpa_user_id);
      //   current_gpa_user_id++;
      // }
      // break;
    case GPA_CALL:
      if (queue_index < 1)
      {
        // Serial.println("GPA_CALL -------------------> ");
        for (uint8_t i = 0; i < MAX_NUM_OF_USERS; i++)
        {
          if (b_temperature_alarm_triggerd)
          {
            if (is_password_configured[i])
            {
              update_queue(GUN_POINT_CALL, i);
            }
          }
          else
          {
            if (is_password_configured[i] && (user_id - 1) != i)
            {
              update_queue(GUN_POINT_CALL, i);
            }
          }
        }
        gpa_state = GPA_SEND_MESSAGE;
      }
      break;
    }
  }
}
#define MASTER_PW_LEN 10
uint8_t master_reset_pw[MASTER_PW_LEN] = {'9', '9', '2', '5', '3', '6', '6', '1', '1', '1'};

bool check_if_user_is_allowed_in_time_slot(uint8_t _user_id)
{
  if (is_in_out_time_configured[_user_id])
  {
    hour = rtc.hour();
    minute = rtc.minute();
    uint8_t in_time = in_time_hour[_user_id] * 100 + in_time_minute[_user_id];
    uint8_t out_time = out_time_hour[_user_id] * 100 + out_time_minute[_user_id];

    uint8_t now_time = hour * 100 + minute;

    if (now_time >= in_time && now_time <= out_time)
    {
      Serial.println("Access Allowed!");
    }
    else
    {
      Serial.println("No Access Allowed!");
    }
    // if(hour == in_time_hour[_user_id] && minute >in_time_minute)
    // if((hour =>in_time_hour[_user_id] && hour <=out_time_hour[_user_id] && (minute =>in_time_minute[_user_id] && minute <=out_time_minute[_user_id])
    return 1;
  }
  else
  {
    return 1;
  }
}
bool b_access_allowed = 0;

void check_if_door_access_is_allowed(uint8_t user_id)
{
  is_displayed = 0;
  pass_length = 0;
  // for (uint8_t i = 0; i < pass_length; i++)
  //   password[i] = '/0';
  if (!is_door_aligned_by_ir())
  {
    lcd.clear();
    lcd.setCursor(5, 0);
    LCD_PRINT("Sensor");
    lcd.setCursor(1, 1);
    LCD_PRINT("Not Aligned!!");
    is_displayed = 0;
    delay(2000);
    b_command_close_door = 0;
    b_command_open_door = 0;
    dc_motor_stop();
    display_screen = MAIN;
    pass_length = 0;
    return 0;
  }

  if (check_if_user_is_allowed_in_time_slot(user_id))
  {
    b_access_allowed = 1;
  }
  if (b_access_allowed)
  {
    door_open_count = door_open_count + 1;
    write_door_open_count_to_eeprom(door_open_count);
    if (user_id == 1)
    {
      Serial.println("MASTER_MAIN");
      display_screen = MASTER_MAIN;
      b_command_open_door = 1;
    }
    else
    {
      Serial.println("USER");
      display_screen = USER;
      // if(check_if_user_is_allowed_in_time_slot(user_id)){
      // }
      b_command_open_door = 1;
    }
    return 1;
  }
  else
  {
    lcd.clear();
    lcd.setCursor(0, 0);
    LCD_PRINT("Invld Password!!");
    is_displayed = 0;
    delay(1000);
    display_screen = LOCK_DOOR_STATE;
    pass_length = 0;
    return 0;
  }
}
#define FINGERPRINT_FSM_STATE_DEFAULT           1
#define FINGERPRINT_FSM_STATE_WRONG_MASTER      2
#define FINGERPRINT_FSM_STATE_ENTER_USER        3
#define FINGERPRINT_FSM_STATE_WRONG_USER        4
#define FINGERPRINT_FSM_STATE_DOOR_UNLOCKED     5

uint8_t fingerprint_manager_fsm_state = FINGERPRINT_FSM_STATE_DEFAULT;
int8_t first_user_verified =0;
uint8_t user_bio_auth_fail_count = 0;
bool verify_dual_password(){
  uint8_t user_id_length = 0;
  user_id = parse_user_id_from_password(password, pass_length, &user_id_length);
  if (user_id == 0) return 0; // Invalid user ID format
  
  if ((first_user_verified || user_id == 1) && is_password_valid(user_id, &password[user_id_length], pass_length - user_id_length))
  {
    if(first_user_verified){
      if(user_id == 1){
        Serial.println("MASTER_INPUT_STATE");
        display_screen = MASTER_INPUT_STATE;
        is_displayed = 0;
        user_bio_auth_fail_count = 0; // Reset on successful authentication
        first_user_verified = 0; // reset first user verified flag as it's just open master menu
      }else{
        // TODO: Unlock the safe
        // user_id already set by parse_user_id_from_password
        Serial.print("USER ID -- >");
        Serial.println(user_id);
        // call funtion
        check_if_door_access_is_allowed(user_id);
        first_user_verified = 0;
        user_bio_auth_fail_count = 0; // Reset on successful authentication
      }
    }else if(user_id == 1){ 
      // if condition is not required as it should be true by default as main if has two conditions only
        first_user_verified = 1;
        is_displayed = 1;
        // lcd.clear();
        // lcd.setCursor(0, 0);
        // lcd.print("ENTER USER PW:");
        lcd.clear();
        lcd.setCursor(0, 0);
        LCD_PRINT("USER PASS/BIO :");
        pass_length = 0;
        fingerprint_manager_fsm_state = FINGERPRINT_FSM_STATE_ENTER_USER;
        memset(password, '\0', 15);
        user_id = 0; // Invalid user ID
        user_bio_auth_fail_count = 0; // Reset when entering USER PASS/BIO screen
      
    }
  }
  else
  {
    Serial.print("USER ID is ");
    Serial.println(user_id);
    lcd.clear();
    lcd.setCursor(0, 0);
    LCD_PRINT("Invld Password!!");
    is_displayed = 0;
    
    // Send alert for invalid password attempt
    update_queue(AUTH_FAIL_MSG, MASTER_USER_ID);
    delay(1000);
    
    // Check if this is a failure on USER PASS/BIO screen
    if (first_user_verified == 1) {
      user_bio_auth_fail_count++;
      if (user_bio_auth_fail_count >= 1) {
        // Send alert to master user (index 0)
        update_queue(AUTH_FAIL_MSG, MASTER_USER_ID);
        user_bio_auth_fail_count = 0; // Reset after sending alert
        // Return to MAIN screen after 2nd failure
        display_screen = MAIN;
        pass_length = 0;
        first_user_verified = 0;
        memset(password, '\0', 15);
      } else {
        // Stay on USER PASS/BIO screen for retry
        lcd.clear();
        lcd.setCursor(0, 0);
        LCD_PRINT("USER PASS/BIO :");
        pass_length = 0;
        memset(password, '\0', 15);
        is_displayed = 1;
      }
    } else {
      // Return to MAIN screen if failure on MAIN screen
      display_screen = MAIN;
      pass_length = 0;
      first_user_verified = 0;
    }
    
    return 0;
  }
}
bool verify_password()
{
  Serial.println(password);
  if (password[0] == master_reset_pw[0] && pass_length == MASTER_PW_LEN)
  {
    bool b_pw_matched = 1;
    for (uint8_t i = 0; i < MASTER_PW_LEN; i++)
    {
      if (password[i] != master_reset_pw[i])
      {
        b_pw_matched = 0;
      }
    }
    if (b_pw_matched)
    {
      for (uint8_t i = 0; i < MAX_NUM_OF_USERS; i++)
      {
        if (check_if_password_is_configured(i))
        {
          clear_password_in_eeprom(i);
        }
        wdt_reset();
      }
    }
    write_door_open_count_to_eeprom(0);
    wdt_reset();
    update_eeprom_data_at_index(0, _mobile_number, _password, 4);
    update_data_from_eeprom();
    wdt_reset();
    lcd.clear();
    lcd.setCursor(0, 0);
    LCD_PRINT("Lock Reseted!!");
    is_displayed = 0;
    delay(1000);
    display_screen = LOCK_DOOR_STATE;
    pass_length = 0;
    return 1;
  }
  uint8_t user_id_length = 0;
  user_id = parse_user_id_from_password(password, pass_length, &user_id_length);
  if (user_id == 0) return 0; // Invalid user ID format
  
  if (is_password_valid(user_id, &password[user_id_length], pass_length - user_id_length))
  {
    // TODO: Unlock the safe
    Serial.print("USER ID -- >");
    Serial.println(user_id);
    // call funtion
    check_if_door_access_is_allowed(user_id);
  }
  else
  {
    lcd.clear();
    lcd.setCursor(0, 0);
    LCD_PRINT("Invld Password!!");
    is_displayed = 0;
    delay(1000);
    display_screen = LOCK_DOOR_STATE;
    pass_length = 0;
    return 0;
  }
}
uint8_t otp[6];
uint8_t otp_length = 0;
uint8_t generated_otp[6] = {
    random(0, 9),
    random(0, 9),
    random(0, 9),
    random(0, 9),
    random(0, 9),
    random(0, 9),
};

uint8_t master_otp[6] = {
    4, 5, 5, 5, 5, 6};

char char_generated_otp[6];
bool b_otp_not_matched = 0;
void generate_random_otp()
{
  uint16_t temp = (minute + (hour * 10)) / 10;
  if (temp > 10)
  {
    temp = temp / 10;
    if (temp > 10)
    {
      temp = temp / 10;
      if (temp > 10)
      {
        temp = temp / 10;
      }
    }
  }
  for (uint8_t i = 0; i < 6; i++)
  {
    generated_otp[i] = random(temp, 9);
    char_generated_otp[i] = generated_otp[i] + '0';
  }
  Serial.print("Generated OTP : ");
  Serial.println((char_generated_otp));
}
void input_otp_fsm()
{

  if (!is_displayed)
  {
    is_displayed = 1;
    lcd.clear();
    lcd.setCursor(0, 0);
    // if (input_mobile_number_count == 0)
    // {
    //   lcd.print("MOBILE NUMBER :");
    // }
    // else
    // {
    //   lcd.print("RECONFIRM :");
    // }
    // lcd.setCursor(11, 0);
    lcd.setCursor(11, 0);
    for (uint8_t i = 0; i < otp_length; i++)
    {
      lcd.setCursor(i, 1);
      lcd.print(otp[i]);
    }
  }
  else
  {
    if (is_new_key)
    {
      is_new_key = 0;
      // Serial.println(key);
      // Serial.println("::::::::::::::::::::::::");
      switch (key)
      {
      case CANCEL:
        // Serial.println("CANCEL----------------");
        if (otp_length == 0)
        {
          is_displayed = 0;
          otp_length = 0;
          memset(otp, '\0', sizeof(otp));
          // display_screen = MAIN;
        }
        else
        {
          is_displayed = 0;
          otp_length = 0;
          memset(otp, '\0', sizeof(otp));
        }
        break;
      case ENTER:
        // Serial.println("ENTER----------------");
        // Serial.println(otp_length);
        b_otp_not_matched = 0;
        // bool recheck_password = 0;
        if (otp_length == 6)
        {
          // Serial.println("ENTER-------1---------");
          for (uint8_t i = 0; i < 6; i++)
          {
            Serial.print(otp[i]);
            Serial.print(":");
            Serial.print(generated_otp[i]);
            Serial.println(">");
            if (otp[i] != generated_otp[i])
            {
              b_otp_not_matched = 1;
              // recheck_password = 1;
            }
          }
        }
        else
        {
          b_otp_not_matched = 1;
        }
        if (b_otp_not_matched)
        {
          b_otp_not_matched = 0;
          for (uint8_t i = 0; i < 6; i++)
          {
            if (otp[i] != master_otp[i])
            {
              b_otp_not_matched = 1;
            }
          }
        }
        if (!b_otp_not_matched)
        {
          Serial.println("ENTER-------2---------");
          b_gun_point_activation_triggerd = 0;
          b_vibration_alarm_triggered = 0;
          b_temperature_alarm_triggerd = 0;
          b_otp_not_matched = 0;
          siren_off(siren_pin[0]);
          // siren_off(siren_pin[1]);
          display_screen = MAIN;
          otp_length = 0;
          memset(otp, '\0', sizeof(otp));
          is_displayed = 0;
          break;
        }
        else
        {
          b_otp_not_matched = 0;
          Serial.println("ENTER-------3---------");
          otp_length = 0;
          memset(otp, '\0', sizeof(otp));
          is_displayed = 0;
          break;
        }
        break;
      default:
        if (otp_length < 16)
        {
          otp_length += uint8_t(is_new_index());
          uint8_t temp_key = uint8_t(key) - 48;

          // lcd.setCursor(input_mobile_number_length - 1, 1);
          // lcd.print(String(get_pressed_character()));
          // input_mobile_number[input_mobile_number_length - 1] = temp_key;
          otp[otp_length - 1] = temp_key;
          is_displayed = 0;
        }
        break;
      }
    }
  }
}
// #define FINGERPRINT_FSM_STATE_DEFAULT           1
// #define FINGERPRINT_FSM_STATE_WRONG_MASTER      2
// #define FINGERPRINT_FSM_STATE_ENTER_USER        3
// #define FINGERPRINT_FSM_STATE_WRONG_USER        4
// #define FINGERPRINT_FSM_STATE_DOOR_UNLOCKED     5

// uint8_t fingerprint_manager_fsm_state = FINGERPRINT_FSM_STATE_DEFAULT;
void fingerprint_manager_fsm(){
  int8_t fingerprint_id; // Declare once at function level
  switch(fingerprint_manager_fsm_state){
    case  FINGERPRINT_FSM_STATE_DEFAULT:
        fingerprint_id = getFingerprintID();
        if (fingerprint_id != -1 && fingerprint_id <= MAX_NUM_OF_USERS ){
          user_id = (uint8_t)fingerprint_id;
          if((user_id - 1) == 0){
            fingerprint_manager_fsm_state = FINGERPRINT_FSM_STATE_ENTER_USER;
            is_displayed = 1;
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("USER PASS/BIO :");
            // lcd.setCursor(0, 1);
            // lcd.print("PLEASE !!");
            first_user_verified = 1;
            user_bio_auth_fail_count = 0; // Reset when entering USER PASS/BIO screen
            delay(2000);
          }else{
            fingerprint_manager_fsm_state = FINGERPRINT_FSM_STATE_WRONG_MASTER;
          }
        }else if(fingerprint_id != -1){
            fingerprint_manager_fsm_state = FINGERPRINT_FSM_STATE_WRONG_MASTER;
        }
      break;
    case FINGERPRINT_FSM_STATE_WRONG_MASTER:
      is_displayed = 1;
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("MASTER FINGERPRNT");
      lcd.setCursor(0, 1);
      lcd.print("NOT MATCHED!");
      user_id = 0; // Invalid user ID
      
      // Send alert for invalid master fingerprint attempt
      update_queue(AUTH_FAIL_MSG, MASTER_USER_ID);
      
      fingerprint_manager_fsm_state = FINGERPRINT_FSM_STATE_DEFAULT;
      delay(2000);
      is_displayed = 0;
      break;
    case FINGERPRINT_FSM_STATE_ENTER_USER:
      fingerprint_id = getFingerprintID();
      if (fingerprint_id != -1 && fingerprint_id <= MAX_NUM_OF_USERS){
        user_id = (uint8_t)fingerprint_id;
        if( check_if_password_is_configured(user_id - 1) && (user_id -1 )!=0){
          is_displayed = 1;
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("USER FINGERPRNT");
          lcd.setCursor(0, 1);
          lcd.print("MATCHED!!");
          delay(2000);
          is_displayed = 0;
          fingerprint_manager_fsm_state = FINGERPRINT_FSM_STATE_DOOR_UNLOCKED;
          user_bio_auth_fail_count = 0; // Reset on successful authentication
          break;
        }else{
          fingerprint_manager_fsm_state = FINGERPRINT_FSM_STATE_WRONG_USER;
          break;
        }
      }else if(fingerprint_id != -1){
        fingerprint_manager_fsm_state = FINGERPRINT_FSM_STATE_WRONG_USER;
        break;
      }
       break;
    case FINGERPRINT_FSM_STATE_WRONG_USER:
      is_displayed = 1;
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("USER FINGERPRNT");
      lcd.setCursor(0, 1);
      lcd.print("NOT MATCHED!");
      user_id = 0; // Invalid user ID
      
      // Track fingerprint failures and send alert
      user_bio_auth_fail_count = user_bio_auth_fail_count + 2;
      // if (user_bio_auth_fail_count >= 2) {
      //   // Send alert to master user
      //   update_queue(AUTH_FAIL_MSG, MASTER_USER_ID);
      //   user_bio_auth_fail_count = 0;
      //   fingerprint_manager_fsm_state = FINGERPRINT_FSM_STATE_DEFAULT;
      //   first_user_verified = 0;
      //   display_screen = MAIN;
      //   is_displayed = 0;
      // }
      // delay(2000);
      
      // Track failure and send alert if needed
      // user_bio_auth_fail_count++;
      if (user_bio_auth_fail_count >= 2) {
        // Send alert to master user (index 0)
        update_queue(AUTH_FAIL_MSG, MASTER_USER_ID);
        user_bio_auth_fail_count = 0; // Reset after sending alert
        // Return to MAIN screen after 2nd failure
        fingerprint_manager_fsm_state = FINGERPRINT_FSM_STATE_DEFAULT;
        first_user_verified = 0;
        display_screen = MAIN;
        is_displayed = 0;
        delay(2000);
      } else {
        // Stay on USER PASS/BIO screen for retry
        fingerprint_manager_fsm_state = FINGERPRINT_FSM_STATE_ENTER_USER;
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("USER PASS/BIO :");
        is_displayed = 1;
      }
      break;
    case FINGERPRINT_FSM_STATE_DOOR_UNLOCKED:
      Serial.print("USER ID Found at ID ");
      Serial.println(user_id);
      check_if_door_access_is_allowed(user_id);
      first_user_verified = 0;
      fingerprint_manager_fsm_state = FINGERPRINT_FSM_STATE_DEFAULT;
      user_bio_auth_fail_count = 0; // Reset on successful authentication
      break;
  }
  /*
  user_id = getFingerprintID();
  if (user_id != -1 && user_id <= MAX_NUM_OF_USERS && check_if_password_is_configured(user_id - 1))
  {
    Serial.print("USER ID Found at ID ");
    Serial.println(user_id);
    check_if_door_access_is_allowed(user_id);
  }
  */
}
void password_input_fsm()
{
  if (!is_displayed)
  {
    is_displayed = 1;
    lcd.clear();
    lcd.setCursor(0, 0);
    switch (display_screen)
    {
    case MAIN:
      lcd.print("PASSWORD:");
      // lcd.setCursor(11, 0);
      break;
    case MASTER_PASSWORD:
      lcd.print("MASTER PW:");
      // lcd.setCursor(11, 0);
      break;
    case USER_PASSWORD:
      lcd.setCursor(0, 0);
      LCD_PRINT("USER-");
      if (user_id < 10) {
        lcd.print("0");
      }
      lcd.print(user_id);
      LCD_PRINT(" PW ");
      // lcd.setCursor(8, 0);
      // lcd.print(user_id);
      break;
    default:
      break;
    }
    lcd.setCursor(11, 0);
    if (is_num)
    {
      lcd.print("NUM");
    }
    else
    {
      lcd.print("ALPHA");
    }
    for (uint8_t i = 0; i < pass_length; i++)
    {
      // lcd.setCursor(i, 1);
      // lcd.print(password[i]);
      // delay(50);
      lcd.setCursor(i, 1);
      lcd.print('*');
      // lcd.print(password[i]);
    }
  }
  else
  {
    fingerprint_manager_fsm();
    // Serial.print("USER ID is ");
    // Serial.println(user_id);
    if (is_new_key)
    {
      is_new_key = 0;
      switch (key)
      {
      case ALPHA_NUM:
        // Serial.println("Coming");
        is_num = !is_num;
        is_displayed = 0;
        break;
      case CANCEL:
        is_displayed = 0;
        if (pass_length == 0)
        {
          if (display_screen == MASTER_PASSWORD)
          {
            display_screen = MASTER_INPUT_STATE;
          }
          else if (display_screen == USER_PASSWORD)
          {
            display_screen = USER;
          }
        }
        is_displayed = 0;
        pass_length = 0;
        memset(password, '\0', 15);
        user_id = 0; // Invalid user ID
        first_user_verified = 0;
        fingerprint_manager_fsm_state = FINGERPRINT_FSM_STATE_DEFAULT;
        user_bio_auth_fail_count = 0; // Reset when canceling from USER PASS/BIO screen
        break;
      case ENTER:
        if (pass_length >= 4 && pass_length <= 16)
        {
          is_displayed = 0;
          // Check for password validation
          Serial.println(password);
          switch (display_screen)
          {
          case MAIN:
            // Serial.print("Time difference : ");
            // Serial.println("Verifying PW : ");
            // verify_password();
            verify_dual_password();
            // Serial.println("Time difffffffffffffffffffffffffffff : ");
            // Serial.println(time_difference);
            if (time_difference > GUN_POINT_PRESS_TIMEOUT)
            {
              b_gun_point_activation_triggerd = 1;
              generate_random_otp();
              gpa_state = GPA_SEND_MESSAGE;
              lcd.clear();
              lcd_power_off();
              lcd_power_on();
              Serial.println("GUN POINT ACTIVATED ");
              // siren_on(siren_pin[0]);
              // siren_on(siren_pin[1]);
              // lcd_state = LCD_STATE_OFF;
              // if (display_screen == MASTER_MAIN)
              // {
              // }
              // else if (display_screen == USER)
              // {
              // }
            }
            break;
          case MASTER_PASSWORD:
            // Serial.print("PW - ");
            // Serial.println(password);
            // Serial.print("PW LEN - ");
            // Serial.println(pass_length);
            save_password_to_eeprom(0, &password[0], pass_length);
            is_displayed = 1;
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("MASTER PW:");
            lcd.setCursor(0, 1);
            lcd.print("PW UPDATED!!");
            my_delay(1);
            pass_length = 0;
            memset(password, '\0', 15);
            is_displayed = 0;
            display_screen = MASTER_MAIN;
            // jump_to_master_main();

            // lcd.setCursor(0, 1);
            break;
          case USER_PASSWORD:
            save_password_to_eeprom(user_id - 1, &password[0], pass_length);
            is_displayed = 1;
            lcd.setCursor(0, 1);
            lcd.print("PW UPDATED!!");
            my_delay(1);
            pass_length = 0;
            memset(password, '\0', 15);
            is_displayed = 0;
            display_screen = USER_INPUT_STATE;
            break;
          default:
            break;
          }
        }
        else
        {
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Password Short!");
          is_displayed = 0;
          delay(1000);
        }
        break;
      default:
        pass_length += uint8_t(is_new_index());
        lcd.setCursor(pass_length - 1, 1);
        lcd.print(String(get_pressed_character()));
        delay(400);
        lcd.setCursor(pass_length - 1, 1);
        lcd.print('*');

        password[pass_length - 1] = char(get_pressed_character());
        break;
      }
    }
  }
}

uint8_t date_time[13];
uint8_t date_time_len = 0;
uint8_t date_time_cursor_index = 0;
void date_time_input_fsm()
{
  if (!is_displayed)
  {
    is_displayed = 1;
    lcd.clear();
    lcd.setCursor(0, 0);
    // lcd.print("HHMMSS AM DDMMYY");
    lcd.print("HHMMSS  DD/MM/YY");
    lcd.setCursor(0, 1);
    date_time_cursor_index = 0;
    for (uint8_t i = 0; i < date_time_len; i++)
    {
      if (i == 6)
      {
        date_time_cursor_index = date_time_cursor_index + 2;
        lcd.setCursor(date_time_cursor_index++, 1);
        lcd.print(date_time[i]);
      }
      else if (i == 8 || i == 10)
      {
        lcd.setCursor(date_time_cursor_index++, 1);
        lcd.print("/");

        lcd.setCursor(date_time_cursor_index++, 1);
        lcd.print(date_time[i]);
      }
      else
      {
        lcd.setCursor(date_time_cursor_index++, 1);
        lcd.print(date_time[i]);
      }
      // if (i == 6)
      // {
      //   date_time_cursor_index = date_time_cursor_index + 1;
      //   lcd.setCursor(date_time_cursor_index, 1);
      //   if (date_time[date_time_len] == 1)
      //   {
      //     // lcd.setCursor(i + 1, 1);
      //     lcd.print("AM");
      //     // lcd.setCursor(i + 3, 1);
      //   }
      //   else // if (date_time[date_time_len - 1] == 2)
      //   {
      //     // lcd.setCursor(i, 1);
      //     lcd.print("PM");
      //   }
      //   date_time_cursor_index = date_time_cursor_index + 3;
      //   lcd.setCursor(date_time_cursor_index, 1);
      // }
      // else
      // {
      //   lcd.setCursor(date_time_cursor_index++, 1);
      //   lcd.print(date_time[i]);
      // }
    }
  }
  else
  {
    if (is_new_key)
    {
      is_new_key = 0;
      switch (key)
      {
      case CANCEL:
        if (date_time_len == 0)
        {
          pass_length = 0;
          is_displayed = 0;
          display_screen = MASTER_INPUT_STATE;
        }
        else
        {
          is_displayed = 0;
          date_time_len = 0;
          memset(date_time, '\0', 13);
        }
        break;
      case ENTER:
        // if (date_time_len >= 12)
        if (date_time_len == 12)
        {
          if (validate_date_and_time((date_time[4] * 10 + date_time[5]), (date_time[2] * 10 + date_time[3]), (date_time[0] * 10 + date_time[1]),
                                     (date_time[6] * 10 + date_time[7]), (date_time[8] * 10 + date_time[9]), (date_time[10] * 10 + date_time[11])))
          {
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("  DATE & TIME  ");
            lcd.setCursor(0, 1);
            lcd.print("SET SUCCESSFULLY");
            //@TODO: RTC time set function to be called
            my_delay(3);
            display_screen = MASTER_MAIN;
            is_displayed = 0;
          }
          else
          {
            memset(date_time, '\0', 13);
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("  DATE & TIME  ");
            lcd.setCursor(0, 1);
            lcd.print("INVALID DATE TIME!!");
            //@TODO: RTC time set function to be called
            my_delay(3);
            is_displayed = 0;
          }
        }
        else
        {
          lcd.clear();
          lcd.setCursor(0, 0);
          // Serial.print("DATE TIME LEN : ");
          // Serial.println(date_time_len);
          lcd.print("ADD ALL DETAILS!");
          is_displayed = 0;
          delay(1000);
        }
        break;
      default:
        bool is_valid_input = is_new_index();
        if (is_valid_input)
        {
          uint8_t temp_key = uint8_t(key) - 48;

          if (date_time_len < 12)
          {
            date_time[date_time_len] = temp_key;
            date_time_len++;
          }
          is_displayed = 0;
        }
        /*
        date_time[date_time_len - 1] = char(get_pressed_character());
        Serial.print(date_time);
        if (date_time_len == 7)
        {
          if (!(((uint8_t)(date_time[date_time_len - 1]) - 48) == 1 || ((uint8_t)(date_time[date_time_len - 1]) - 48) == 2))
          {
            date_time_len--;
            Serial.println("Ignoring input for AM & PM ");
            is_displayed = 1;
          }
        }
        else
        {
          is_displayed = 0;
        }
        */
        break;
      }
    }
  }
}

char input_mobile_number[10];
uint8_t input_mobile_number_length = 0;
uint8_t input_mobile_number_count = 0;
char prev_input_mobile_number[10];
bool b_mobile_number_not_matched = 0;

void mobile_number_input_fsm(uint8_t id)
{
  if (!is_displayed)
  {
    is_displayed = 1;
    lcd.clear();
    lcd.setCursor(0, 0);
    if (input_mobile_number_count == 0)
    {
      lcd.print("MOBILE NUMBER :");
    }
    else
    {
      lcd.print("RECONFIRM :");
    }
    // lcd.setCursor(11, 0);
    lcd.setCursor(11, 0);
    for (uint8_t i = 0; i < input_mobile_number_length; i++)
    {
      lcd.setCursor(i, 1);
      lcd.print(input_mobile_number[i]);
    }
  }
  else
  {
    if (is_new_key)
    {
      is_new_key = 0;
      switch (key)
      {
      case CANCEL:
        if (input_mobile_number_length == 0)
        {
          is_displayed = 0;
          input_mobile_number_count = 0;
          display_screen = MASTER_INPUT_STATE;
          memset(input_mobile_number, '\0', 10);
        }
        else
        {
          is_displayed = 0;
          input_mobile_number_count = 0;
          input_mobile_number_length = 0;
          memset(input_mobile_number, '\0', 10);
        }
        break;
      case ENTER:
        if (input_mobile_number_length == 10)
        {
          is_displayed = 0;
          // Check for password validation
          for (uint8_t i = 0; i < 10; i++)
            Serial.println(input_mobile_number[i]);
          if (input_mobile_number_count == 0)
          {
            input_mobile_number_count++;
            for (uint8_t i = 0; i < 10; i++)
            {
              prev_input_mobile_number[i] = input_mobile_number[i];
            }
            input_mobile_number_length = 0;
            memset(input_mobile_number, '\0', 10);
          }
          else
          {
            for (uint8_t i = 0; i < 10; i++)
            {
              if (prev_input_mobile_number[i] != input_mobile_number[i])
              {
                b_mobile_number_not_matched = 1;
              }
            }
            if (b_mobile_number_not_matched)
            {
              lcd.clear();
              lcd.setCursor(0, 0);
              lcd.print("MOBILE NUMBER :");
              lcd.setCursor(0, 1);
              lcd.print("NO NOT MATCHED!");
              input_mobile_number_count = 0;
              input_mobile_number_length = 0;
              is_displayed = 0;
              b_mobile_number_not_matched = 0;
            }
            else
            {

              if (id == 1)
              {
                lcd.clear();
                lcd.setCursor(0, 0);
                lcd.print("MOBILE NUMBER :");
                lcd.setCursor(0, 1);
                lcd.print("UPDATED!!");
                is_displayed = 0;
                input_mobile_number_count = 0;
                input_mobile_number_length = 0;
                b_mobile_number_not_matched = 1;
                Serial.print("USER ID ---->");
                Serial.println(id);
                update_eeprom_data_at_index((id - 1), input_mobile_number, password_value[(id - 1)], password_length[(id - 1)]);
                my_delay(1);
                is_displayed = 0;
                display_screen = MASTER_MAIN;
                break;
              }
              else
              {
                lcd.setCursor(0, 0);
                lcd.print("PLEASE WAIT...!!");
                lcd.setCursor(0, 1);
                lcd.print("CREATING USER-");
                lcd.print(id);
                // update_eeprom_data_at_index(temp_key - 1, _mobile_number, _password, 4);
                update_eeprom_data_at_index((id - 1), input_mobile_number, _password, 4);
                my_delay(1);
                lcd.clear();
                lcd.setCursor(0, 0);
                lcd.print("DEFAULT PASSWORD");
                lcd.setCursor(0, 1);
                lcd.print("USER-");
                lcd.print(id);
                lcd.print(": ");
                for (uint8_t i = 0; i < 4; i++)
                  lcd.print(_password[i]);
                my_delay(1);
                input_mobile_number_count = 0;
                input_mobile_number_length = 0;
                is_displayed = 0;
                display_screen = MASTER_MAIN;
                break;
                // jump_to_master_main();
              }
              // else
              // {

              //   update_eeprom_data_at_index((id - 1), input_mobile_number, password_value[(id - 1)], password_length[(id - 1)]);
              //   delay(1000);
              //   is_displayed = 0;
              //   display_screen = USER;
              // }
            }
            break;
          }
        }
        else
        {
          lcd.clear();
          lcd.setCursor(0, 1);
          lcd.print("Short!");
          is_displayed = 0;
          delay(1000);
        }
        break;
      default:
        if (input_mobile_number_length < 10)
        {
          input_mobile_number_length += uint8_t(is_new_index());
          // uint8_t temp_key = uint8_t(key) - 48;

          // lcd.setCursor(input_mobile_number_length - 1, 1);
          // lcd.print(String(get_pressed_character()));
          // input_mobile_number[input_mobile_number_length - 1] = temp_key;
          input_mobile_number[input_mobile_number_length - 1] = char(get_pressed_character());
          is_displayed = 0;
        }
        break;
      }
    }
  }
}

bool toogle_bit = 0;
void backup_screen_fsm()
{
  if (!is_displayed)
  {
    if (b_backup_in_progress == 1)
    {
      if (b_backup_complete)
      {
        b_backup_complete = 0;
        b_backup_in_progress = 0;
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("  BACKUP ");
        lcd.setCursor(0, 1);
        Serial.print("  COMPLETED!!");
        is_displayed = 0;
        display_screen = MASTER_MAIN;
      }
      else
      {

        if (toogle_bit)
        {
          toogle_bit = 0;
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("  BACKUP ");
          lcd.setCursor(0, 1);
          Serial.print("IN PROGRESS .. ");
        }
        else
        {
          toogle_bit = 1;
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("  BACKUP ");
          lcd.setCursor(0, 1);
          Serial.print("IN PROGRESS .. ..");
        }
      }
    }
    else
    {
      is_displayed = 1;
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("  WANT TO TAKE");
      lcd.setCursor(0, 1);
      lcd.print("  BACKUP..??");
      b_backup_in_progress = 0;
      b_backup_complete = 0;
    }
  }
  else
  {
    if (is_new_key)
    {
      is_new_key = 0;
      switch (key)
      {
      case CANCEL:
        is_displayed = 0;
        display_screen = MASTER_INPUT_STATE;
        break;
      case ENTER:
        lcd.clear();
        if (b_sd_card_not_initiated)
        {
          lcd.setCursor(0, 0);
          lcd.print("  NO SD CARD ");
          lcd.setCursor(0, 1);
          lcd.print("  ATTACHED!!");

          my_delay(1);
          is_displayed = 0;
          display_screen = MASTER_INPUT_STATE;
          break;
        }
        if (!b_flash_drive_attached)
        {
          lcd.setCursor(0, 0);
          lcd.print(" NO FLASH DRIVE");
          lcd.setCursor(0, 1);
          lcd.print("  ATTACHED!!");

          my_delay(1);
          is_displayed = 0;
          display_screen = MASTER_INPUT_STATE;
          break;
        }
        // if (b_flash_drive_attached && !b_sd_card_not_initiated)
        lcd.setCursor(0, 0);
        lcd.print("  BACKUP ");
        lcd.setCursor(0, 1);
        Serial.print("IN PROGRESS .. ..");
        // my_delay(1);
        is_displayed = 0;
        b_backup_in_progress = 1;
        b_backup_complete = 0;
        // display_screen = MASTER_MAIN;
        break;
      default:
        break;
      }
    }
  }
}

bool b_buzzer_timeout_updated = 0;
uint8_t buzzer_counter = 0;
uint16_t input_buzzer_timeout = 0;
void buzzer_input_fsm()
{
  if (!is_displayed)
  {
    is_displayed = 1;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("  DOOR TIMEOUT");
    lcd.setCursor(0, 1);
    lcd.print("MINUTE: ");
    if (b_buzzer_timeout_updated)
    {
      buzzer_counter++;
      b_buzzer_timeout_updated = 0;
      lcd.print(input_buzzer_timeout);
    }
  }
  else
  {
    if (is_new_key)
    {
      is_new_key = 0;
      switch (key)
      {
      case CANCEL:
        if (buzzer_counter == 0)
        {
          pass_length = 0;
          is_displayed = 0;
          display_screen = MASTER_INPUT_STATE;
        }
        else
        {
          buzzer_counter = 0;
          is_displayed = 0;
          b_buzzer_timeout_updated = 0;
          input_buzzer_timeout = 0;
        }
        break;
      case ENTER:
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(" BUZZER TIMEOUT ");
        lcd.setCursor(0, 1);
        // input_buzzer_timeout = input_buzzer_timeout + 800;
        if (input_buzzer_timeout == 0)
        {
          input_buzzer_timeout = 1;
        }
        write_buzzer_timeout_to_eeprom(input_buzzer_timeout);
        //@TODO : Needs to udpate alpha speed to EEPROM
        lcd.print("    UPDATED.  ");
        my_delay(3);
        is_displayed = 0;
        buzzer_timeout = input_buzzer_timeout;
        display_screen = MASTER_MAIN;
        break;
      default:
        b_buzzer_timeout_updated = uint8_t(is_new_index());
        if (b_buzzer_timeout_updated)
        {
          uint8_t temp_key = uint8_t(key) - 48;
          if (temp_key <= 9)
          {
            b_buzzer_timeout_updated = 1;
            input_buzzer_timeout = input_buzzer_timeout + (temp_key);
            if (input_buzzer_timeout > 150)
            {
              input_buzzer_timeout = 150;
            }
            is_displayed = 0;
          }
        }
        break;
      }
    }
  }
}
void alpha_input_fsm()
{
  if (!is_displayed)
  {
    is_displayed = 1;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("  SELECT ALPHA  ");
    lcd.setCursor(0, 1);
    lcd.print("   SPEED: ");
    if (b_alpha_speed_updated)
    {
      alpha_counter++;
      b_alpha_speed_updated = 0;
      lcd.print(input_alpha_speed);
    }
  }
  else
  {
    if (is_new_key)
    {
      is_new_key = 0;
      switch (key)
      {
      case CANCEL:
        if (alpha_counter == 0)
        {
          pass_length = 0;
          is_displayed = 0;
          display_screen = MASTER_INPUT_STATE;
        }
        else
        {
          alpha_counter = 0;
          is_displayed = 0;
          b_alpha_speed_updated = 0;
          input_alpha_speed = 0;
        }
        break;
      case ENTER:
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("  SELECT ALPHA");
        lcd.setCursor(0, 1);
        input_alpha_speed = input_alpha_speed + 800;
        write_alpha_speed_to_eeprom(input_alpha_speed);
        //@TODO : Needs to udpate alpha speed to EEPROM
        lcd.print("    UPDATED.  ");
        my_delay(3);
        is_displayed = 0;
        alpha_speed = input_alpha_speed;
        display_screen = MASTER_MAIN;
        break;
      default:
        b_alpha_speed_updated = uint8_t(is_new_index());
        if (b_alpha_speed_updated)
        {
          uint8_t temp_key = uint8_t(key) - 48;
          if (temp_key <= 9)
          {
            b_alpha_speed_updated = 1;
            input_alpha_speed = input_alpha_speed + (temp_key * 10);
            if (input_alpha_speed > 2000)
            {
              input_alpha_speed = 2000;
            }
            is_displayed = 0;
          }
        }
        break;
      }
    }
  }
}
void finger_print_sensor_init()
{
  Serial.println("\n\nAdafruit finger detect test");

  // set the data rate for the sensor serial port
  finger.begin(57600);
  finger.LEDcontrol(FINGERPRINT_LED_ON, 0, FINGERPRINT_LED_OFF);
  delay(5);
  if (finger.verifyPassword())
  {
    Serial.println("Found fingerprint sensor!");
  }
  else
  {
    Serial.println("Did not find fingerprint sensor :(");
    while (1)
    {
      delay(1);
    }
  }

  Serial.println(F("Reading sensor parameters"));
  finger.getParameters();
  Serial.print(F("Status: 0x"));
  Serial.println(finger.status_reg, HEX);
  Serial.print(F("Sys ID: 0x"));
  Serial.println(finger.system_id, HEX);
  Serial.print(F("Capacity: "));
  Serial.println(finger.capacity);
  Serial.print(F("Security level: "));
  Serial.println(finger.security_level);
  Serial.print(F("Device address: "));
  Serial.println(finger.device_addr, HEX);
  Serial.print(F("Packet len: "));
  Serial.println(finger.packet_len);
  Serial.print(F("Baud rate: "));
  Serial.println(finger.baud_rate);

  finger.getTemplateCount();

  if (finger.templateCount == 0)
  {
    Serial.print("Sensor doesn't contain any fingerprint data. Please run the 'enroll' example.");
  }
  else
  {
    Serial.println("Waiting for valid finger...");
    Serial.print("Sensor contains ");
    Serial.print(finger.templateCount);
    Serial.println(" templates");
  }
}
int8_t getFingerprintID()
{
  uint8_t p = finger.getImage();
  switch (p)
  {
  case FINGERPRINT_OK:
    Serial.println("Image taken");
    break;
  case FINGERPRINT_NOFINGER:
    // Serial.println("No finger detected");
    return -1;
  // case FINGERPRINT_PACKETRECIEVEERR:
  //   Serial.println("Communication error");
  //   return -1;
  // case FINGERPRINT_IMAGEFAIL:
  //   Serial.println("Imaging error");
  //   return -1;
  default:
    // Serial.println("Unknown error");
    return  -1;
  }

  // OK success!

  p = finger.image2Tz();
  switch (p)
  {
  case FINGERPRINT_OK:
    Serial.println("Image converted");
    break;
  // case FINGERPRINT_IMAGEMESS:
  //   Serial.println("Image too messy");
  //   return p;
  // case FINGERPRINT_PACKETRECIEVEERR:
  //   Serial.println("Communication error");
  //   return p;
  // case FINGERPRINT_FEATUREFAIL:
  //   Serial.println("Could not find fingerprint features");
  //   return p;
  // case FINGERPRINT_INVALIDIMAGE:
  //   Serial.println("Could not find fingerprint features");
  //   return p;
  default:
    Serial.println("Unknown error");
    return -1;
  }

  // OK converted!
  p = finger.fingerSearch();
  if (p == FINGERPRINT_OK)
  {
    Serial.println("Found a print match!");
    // } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    //   Serial.println("Communication error");
    //   return p;
  }
  else if (p == FINGERPRINT_NOTFOUND)
  {
    Serial.println("Did not find a match");
    return MAX_NUM_OF_USERS + 2;
  }
  else
  {
    Serial.println("Unknown error");
    return -1;
  }

  // found a match!
  Serial.print("Found ID #");
  Serial.print(finger.fingerID);
  Serial.print(" with confidence of ");
  Serial.println(finger.confidence);

  return finger.fingerID;
}

uint8_t deleteFingerprint(uint8_t id)
{
  uint8_t p = -1;

  p = finger.deleteModel(id);

  if (p == FINGERPRINT_OK)
  {
    Serial.println("Deleted!");
  }
  else if (p == FINGERPRINT_PACKETRECIEVEERR)
  {
    Serial.println("Communication error");
  }
  else if (p == FINGERPRINT_BADLOCATION)
  {
    Serial.println("Could not delete in that location");
  }
  else if (p == FINGERPRINT_FLASHERR)
  {
    Serial.println("Error writing to flash");
  }
  else
  {
    Serial.print("Unknown error: 0x");
    Serial.println(p, HEX);
  }

  return p;
}
void clear_screen_and_enroll_finger()
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("ENROLL FINGER :");
  lcd.setCursor(0, 1);
}
int8_t getFingerprintEnroll(int id)
{
  // deleteFingerprint(id);
  int p = -1;
  Serial.print("Waiting for valid finger to enroll as #");
  Serial.println(id);
  clear_screen_and_enroll_finger();
  while (p != FINGERPRINT_OK)
  {
    p = finger.getImage();
    switch (p)
    {
    case FINGERPRINT_OK:
      Serial.println("Image taken");
      lcd.print("IMAGE TAKEN");
      break;
    case FINGERPRINT_NOFINGER:
      //   Serial.print(".");
      break;
    default:
      Serial.println("Unknown error");
      lcd.print("UNKNOWN ERROR!");
      break;
    }
  }

  // OK success!

  p = finger.image2Tz(1);
  clear_screen_and_enroll_finger();

  switch (p)
  {
  case FINGERPRINT_OK:
    Serial.println("Image converted");
    lcd.print("IMAGE CONVERTED");
    break;
  default:
    Serial.println("Unknown error");
    lcd.print("UNKNOWN ERROR!");
    return -1;
  }
  clear_screen_and_enroll_finger();
  lcd.print("REMOVE FINGER!");
  Serial.println("Remove finger");
  delay(2000);
  p = 0;
  while (p != FINGERPRINT_NOFINGER)
  {
    p = finger.getImage();
  }
  Serial.print("ID ");
  Serial.println(id);
  p = -1;
  clear_screen_and_enroll_finger();
  lcd.print("CONFIRM FINGER!");
  Serial.println("Place same finger again");
  while (p != FINGERPRINT_OK)
  {
    p = finger.getImage();
    switch (p)
    {
    case FINGERPRINT_OK:
      clear_screen_and_enroll_finger();
      Serial.println("Image taken");
      lcd.print("IMAGE TAKEN");
      break;
    case FINGERPRINT_NOFINGER:
      //   Serial.print(".");
      break;
    default:
      clear_screen_and_enroll_finger();
      Serial.println("Unknown error");
      lcd.print("UNKNOWN ERROR!");
      break;
    }
  }

  // OK success!
  clear_screen_and_enroll_finger();
  p = finger.image2Tz(2);
  switch (p)
  {
  case FINGERPRINT_OK:
    Serial.println("Image converted");
    lcd.print("IMAGE CONVERTED!");
    break;
  default:
    Serial.println("Unknown error");
    lcd.print("UNKNOWN ERROR!");
    return -1;
  }

  // OK converted!
  Serial.print("Creating model for #");
  Serial.println(id);
  clear_screen_and_enroll_finger();
  p = finger.createModel();
  if (p == FINGERPRINT_OK)
  {
    Serial.println("Prints matched!");
    lcd.print("PRINTS MATCHED!");
  }
  else
  {
    Serial.println("Unknown error");
    lcd.print("UNKNOWN ERROR!");
    return -1;
  }

  Serial.print("ID ");
  Serial.println(id);
  clear_screen_and_enroll_finger();
  p = finger.storeModel(id);
  if (p == FINGERPRINT_OK)
  {
    Serial.println("Stored!");
    lcd.print("STORED!");
    delay(2000);
    return true;
  }
  else
  {
    Serial.println("Unknown error");
    lcd.print("UNKNOWN ERROR!");
    return -1;
  }

  return true;
}
uint8_t temp_user_id = 0;

// Helper function to parse user ID from password input
uint8_t parse_user_id_from_password(char* password, uint8_t pass_length, uint8_t* user_id_length)
{
  if (pass_length < 1) return 0;
  
  // Check if first two characters form a valid 2-digit user ID (10-28)
  if (pass_length >= 2 && password[0] >= '1' && password[0] <= '2' && password[1] >= '0' && password[1] <= '8')
  {
    uint8_t user_id = (password[0] - '0') * 10 + (password[1] - '0');
    if (user_id >= 10 && user_id <= 28)
    {
      *user_id_length = 2;
      return user_id;
    }
  }
  
  // Check if first two characters form a valid 2-digit user ID with leading zero (01-09)
  if (pass_length >= 2 && password[0] == '0' && password[1] >= '1' && password[1] <= '9')
  {
    uint8_t user_id = password[1] - '0'; // Extract the second digit as the actual user ID
    *user_id_length = 2;
    return user_id;
  }
  
  // Check if first character is a valid 1-digit user ID (1-9)
  if (password[0] >= '1' && password[0] <= '9')
  {
    uint8_t user_id = password[0] - '0';
    *user_id_length = 1;
    return user_id;
  }
  
  return 0; // Invalid user ID
}

// User ID input variables
char user_id_input[3] = {'\0'};  // Max 2 digits + null terminator
uint8_t user_id_input_length = 0;
uint8_t user_id_input_screen_type = 0;  // 1=ADD_USER, 2=REMOVE_USER, 3=ADD_FINGERPRINT

void user_id_input_fsm()
{
  if (!is_displayed)
  {
    is_displayed = 1;
    lcd.clear();
    lcd.setCursor(0, 0);
    switch (user_id_input_screen_type)
    {
      case 1:
        LCD_PRINT("CREATE USER");
        break;
      case 2:
        LCD_PRINT("REMOVE USER");
        break;
      case 3:
        LCD_PRINT("ADD FINGERPRINT");
        break;
    }
    lcd.setCursor(0, 1);
    LCD_PRINT("USER ID: ");
    user_id_input_length = 0;
    memset(user_id_input, '\0', sizeof(user_id_input));
  }
  else
  {
    if (is_new_key)
    {
      is_new_key = 0;
      if (key == CANCEL)
      {
        user_id_input_length = 0;
        memset(user_id_input, '\0', sizeof(user_id_input));
        is_displayed = 0;
        display_screen = MASTER_INPUT_STATE;
      }
      else if (key == ENTER)
      {
        if (user_id_input_length > 0)
        {
          uint8_t user_id = atoi(user_id_input);
          Serial.print("User ID entered: ");
          Serial.println(user_id);
          Serial.print("Input length: ");
          Serial.println(user_id_input_length);
          if (user_id >= 1 && user_id <= MAX_NUM_OF_USERS)
          {
            switch (user_id_input_screen_type)
            {
              case 1: // ADD_USER
                if (!check_if_password_is_configured(user_id - 1) && user_id != 1)
                {
                  temp_user_id = user_id;
                  is_displayed = 0;
                  display_screen = MASTER_ADD_USER_MOBILE_NUMBER;
                }
                else
                {
                  lcd.setCursor(0, 0);
                  LCD_PRINT("USER ALREADY");
                  lcd.setCursor(0, 1);
                  LCD_PRINT("EXISTS!!");
                  delay(2000);
                  is_displayed = 0;
                  display_screen = MASTER_INPUT_STATE;
                }
                break;
              case 2: // REMOVE_USER
                if (check_if_password_is_configured(user_id - 1) && user_id != 1)
                {
                  lcd.clear();
                  lcd.setCursor(0, 0);
                  LCD_PRINT("PLEASE WAIT...!!");
                  lcd.setCursor(0, 1);
                  LCD_PRINT("DELETING USER-");
                  lcd.print(user_id);
                  deleteFingerprint(user_id);
                  clear_password_in_eeprom(user_id - 1);
                  my_delay(3);
                  lcd.clear();
                  lcd.setCursor(0, 0);
                  LCD_PRINT("USER-");
                  lcd.print(user_id);
                  LCD_PRINT(" DELETED!!");
                  jump_to_master_main();
                  display_screen = MASTER_INPUT_STATE;
                }
                else
                {
                  lcd.clear();
                  lcd.setCursor(0, 0);
                  LCD_PRINT("USER NOT");
                  lcd.setCursor(0, 1);
                  LCD_PRINT("FOUND!!");
                  delay(2000);
                  is_displayed = 0;
                  display_screen = MASTER_INPUT_STATE;
                }
                break;
                case 3: // ADD_FINGERPRINT
                if (check_if_password_is_configured(user_id - 1))
                {
                  temp_user_id = user_id;
                  is_displayed = 0;
                  display_screen = ADD_FINGERPRINT_SCREEN;
                }
                else
                {
                  lcd.clear();
                  lcd.setCursor(0, 0);
                  LCD_PRINT("USER NOT");
                  lcd.setCursor(0, 1);
                  LCD_PRINT("CONFIGURED!!");
                  delay(2000);
                  is_displayed = 0;
                  display_screen = MASTER_INPUT_STATE;
                }
                break;
              }
            }
            else
            {
              lcd.clear();
              lcd.setCursor(0, 0);
              LCD_PRINT("INVALID USER");
              lcd.setCursor(0, 1);
              LCD_PRINT("ID (1-28)!!");
              delay(2000);
              is_displayed = 0;
              display_screen = MASTER_INPUT_STATE;
            }
          }
          else
        {
          lcd.setCursor(0, 0);
          LCD_PRINT("ENTER USER");
          lcd.setCursor(0, 1);
          LCD_PRINT("ID FIRST!!");
          delay(2000);
          is_displayed = 0;
          display_screen = MASTER_INPUT_STATE;
        }
      }
      else if (key >= '0' && key <= '9' && user_id_input_length < 2)
      {
        user_id_input[user_id_input_length] = key;
        user_id_input_length++;
        lcd.setCursor(8 + user_id_input_length - 1, 1);
        lcd.print(key);
      }
    }
  }
}

void fingerprint_register_fsm()
{
  if (getFingerprintEnroll(temp_user_id))
  {
    is_displayed = 0;
    display_screen = MASTER_INPUT_STATE;
  }
  else
  {
    is_displayed = 0;
    display_screen = FINGERPRINT_SCREEN;
  }
}


void update_queue(uint8_t message_type, uint8_t message)
{
  if (queue_index < 10)
  {
    type_list[queue_index] = message_type;
    message_details[queue_index] = message;
    queue_index++;
    Serial.print("queue updated .... ");
    Serial.println(queue_index);
  }
}

bool b_sms_sent_for_open = 0;
unsigned long applicable_buzzer_timeout = buzzer_timeout;
void lcd_task()
{
  switch (display_screen)
  {
  case MAIN:
    if (millis() - display_on_timer > display_on_timeout)
    {
      pass_length = 0;
      lcd_power_off();
      user_bio_auth_fail_count = 0; // Reset on MAIN screen timeout
      first_user_verified = 0;
      // lcd_state = LCD_STATE_OFF;
    }
    if (b_gun_point_activation_triggerd || b_temperature_alarm_triggerd || b_vibration_alarm_triggered)
    {
      input_otp_fsm();
    }
    else
    {
      password_input_fsm();
    }
    break;
  case MASTER_MAIN:
    if (!is_displayed)
    {

      if (b_error_in_door_open)
      {
        lcd.setCursor(0, 1);
        lcd.print("ERROR IN OPENING");
        is_displayed = 1;
      }
      else if (is_door_opening)
      {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("OPENING DOOR ");
        lcd.print(door_open_count);
        lcd.setCursor(11, 0);
        is_displayed = 1;
        door_open_start_time = millis();
        b_error_in_door_open = 0;
      }
      else if (is_door_open())
      {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("DOOR OPENED  ");
        lcd.print(door_open_count);
        lcd.setCursor(11, 0);
        is_displayed = 1;
        b_error_in_door_open = 0;
        door_open_time = millis();
        if (!b_sms_sent_for_open)
        {
          b_sms_sent_for_open = 1;
          update_queue(OPEN_DOOR_MSG, user_id);
          update_log_entry(door_open_count, user_id, OPEN);
        }
        display_screen = MASTER_INPUT_STATE;
        break;
      }
      // TODO: Display the number of times the door was opened for master user
    }
    if (!is_door_opening)
    {
      is_displayed = 0;
    }
    else if (!b_error_in_door_open && millis() - door_open_start_time > DOOR_OPEN_TIMEOUT)
    {
      Serial.println("ERROR IN OPENING DOOR");
      b_error_in_door_open = 1;
      is_displayed = 0;
      door_error_start_time = millis();
    }
    else if (b_error_in_door_open && millis() - door_error_start_time > DOOR_OPEN_ERROR_TIMEOUT)
    {
      is_displayed = 0;
      // b_command_close_door = 1;
      // resetting variables for openg
      b_command_open_door = 0;
      b_error_in_door_open = 0;

      b_command_close_door = 1;
      Serial.println("4443");
      b_error_in_door_close = 0;
      display_screen = LOCK_DOOR_STATE;
      break;
    }
    break;
  case MASTER_INPUT_STATE:
    applicable_buzzer_timeout = buzzer_timeout * 60000;
    // Serial.println(applicable_buzzer_timeout);
    if (millis() - door_open_time > applicable_buzzer_timeout)
    {
      Serial.println(applicable_buzzer_timeout);
      Serial.println("Buzzer ON");
      Serial.println(door_open_time);
      Serial.println(millis());
      door_open_time = millis();
      b_buzzer_on = 1;
    }
    if(!is_displayed){
      is_displayed = 1;
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("MASTER SCREEN");
    }
    if (is_new_key)
    {
      is_new_key = 0;
      Serial.println("MASTER_MAIN");
      Serial.println(key);
      switch (key)
      {
      case '1':
        is_displayed = 0;
        display_screen = MASTER_ADD_USER;
        break;
      case '2':
        is_displayed = 0;
        display_screen = MASTER_REMOVE_USER;
        break;
      case '3':
        is_displayed = 0;
        display_screen = MASTER_PASSWORD;
        break;
      case '4':
        is_displayed = 0;
        date_time_len = 0;
        memset(date_time, '\0', 13);
        Serial.println("MASTER_DAT_TIM");
        display_screen = MASTER_DAT_TIM;
        break;
      case '5':
        is_displayed = 0;
        input_mobile_number_length = 0;
        display_screen = INPUT_MOBILE_NUMBER;
        break;
      case '6':
        is_displayed = 0;
        alpha_counter = 0;
        display_screen = ALPHA_SCREEN;
        break;
      case '7':
        is_displayed = 0;
        alpha_counter = 0;
        display_screen = BACKUP_SCREEN;
        break;
      case '8':
        is_displayed = 0;
        buzzer_counter = 0;
        display_screen = BUZZER_SCREEN;
        break;
      case '9':
        is_displayed = 0;
        buzzer_counter = 0;
        display_screen = FINGERPRINT_SCREEN;
        break;
      case CANCEL:
        pass_length = 0;
        is_displayed = 0;
        display_screen = MAIN;
        break;
      case LOCK:
        // open_door();
        is_displayed = 0;
        b_command_close_door = 1;
        Serial.println("4445");
        b_error_in_door_close = 0;
        display_screen = LOCK_DOOR_STATE;
        break;
      default:
        break;
      }
    }
    break;
  case LOCK_DOOR_STATE:
    if (!is_displayed)
    {
      if (b_error_in_door_close)
      {
        lcd.clear();
        lcd.setCursor(0, 1);
        lcd.print("ERROR IN CLOSING!!");
        is_displayed = 0;
      }
      else if (!is_door_aligned_by_ir())
      {
        lcd.clear();
        lcd.setCursor(5, 0);
        lcd.print("Sensor");
        lcd.setCursor(1, 1);
        lcd.print("Not Aligned!!");
        is_displayed = 0;
        delay(1000);
        b_command_close_door = 0;
        b_command_open_door = 0;
        dc_motor_stop();
        // display_screen = MAIN;
        if (user_id == 1)
        {
          Serial.println("MASTER_MAIN");
          display_screen = MASTER_MAIN;
        }
        else
        {
          Serial.println("USER");
          display_screen = USER;
        }
        break;
      }
      else if (is_door_closing)
      {
        Serial.println("CLOSING DOOR");
        door_open_start_time = millis();
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("CLOSING DOOR ");
        lcd.print(door_open_count);
        lcd.setCursor(11, 0);
        is_displayed = 1;
        display_screen = LOCK_DOOR_STATE;
        break;
      }
      else if (is_door_close())
      {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("DOOR CLOSED  ");
        lcd.print(door_open_count);
        lcd.setCursor(11, 0);
        is_displayed = 0;
        b_error_in_door_close = 0;
        b_sms_sent_for_open = 0;
        if (user_id != 1)
        {
          update_queue(CLOSE_DOOR_MSG, MASTER_USER_ID + 1);
        }
        update_queue(CLOSE_DOOR_MSG, user_id);
        update_log_entry(door_open_count, user_id, CLOSE);
        delay(3000);
        lcd_power_off();
        // lcd_state = LCD_STATE_OFF;
        display_screen = MAIN;
        break;
      }
      // TODO: Display the number of times the door was opened for master user
    }
    if (!is_door_closing)
    {
      is_displayed = 0;
    }
    else if (millis() - door_open_start_time > 5000)
    {
      b_error_in_door_close = 1;
      is_displayed = 0;
    }
    break;
  case MASTER_ADD_USER:
    user_id_input_screen_type = 1;  // ADD_USER
    is_displayed = 0;
    display_screen = USER_ID_INPUT_SCREEN;
    break;
  case MASTER_ADD_USER_MOBILE_NUMBER:
    mobile_number_input_fsm(temp_user_id);
    break;
  case MASTER_REMOVE_USER:
    user_id_input_screen_type = 2;  // REMOVE_USER
    is_displayed = 0;
    display_screen = USER_ID_INPUT_SCREEN;
    break;

  case MASTER_PASSWORD:
    password_input_fsm();
    break;

  case MASTER_DAT_TIM:
    date_time_input_fsm();
    break;

  case MASTER_BACKUP:
    break;

  case ALPHA_SCREEN:
    alpha_input_fsm();
    break;

  case BACKUP_SCREEN:
    backup_screen_fsm();
    break;

  case BUZZER_SCREEN:
    buzzer_input_fsm();
    break;

  case FINGERPRINT_SCREEN:
    user_id_input_screen_type = 3;  // ADD_FINGERPRINT
    is_displayed = 0;
    display_screen = USER_ID_INPUT_SCREEN;
    break;
  case ADD_FINGERPRINT_SCREEN:
    fingerprint_register_fsm();
    break;

  case USER_ID_INPUT_SCREEN:
    user_id_input_fsm();
    break;

  case INPUT_MOBILE_NUMBER:
    mobile_number_input_fsm(user_id);
    break;

  case USER:
    if (!is_displayed)
    {
      if (b_error_in_door_open)
      {
        lcd.setCursor(0, 1);
        lcd.print("ERROR IN OPENING");
        is_displayed = 1;
      }
      else if (is_door_opening)
      {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("OPENING DOOR..");
        lcd.print(door_open_count);
        lcd.setCursor(11, 0);
        is_displayed = 1;
        door_open_start_time = millis();
        b_error_in_door_open = 0;
      }
      else if (is_door_open())
      {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("DOOR OPENED  ");
        lcd.print(door_open_count);
        lcd.setCursor(11, 0);
        is_displayed = 1;
        b_error_in_door_open = 0;
        if (!b_sms_sent_for_open)
        {
          b_sms_sent_for_open = 1;
          update_queue(OPEN_DOOR_MSG, MASTER_USER_ID + 1);
          update_queue(OPEN_DOOR_MSG, user_id);
        }
        display_screen = USER_INPUT_STATE;
        break;
      }
      // TODO: Display the number of times the door was opened for master user
    }
    if (!is_door_opening)
    {
      is_displayed = 0;
    }
    else if (!b_error_in_door_open && millis() - door_open_start_time > DOOR_OPEN_TIMEOUT)
    {
      Serial.println("ERROR IN OPENING DOOR");
      b_error_in_door_open = 1;
      is_displayed = 0;
      door_error_start_time = millis();
    }
    else if (b_error_in_door_open && millis() - door_error_start_time > DOOR_OPEN_ERROR_TIMEOUT)
    {
      is_displayed = 0;
      // b_command_close_door = 1;
      // resetting variables for openg
      b_command_open_door = 0;
      b_error_in_door_open = 0;

      b_command_close_door = 1;
      Serial.println("4446");
      b_error_in_door_close = 0;
      display_screen = LOCK_DOOR_STATE;
      break;
    }
    break;
  case USER_LOCK_DOOR_STATE:
    if (!is_displayed)
    {
      if (b_error_in_door_close)
      {
        lcd.setCursor(0, 1);
        lcd.print("ERROR IN CLOSING!!");
        is_displayed = 1;
      }
      else if (is_door_closing)
      {
        Serial.println("CLOSING DOOR");
        door_open_start_time = millis();
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("CLOSING DOOR ");
        lcd.print(door_open_count);
        lcd.setCursor(11, 0);
        is_displayed = 1;
        display_screen = LOCK_DOOR_STATE;
        break;
      }
      else if (is_door_close())
      {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("DOOR CLOSED  ");
        lcd.print(door_open_count);
        lcd.setCursor(11, 0);
        is_displayed = 0;
        b_error_in_door_close = 0;
        b_sms_sent_for_open = 0;
        update_queue(CLOSE_DOOR_MSG, MASTER_USER_ID + 1);
        update_queue(CLOSE_DOOR_MSG, user_id);
        update_log_entry(door_open_count, user_id, CLOSE);
        delay(3000);
        lcd_power_off();
        // lcd_state = LCD_STATE_OFF;
        display_screen = MAIN;
        break;
      }
      // TODO: Display the number of times the door was opened for master user
    }
    if (!is_door_closing)
    {
      is_displayed = 0;
    }
    else if (millis() - door_open_start_time > 5000)
    {
      b_error_in_door_close = 1;
      is_displayed = 0;
    }
    break;
  case USER_INPUT_STATE:
    if (!is_displayed)
    {
      is_displayed = 1;
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("OPENING DOOR ");
      lcd.print(door_open_count);
      lcd.setCursor(11, 0);
      b_command_open_door = 1;
      // TODO: Display the number of times the door was opened for master user
    }
    else if (is_new_key)
    {
      is_new_key = 0;
      Serial.println("USER_MAIN");
      switch (key)
      {
      case 'LOCK':
        open_door();
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("CLOSING DOOR ...");
        lcd.setCursor(11, 0);
        is_displayed = 0;
        display_screen = LOCK_DOOR_STATE;
        b_command_close_door = 1;
        Serial.println("4447");
        break;
      case '1':
        is_displayed = 0;
        display_screen = USER_PASSWORD;
        break;
      case '2':
        // is_displayed = 0;
        // input_mobile_number_length = 0;
        // input_mobile_number_count = 0;
        // display_screen = INPUT_MOBILE_NUMBER;
        // break;
      default:
        break;
      }
    }
    break;

  case USER_PASSWORD:
    password_input_fsm();
    break;
  }
}

bool is_password_valid(uint8_t _user_id, char *password, uint8_t pass_len)
{
  if (_user_id < 1 || _user_id > MAX_NUM_OF_USERS) return 0;
  
  uint8_t user_index = _user_id - 1; // Convert to 0-based index
  if (pass_len != password_length[user_index])
  {
    return 0;
  }
  bool status = is_password_matched(user_index, password, pass_len);
  Serial.print("user_id: ");
  Serial.println(_user_id);
  Serial.print("password: ");
  Serial.println(password);
  Serial.print("pass_len: ");
  Serial.println(pass_len);
  return status;
}
/*************** LCD CODE [END] ******************/
/*************** GSM CODE [START] ****************/

char msg;
char call;

String a, b;
uint8_t i = 0;

char char_array[100];  // Reduced from 200 to save 100 bytes RAM
// uint8_t received_mobile_number[10];
char received_mobile_number_in_char[11];
// char received_mnic[10];  // Removed unused buffer to save 10 bytes RAM
int8_t received_mobile_number_index1 = -1;

#define MIN_CMD_LEN 3
#define MAX_CMD_LEN 20
#define MAX_PARA_LEN 20  // Reduced from 24 to save 20 bytes RAM (para array: 5*20=100 vs 5*24=120)
#define CMD_SEPARATOR ','

#define MSG_START_CHAR '&'
#define MSG_END_CHAR '#'

#define MAX_PARAMETER 5
char cmd[20] = {'\0'};
char para[MAX_PARAMETER][MAX_PARA_LEN];
uint8_t para_len[MAX_PARAMETER];
uint8_t para_count = 0;
uint8_t cmd_length = 0;

/* api function pointer array variables [START] */
typedef char *(*functionPtr)();
functionPtr func_list[10] = {};

#define MAX_API 10
uint8_t total_api = 0;
static char api_list[MAX_API + 1][20] = {"{\"status\":\"failure\",\"data\":\"ANF or WSP \"}"};
/* api function pointer array variables [END] */
void gsm_module_init()
{

  // Serial.println("GSM SIM7600 BEGIN");
  // Serial.println("Enter character for control option:");
  // Serial.println("a : Send Message ");
  // Serial.println("b : Make a Call ");
  // Serial.println("c : Hang Up Call ");
  // Serial.println("d : RedialCall");
  // Serial.println("e : Receive Call ");
  // Serial.println("f : Receive Message ");
  // Serial.println("g : Reset Module ");
  // Serial.println();
  ResetModule();
  delay(5000);
  gsm_init();
  add_all_api();
  Serial.println("FULL GSM CODE ---------->>>>");
  ReceiveMessage();
  //  process_string(temp);
  delay(100);
}

// String otp = "024545";
uint32_t call_start_time = millis();
uint32_t call_timeout = 20000;
void gsm_housekeeping_task()
{
  if (queue_index > 0)
  {
    // Serial.print("queue index -- ");
    // Serial.println(queue_index);
    // Serial.println(type_list[queue_index - 1]);
    // Serial.println(message_details[queue_index - 1]);
    // Serial.println(":::::::::::::");
    switch (type_list[queue_index - 1])
    {
    case OPEN_DOOR_MSG:
      // SendMessageDoorStatus(MASTER_USER_ID, message_details[queue_index - 1], 1);
      // if ((message_details[queue_index - 1] - 1) != 0)
      Serial.print("message details -->");
      Serial.print(message_details[queue_index - 1]);
      Serial.print(" | ");
      Serial.println((message_details[queue_index - 1] - 1));
      if ((message_details[queue_index - 1] - 1) == 0)
      {
        SendMessageDoorStatus(MASTER_USER_ID, user_id, 1);
      }
      else if ((message_details[queue_index - 1] - 1) >= MAX_NUM_OF_USERS &&
               (message_details[queue_index - 1] - 1) != RECEIVED_MOBILE_NUMBER_INDEX)
      {
        SendMessageDoorStatus(MASTER_USER_ID, user_id, 1);
      }
      else
      {
        SendMessageDoorStatus((message_details[queue_index - 1] - 1), message_details[queue_index - 1], 1);
      }
      queue_index--;
      break;
    case CLOSE_DOOR_MSG:
      // SendMessageDoorStatus(MASTER_USER_ID, message_details[queue_index - 1], 0);
      // if ((message_details[queue_index - 1] - 1) != 0)
      // SendMessageDoorStatus((message_details[queue_index - 1] - 1), message_details[queue_index - 1], 0);
      if ((message_details[queue_index - 1] - 1) == 0)
      {
        SendMessageDoorStatus(MASTER_USER_ID, user_id, 0);
      }
      else if ((message_details[queue_index - 1] - 1) >= MAX_NUM_OF_USERS &&
               (message_details[queue_index - 1] - 1) != RECEIVED_MOBILE_NUMBER_INDEX)
      {
        SendMessageDoorStatus(MASTER_USER_ID, user_id, 0);
      }
      else
      {
        SendMessageDoorStatus((message_details[queue_index - 1] - 1), message_details[queue_index - 1], 0);
      }
      queue_index--;
      break;
    case GUN_POINT_MSG:
      if (millis() - call_start_time > 10000)
      {
        call_start_time = millis();
        Serial.println("Gun Point Message Sent!!");
        Serial.println(message_details[queue_index - 1]);
        // delay(1000);
        SendMessageGunPointMessage(message_details[queue_index - 1], char_generated_otp);
        queue_index--;
      }
      break;
    case VIBRATION_ALARM_MSG:
      if (millis() - call_start_time > 10000)
      {
        call_start_time = millis();
        Serial.println("Vibration Message Alert Sent!!");
        Serial.println(message_details[queue_index - 1]);
        // delay(1000);
        SendMessageVibrationAlarmMessage(message_details[queue_index - 1], char_generated_otp);
        queue_index--;
      }
      break;
    case TEMP_ALARM_MSG:
      if (millis() - call_start_time > 10000)
      {
        call_start_time = millis();
        Serial.println("Temperature Message Alert Sent!!");
        Serial.println(message_details[queue_index - 1]);
        // delay(1000);
        SendMessageTempAlarmMessage(message_details[queue_index - 1], char_generated_otp);
        queue_index--;
      }
      break;
    case GUN_POINT_CALL:
      if (millis() - call_start_time > call_timeout)
      {
        Serial.println("Gun Point Call Sent!!");
        Serial.println(message_details[queue_index - 1]);
        // delay(1000);
        call_start_time = millis();
        MakeCallWithNumber(message_details[queue_index - 1]);
        // SendMessageGunPointMessage(str_mobile_number[message_details[queue_index - 1]], otp);
        queue_index--;
        break;
      }
      break;
    case AUTH_FAIL_MSG:
      if (millis() - call_start_time > 10000)
      {
        call_start_time = millis();
        Serial.println("Auth Fail Message Sent!!");
        Serial.println(message_details[queue_index - 1]);
        // delay(1000);
        SendMessageAuthFail(message_details[queue_index - 1]);
        queue_index--;
      }
      break;
    default:
      queue_index--;
      break;
    }
  }
}

void gsm_module_task()
{
  if (Serial.available() > 0)
  {
    a = Serial.readString();
    Serial.println(a);
    process_string(a);
  } /*
   if (Serial.available() > 0)
     switch (Serial.read())
     {
     case 'a':
       SendMessage();
       break;
     case 'b':
       MakeCall();
       break;
     case 'c':
       HangupCall();
       break;
     case 'd':
       RedialCall();
       break;
     case 'e':
       ReceiveCall();
       break;
     case 'f':
       ReceiveMessage();
       break;
     case 'g':
       ResetModule();
       break;
     }
     */
  if (SIM7600.available() > 0)
  {
    a = SIM7600.readString();
    Serial.println(a);
    process_string(a);
  }
}
String _mobile_number1;
uint8_t index;
String str_to_be_parsed;
int8_t _user_id_from_mobile_number = -1;
bool find_mobile_number(String _input)
{
  // index = _input.indexOf("+91");
  int16_t index1 = _input.indexOf("+91");
  int16_t index2 = _input.indexOf("\",\"");
  Serial.println(index1);
  Serial.println(index2);

  if (index1 != -1 && index2 != -1)
  {
    if (index1 >= index2)
      return 0;
    uint8_t len = index2 - index1;
    if (len > 100)
      return 0;

    String str_to_be_parsed = _input.substring(index1 + 3, index2);
    Serial.println(str_to_be_parsed);
    memset(received_mobile_number_in_char, '\0', sizeof(received_mobile_number_in_char));
    str_to_be_parsed.toCharArray(char_array, str_to_be_parsed.length() + 1);
    // Serial.println(char_array);
    // char_array[10] = '\0';
    for (uint8_t i = 0, k = 0; i < 10; i++)
    {
      if (char_array[i] != ' ' || char_array[i] != '\0')
      {
        received_mobile_number_in_char[k] = char_array[i];
        // Serial.println(received_mobile_number_in_char[i]);
        k++;
        if (k == 10)
        {
          received_mobile_number_in_char[k] = '\0';
          break;
        }
      }
    }
    // Serial.println(String(received_mobile_number_in_char));
    // received_mnic[0]='\0';
    // for(uint8_t i=0;i<10;i++){
    //   received_mnic[i] = received_mobile_number_in_char[i];
    //   received_mnic[i + 1]='\0';
    // }
    Serial.println((received_mobile_number_in_char));
    // update_password_from_eeprom(0);
    // bool b_mobile_number_not_found = 0;
    // int8_t _user_id_from_mobile_number = -1;
    // for(uint8_t i=0;i<MAX_NUM_OF_USERS;i++){
    //   if(is_password_configured[i]){
    //     b_mobile_number_not_found = 0;
    //     for(uint8_t j=0;j<10;j++){
    //       if(received_mobile_number_in_char[j] != mobile_number[i][j]){
    //         b_mobile_number_not_found = 1;
    //         break;
    //       }
    //     }
    //     if(!b_mobile_number_not_found){
    //       _user_id_from_mobile_number = i;
    //       Serial.println(_user_id_from_mobile_number);
    //       break;
    //     }
    //   }
    // }

    _user_id_from_mobile_number = find_mobile_number_index_from_eeprom(received_mobile_number_in_char);
    Serial.println(_user_id_from_mobile_number);
    return true;
  }
  else
  {
    return false;
  }
  return;

  /*
    Serial.print("index :");
    Serial.println(index);
    // Serial.println(_input);
    if (!index)
    {
      return false;
    }
    _mobile_number1 = "";
    _mobile_number1 = _input.substring(index+3,index+3+20);
    Serial.println(_mobile_number1);

    memset(received_mobile_number_in_char,'\0',sizeof(received_mobile_number_in_char));
    memset(char_array,'\0',sizeof(char_array));

    _mobile_number1.toCharArray(char_array,_mobile_number1.length());
    Serial.println(char_array);

    for(uint8_t i=0;i<_mobile_number1.length();i++){
      if(char_array[i] == "\"" ){
        memset(received_mobile_number_in_char, '\0', sizeof(received_mobile_number_in_char));
        for(uint8_t j=0,k=0;j<i;j++){
          if (char_array[j] != ' '){
            received_mobile_number_in_char[k] = char_array[j];
            k++;
            Serial.println(k);
          }
        }

        // if(j == 10){
        // }
        // received_mobile_number_index1 = find_mobile_number_index_from_eeprom(received_mobile_number_in_char);

        Serial.print(received_mobile_number_index1);
        Serial.print("mobile number received");
        Serial.println(received_mobile_number_in_char);

      }
    }
    // for (uint8_t i = 0; i < 10; i++)
    // {
      // received_mobile_number_in_char[i] = (char)char_array[i];
      // received_mobile_number[i] = (uint8_t)received_mobile_number_in_char[i] - 48;
      // Serial.print(received_mobile_number_in_char[i]);
      // DEBUG_PRINT(received_mobile_number[i]);
    // }

    // for(uint8_t i=0;i<10;i++){
    //   Serial.print(char_array11[i]);
    // }
    Serial.println(received_mobile_number_in_char);
    Serial.println("---->");
    return 1;
      // str_to_be_parsed = _input.substring(index1 + 1, index2 + 2);
      // memset(char_array, '\0', sizeof(char_array));
      // str_to_be_parsed.toCharArray(char_array, str_to_be_parsed.length() + 1);

    // _mobile_number1 = _input.substring(index + 3, 20);
    // Serial.println(_mobile_number1);
    // memset(char_array, '\0', sizeof(char_array));
    // memset(received_mobile_number_in_char,'\0',sizeof(received_mobile_number_in_char));
    // _mobile_number1.toCharArray(char_array, _mobile_number1.length()+1);
    // // Serial.print("char array --> ");
    // // Serial.println(String(char_array));
    // memcpy(received_mobile_number_in_char,char_array,10);
    // for (uint8_t i = 0; i < 10; i++)
    // {
    //   // received_mobile_number_in_char[i] = (char)char_array[i];
    //   received_mobile_number[i] = (uint8_t)char_array[i] - 48;
    //   Serial.println(received_mobile_number_in_char[i])
    //   DEBUG_PRINT(received_mobile_number[i]);
    // }

    // // received_mobile_number_in_char[10]='\0';
    // Serial.print("received mobile number == ");
    // Serial.println(received_mobile_number_in_char);
    // DEBUG_PRINTLN();
    */
}
uint8_t api_unlock_door()
{
  DEBUG_PRINTLN("Unlock Door");
  print_all_received_para();
  // _user_id_from_mobile_number = find_mobile_number_index_from_eeprom(received_mobile_number_in_char);
  if (_user_id_from_mobile_number == -1)
  {
    SendMessageWithDesc(RECEIVED_MOBILE_NUMBER_INDEX, USER_NO_REGISTERED);
  }
  memset(password, '\0', sizeof(password));
  if (para_count > 1 && para_len[1] > 3)
  {
    memset(password, '\0', sizeof(password));
    copy_array(para[1], &password[0], para_len[1]);
    pass_length = para_len[1];
    if (is_password_valid((_user_id_from_mobile_number + 49), &password[0], pass_length))
    {
      if (check_if_user_is_allowed_in_time_slot(user_id))
      {
        user_id = (char)_user_id_from_mobile_number;
        user_id = user_id + 1;
        door_open_count = door_open_count + 1;
        write_door_open_count_to_eeprom(door_open_count);
        if (user_id == 1)
        {
          display_screen = MASTER_MAIN;
        }
        else
        {
          display_screen = USER;
        }
        b_command_open_door = 1;
        SendMessageWithDesc(RECEIVED_MOBILE_NUMBER_INDEX, DOOR_UNLOCK_CMD_ACCEPTED);
        return;
      }
      SendMessageWithDesc(RECEIVED_MOBILE_NUMBER_INDEX, NO_ACCESS_ALLOWED);
      return;
    }
    else
    {
      SendMessageWithDesc(RECEIVED_MOBILE_NUMBER_INDEX, PW_IS_NO_VALID);
      return;
    }
  }
  else
  {
    SendMessageWithDesc(RECEIVED_MOBILE_NUMBER_INDEX, PW_LENGH_IS_NOT_IN_LIMIT);
    return;
    // Serial.println("PW length insufficient!!");
  }
}
uint8_t api_lock_door()
{
  DEBUG_PRINTLN("Lock Door");
  print_all_received_para();
  // uint8_t _user_id_from_mobile_number = find_mobile_number_index_from_eeprom(received_mobile_number_in_char);
  if (_user_id_from_mobile_number == -1)
  {
    SendMessageWithDesc(RECEIVED_MOBILE_NUMBER_INDEX, USER_NO_REGISTERED);
    return;
  }
  else if (_user_id_from_mobile_number != 1 && _user_id_from_mobile_number != user_id)
  {
    SendMessageWithDesc(RECEIVED_MOBILE_NUMBER_INDEX, NO_ACCESS_ALLOWED);
    return;
  }
  else
  {
    if (para_count > 1 && para_len[1] > 3)
    {
      memset(password, '\0', sizeof(password));
      copy_array(para[1], &password[0], para_len[1]);
      pass_length = para_len[1];

      if (is_password_valid(_user_id_from_mobile_number + 49, &password[0], pass_length))
      {
        is_displayed = 0;
        b_command_close_door = 1;
        Serial.println("4441");
        b_error_in_door_close = 0;
        display_screen = LOCK_DOOR_STATE;
        SendMessageWithDesc(RECEIVED_MOBILE_NUMBER_INDEX, DOOR_LOCK_CMD_ACCEPTED);
        return;
      }
      SendMessageWithDesc(RECEIVED_MOBILE_NUMBER_INDEX, PW_IS_NO_VALID);
      return;
    }
    SendMessageWithDesc(RECEIVED_MOBILE_NUMBER_INDEX, PARA_MISSING);
    return;
  }
}
// String temp_str;
// String return_string_from_uint8_t(uint8_t mobile_number[])
// {
//   char _mobile_number[10];
//   for (uint8_t i = 0; i < 10; i++)
//   {
//     _mobile_number[i] = mobile_number[i] + '0';
//     printf("%c", _mobile_number[i]);
//   }
//   temp_str = String(_mobile_number);
//   return temp_str;
// }
String m;
uint8_t api_add_user()
{
  DEBUG_PRINTLN("Add User");
  print_all_received_para();

  // uint8_t _user_id_from_mobile_number = find_mobile_number_index_from_eeprom(received_mobile_number_in_char);
  if (_user_id_from_mobile_number != 0)
  {
    SendMessageWithDesc(RECEIVED_MOBILE_NUMBER_INDEX, NO_ACCESS_ALLOWED);
    return;
  }
  if (para_count > 2)
  {
    uint8_t _user_id = (uint8_t)para[0][0] - 48 - 1;
    if (is_password_configured[_user_id])
    {
      SendMessageWithDesc(RECEIVED_MOBILE_NUMBER_INDEX, USER_IS_ALREADY_REGISTERD);
    }
    else if (para_len[1] != 10)
    {
      SendMessageWithDesc(RECEIVED_MOBILE_NUMBER_INDEX, MOBILE_NUM_LEN_IS_INVALID);
    }
    else if (para_len[2] < 4 || para_len[2] > 15)
    {
      SendMessageWithDesc(RECEIVED_MOBILE_NUMBER_INDEX, PW_LENGH_IS_NOT_IN_LIMIT);
    }
    else
    {
      memset(_mobile_number, '\0', sizeof(_mobile_number));
      memset(_password, '\0', sizeof(_password));
      for (uint8_t j = 0, k = 0; j < para_len[1]; j++)
      {
        if (para[1][j] != ' ')
        {
          _mobile_number[k] = para[1][j];
          k++;
        }
      }
      for (uint8_t j = 0, k = 0; j < para_len[2]; j++)
      {
        if (para[2][j] != ' ')
        {
          _password[k] = para[2][j];
          k++;
        }
      }
      // copy_array(&para[1][0], &_mobile_number[0], para_len[1]);
      // copy_array(&para[2][0], &_password[0], para_len[2]);
      // uint8_t _user_id = (uint8_t)para[0][0] - 48-1;
      // Serial.println(_user_id);
      // Serial.println(para_len[2]);
      // Serial.println(((uint8_t)para_len[2]));
      update_eeprom_data_at_index(_user_id, _mobile_number, _password, (uint8_t)para_len[2]);
      // print_eeprom_data(port);
      SendMessageWithDesc(RECEIVED_MOBILE_NUMBER_INDEX, USER_CREATED);
      delay(100);
      SendMessageWithDesc(_user_id, USER_CREATED_ACK);
    }
  }
  else
  {
    SendMessageWithDesc(RECEIVED_MOBILE_NUMBER_INDEX, PARA_MISSING);
  }
}
void api_verify_otp()
{
  DEBUG_PRINTLN("Verify OTP");
  print_all_received_para();

  // uint8_t _user_id_from_mobile_number = find_mobile_number_index_from_eeprom(received_mobile_number_in_char);
  if (_user_id_from_mobile_number != 0)
  {
    SendMessageWithDesc(RECEIVED_MOBILE_NUMBER_INDEX, NO_ACCESS_ALLOWED);
    return;
  }
  if (para_count > 0 && para_len[0] == 6)
  {
    memset(otp, '\0', sizeof(otp));
    for (uint8_t j = 0; j < 6; j++)
    {
      otp[j] = para[0][j] - 48;
    }
    b_otp_not_matched = 0;
    for (uint8_t i = 0; i < 6; i++)
    {
      Serial.print(otp[i]);
      Serial.print(":");
      Serial.print(generated_otp[i]);
      Serial.println(">");
      if (otp[i] != generated_otp[i])
      {
        b_otp_not_matched = 1;
        // recheck_password = 1;
      }
    }

    if (b_otp_not_matched)
    {
      b_otp_not_matched = 0;
      for (uint8_t i = 0; i < 6; i++)
      {
        if (otp[i] != master_otp[i])
        {
          b_otp_not_matched = 1;
        }
      }
    }
    if (!b_otp_not_matched)
    {

      b_gun_point_activation_triggerd = 0;
      b_vibration_alarm_triggered = 0;
      b_temperature_alarm_triggerd = 0;
      b_otp_not_matched = 0;
      siren_off(siren_pin[0]);
      // siren_off(siren_pin[1]);
      display_screen = MAIN;
      otp_length = 0;
      memset(otp, '\0', sizeof(otp));
      is_displayed = 0;
      SendMessageWithDesc(RECEIVED_MOBILE_NUMBER_INDEX, OTP_MATCHED);
    }
    else
    {
      SendMessageWithDesc(RECEIVED_MOBILE_NUMBER_INDEX, OTP_NOT_MATCHED);
    }
  }
  else
  {
    SendMessageWithDesc(RECEIVED_MOBILE_NUMBER_INDEX, PARA_MISSING);
  }
}

uint8_t api_remove_user()
{
  DEBUG_PRINTLN("Remove User");
  print_all_received_para();
  DEBUG_PRINTLN("Remove User");
  uint8_t _user_id;
  // int8_t _user_id_from_mobile_number = find_mobile_number_index_from_eeprom(received_mobile_number_in_char);
  // Serial.println(_user_id_from_mobile_number);
  // // print_eeprom_data(port);
  if (_user_id_from_mobile_number != 0)
  {
    SendMessageWithDesc(RECEIVED_MOBILE_NUMBER_INDEX, NO_ACCESS_ALLOWED);
    return;
  }
  // print_all_received_para();
  if (para_count > 0)
  {
    uint8_t _user_id = (uint8_t)para[0][0] - 48 - 1;
    // print_all_received_para();
    // Serial.println(_user_id);
    if (_user_id < 5 && _user_id > 0)
    {

      clear_password_in_eeprom(_user_id);
      print_eeprom_data(port);
      SendMessageWithDesc(RECEIVED_MOBILE_NUMBER_INDEX, USER_REMOVED);
      return;
    }
    else
    {
      SendMessageWithDesc(RECEIVED_MOBILE_NUMBER_INDEX, USER_NO_REGISTERED);
      return;
    }
  }
  SendMessageWithDesc(RECEIVED_MOBILE_NUMBER_INDEX, PARA_MISSING);
  return 0;
}

uint8_t api_factory_reset()
{
  // uint8_t _user_id_from_mobile_number = find_mobile_number_index_from_eeprom(received_mobile_number_in_char);
  if (_user_id_from_mobile_number != 0)
  {
    SendMessageWithDesc(RECEIVED_MOBILE_NUMBER_INDEX, NO_ACCESS_ALLOWED);
    return;
  }
  if (para_count >= 1)
  {
    copy_array(&para[0][0], &password[0], para_len[0]);
    pass_length = para_len[0];
    if (verify_password())
    {
      SendMessageWithDesc(RECEIVED_MOBILE_NUMBER_INDEX, MASTER_RESET_DONE);
      return;
    }
    else
    {
      SendMessageWithDesc(RECEIVED_MOBILE_NUMBER_INDEX, PW_IS_NO_VALID);
      return;
    }
  }
  else
  {
    SendMessageWithDesc(RECEIVED_MOBILE_NUMBER_INDEX, PARA_MISSING);
    return;
  }
}

uint8_t api_change_password()
{
  DEBUG_PRINTLN("Add User");
  print_all_received_para();
  if (para_count > 2)
  {
    uint8_t _user_id = (uint8_t)para[0][0] - 48 - 1;
    // uint8_t _user_id_from_mobile_number = find_mobile_number_index_from_eeprom(received_mobile_number_in_char);
    if (_user_id_from_mobile_number != 0 || _user_id != _user_id_from_mobile_number)
    {
      SendMessageWithDesc(RECEIVED_MOBILE_NUMBER_INDEX, NO_ACCESS_ALLOWED);
      return;
    }
    if (_user_id < 5 && !is_password_configured[_user_id])
    {
      SendMessageWithDesc(RECEIVED_MOBILE_NUMBER_INDEX, USER_NO_REGISTERED);
    }
    else if (para_len[1] < 4 || para_len[1] > 15)
    {
      SendMessageWithDesc(RECEIVED_MOBILE_NUMBER_INDEX, PW_LENGH_IS_NOT_IN_LIMIT);
    }
    else if (para_len[2] < 4 || para_len[2] > 15)
    {
      SendMessageWithDesc(RECEIVED_MOBILE_NUMBER_INDEX, PW_LENGH_IS_NOT_IN_LIMIT);
    }
    else
    {
      memset(_password1, '\0', sizeof(_password1));
      memset(_password, '\0', sizeof(_password));
      for (uint8_t j = 0, k = 0; j < para_len[1]; j++)
      {
        if (para[1][j] != ' ')
        {
          _password[k] = para[1][j];
          k++;
        }
      }
      for (uint8_t j = 0, k = 0; j < para_len[2]; j++)
      {
        if (para[2][j] != ' ')
        {
          _password1[k] = para[2][j];
          k++;
        }
      }
      if (is_password_valid(_user_id + 49, _password, para_len[1]))
      {
        password_length[_user_id] = para_len[2];
        for (uint8_t i = 0; i < para_len[2]; i++)
        {
          password_value[_user_id][i] = _password1[i];
        }
        print_eeprom_data(port);
        update_password_to_eeprom(_user_id);
        print_eeprom_data(port);
        SendMessageWithDesc(RECEIVED_MOBILE_NUMBER_INDEX, PW_CHANGED);
        return;
      }
      else
      {
        SendMessageWithDesc(RECEIVED_MOBILE_NUMBER_INDEX, PW_IS_NO_VALID);
      }
    }
  }
  else
  {
    SendMessageWithDesc(RECEIVED_MOBILE_NUMBER_INDEX, PARA_MISSING);
  }
}

void api_lost_password()
{
  DEBUG_PRINTLN("Lost Password");
  print_all_received_para();
  int8_t index = _user_id_from_mobile_number; // find_mobile_number_index_from_eeprom(received_mobile_number_in_char);
  if (index != -1)
  {
    if (index == 0)
    {
      if (para_count > 0)
      {
        index = (uint8_t)para[0][0] - 48 - 1;
      }
    }
    if ((index < 0 && index > 4) || (index >= 0 && index < 5 && !is_password_configured[index]))
    {
      SendMessageWithDesc(RECEIVED_MOBILE_NUMBER_INDEX, PW_IS_NOT_CONFIGURED);
    }
    else
    {
      SendPWMessageWithDesc(RECEIVED_MOBILE_NUMBER_INDEX, index);
    }
  }
  else
  {
    SendMessageWithDesc(RECEIVED_MOBILE_NUMBER_INDEX, MOBILE_NUMBER_IS_NOT_REGISTERD);
  }
}

void api_update_time_slot()
{
  DEBUG_PRINTLN("UPDATE_TIME_SLOT");
  print_all_received_para();
  if (para_count > 4 && para_len[1] > 1 && para_len[2] > 1 && para_len[3] > 1 && para_len[4] > 1)
  {
    DEBUG_PRINTLN("Add User");
    print_all_received_para();
    uint8_t _user_id = (uint8_t)para[0][0] - 48 - 1;
    uint8_t _in_time_hour = ((uint8_t)para[1][0] - 48) * 10 + ((uint8_t)para[1][1] - 48);
    uint8_t _in_time_minute = ((uint8_t)para[2][0] - 48) * 10 + ((uint8_t)para[2][1] - 48);
    uint8_t _out_time_hour = ((uint8_t)para[3][0] - 48) * 10 + ((uint8_t)para[3][1] - 48);
    uint8_t _out_time_minute = ((uint8_t)para[4][0] - 48) * 10 + ((uint8_t)para[4][1] - 48);

    // uint8_t _user_id_from_mobile_number = find_mobile_number_index_from_eeprom(received_mobile_number_in_char);
    if (_user_id_from_mobile_number != 0)
    {
      SendMessageWithDesc(RECEIVED_MOBILE_NUMBER_INDEX, NO_ACCESS_ALLOWED);
      return;
    }
    if (_user_id < MAX_NUM_OF_USERS)
    {
      if (!is_password_configured[_user_id])
      {
        SendMessageWithDesc(RECEIVED_MOBILE_NUMBER_INDEX, USER_NO_REGISTERED);
        return;
      }
      if (_in_time_hour < 24 && _in_time_minute < 60 &&
          _out_time_hour < 24 && _out_time_minute < 60)
      {
        // update_in_out_time_to_eeprom(_user_id,_in_time_hour,_in_time_minute,_out_time_hour,_out_time_minute);
        in_time_hour[_user_id] = _in_time_hour;
        in_time_minute[_user_id] = _in_time_minute;
        out_time_hour[_user_id] = _out_time_hour;
        out_time_minute[_user_id] = _out_time_minute;
        is_in_out_time_configured[_user_id] = true;
        // for(uint8_t i=0;i<MAX_USER_TO_BE_STORED;i++)
        update_password_to_eeprom(_user_id);
        SendMessageWithDesc(RECEIVED_MOBILE_NUMBER_INDEX, IN_OUT_TIME_UPDATED);
        // update_data_from_eeprom();
        // print_eeprom_data(port);
        return;
      }
    }
    SendMessageWithDesc(RECEIVED_MOBILE_NUMBER_INDEX, PARA_INVALID);
  }
  else
  {
    SendMessageWithDesc(RECEIVED_MOBILE_NUMBER_INDEX, PARA_MISSING);
  }
}

void print_all_received_para()
{
  // DEBUG_PRINT("CMD:");
  // DEBUG_PRINTLN(cmd);
  Serial.print("CMD:");
  Serial.println(cmd);
  Serial.print("PARA_COUNT:");
  Serial.println(para_count);
  for (uint8_t i = 0; i < para_count; i++)
  {
    Serial.print("Para[");
    Serial.print(i);
    Serial.print(":");
    Serial.print(para_len[i]);
    Serial.print("]:");
    // for (uint8_t j = 0; j < MAX_CMD_LEN; j++)
    // {
    //   DEBUG_PRINT(para[i][j]);
    // }
    Serial.println(para[i]);
  }
}

void print_array(char *arr, uint8_t len)
{
  for (uint8_t i = 0; i < len; i++)
    Serial.print(arr[i]);
  Serial.println("--------->");
}
int8_t find_mobile_number_index_from_eeprom(char *_input_mobile_number)
{
  // update_password_from_eeprom(0);
  bool b_mobile_number_not_found = 0;
  for (uint8_t i = 0; i < MAX_NUM_OF_USERS; i++)
  {
    if (is_password_configured[i])
    {
      b_mobile_number_not_found = 0;
      // print_array(_input_mobile_number,10);
      // print_array(mobile_number[i],10);
      // Serial.println(String(_input_mobile_number));
      // Serial.println(String(mobile_number[i]));
      for (uint8_t j = 0; j < 10; j++)
      {
        // Serial.print(String(_input_mobile_number[j]) + "-" + String(mobile_number[i][j]) + ">>");
        // Serial.printl
        if (_input_mobile_number[j] != mobile_number[i][j])
        {
          //   Serial.println(_input_mobile_number[j]);
          //   Serial.println(mobile_number[i][j]);
          //   Serial.println(j);
          b_mobile_number_not_found = 1;
          // Serial.print("mobile_number_index_not_found -");
          //   Serial.println(i);
          break;
        }
      }
      // Serial.println("----------------------");
      if (!b_mobile_number_not_found)
      {
        // Serial.print("mobile_number_index -");
        // Serial.println(i);
        return i;
      }
    }
  }
  return -1;
}

void add_all_api()
{
  add_api("UNLOCK", api_unlock_door);
  add_api("LOCK", api_lock_door);
  add_api("ADD_USER", api_add_user);
  add_api("OTP", api_verify_otp);
  add_api("REMOVE_USER", api_remove_user);
  add_api("LOSTPW", api_lost_password);
  add_api("CHANGEPW", api_change_password);
  add_api("FACT_RESET", api_factory_reset);
  add_api("TIME_SLOT", api_update_time_slot);
}
void add_api(char *api_string, void *function)
{
  func_list[total_api] = function;
  total_api = total_api + 1;
  strcpy(api_list[total_api], api_string);
}

int8_t find_cmd_index(char *str_cmd)
{
  for (uint8_t i = 0; i < total_api + 1; i++)
  {
    // DEBUG_PRINTLN(api_list[i]);
    // Serial.println(api_list[i]);
    if (!strcmp((const char *)&api_list[i], str_cmd))
    {
      return i;
    }
  }
  // DEBUG_PRINTLN(str_cmd);
  return -1;
}
#define CMD_NOT_FOUND 0
#define CMD_EXECUTED 1

bool process_request()
{
  int8_t cmd_index = find_cmd_index(&cmd[0]);
  Serial.print("cmd index ");
  Serial.println(cmd_index);
  uint8_t response = 0;
  if (cmd_index == -1)
  {
    DEBUG_PRINTLN("CMD NOT FOUND!");
    return 0;
  }
  else
  {
    // update_data_from_eeprom();
    response = func_list[cmd_index - 1]();
    switch (response)
    {
    case CMD_NOT_FOUND:
      break;
    case CMD_EXECUTED:
      break;
    }
  }
}
void copy_array(char *from_array, char *to_array, uint8_t len_to_be_copied)
{
  for (uint8_t i = 0, j = 0; i < len_to_be_copied; i++)
  {
    // removing whitespaces
    if (from_array[i] != ' ')
    {
      to_array[j] = from_array[i];
      j++;
      // DEBUG_PRINT(from_array[i]);
      Serial.print(from_array[i]);
    }
  }
  // DEBUG_PRINTLN();
  Serial.println();
}

bool parse_vars(String _input)
{
  int16_t index1 = _input.indexOf("&");
  int16_t index2 = _input.indexOf("#");
  uint8_t len = 0;
  uint8_t counter = 0;
  uint8_t prev_index = 0;
  if (index1 != -1 && index2 != -1)
  {
    if (index1 >= index2)
      return 0;
    uint8_t len = index2 - index1;
    if (len > 100)
      return 0;
    str_to_be_parsed = _input.substring(index1 + 1, index2 + 2);
    memset(char_array, '\0', sizeof(char_array));
    str_to_be_parsed.toCharArray(char_array, str_to_be_parsed.length() + 1);
    // DEBUG_PRINTLN(char_array);
    for (uint8_t i = 0; i < len; i++)
    {
      // DEBUG_PRINT(char_array[i]);
      if (char_array[i] == CMD_SEPARATOR || char_array[i] == MSG_END_CHAR)
      {
        // DEBUG_PRINTLN("Coming1");
        if (counter == 0)
        {
          if (i >= MIN_CMD_LEN && i < MAX_CMD_LEN)
          {
            memset(cmd, '\0', sizeof(cmd));
            for (uint8_t j = 0, k = 0; j < i; j++)
            {
              if (char_array[j] != ' ')
              {
                cmd[k] = char_array[j];
                k++;
              }
            }
            // copy_array(&char_array[0], &cmd[0], i);
            prev_index = i;
            para_count = 0;
            counter++;
          }
          else
          {
            DEBUG_PRINTLN("cmd not found in parse request");
            return 0;
          }
        }
        else if (i - prev_index < 2)
        {
          prev_index = i;
        }
        else if ((i - prev_index < MAX_PARA_LEN))
        {
          // DEBUG_PRINTLN("Coming");
          memset(para[para_count], '\0', sizeof(para[para_count]));
          para_len[para_count] = (i - prev_index - 1);
          for (uint8_t j = 0, k = 0; j < para_len[para_count]; j++)
          {
            if (char_array[j] != ' ')
            {
              para[para_count][k] = char_array[prev_index + 1 + j];
              k++;
            }
          }
          // copy_array(&char_array[prev_index + 1], &para[para_count][0], (i - prev_index - 1));

          prev_index = i;
          para_count++;
          counter++;
        }
      }
    }
    /* Print Para Counts and CMD */
    DEBUG_PRINT("CMD:");
    DEBUG_PRINTLN(cmd);
    for (uint8_t i = 0; i < para_count; i++)
    {
      DEBUG_PRINT("Para[");
      DEBUG_PRINT(i);
      DEBUG_PRINT("]:");
      DEBUG_PRINTLN(para[i]);
    }

    return 1;
  }
  return 0;
}
void process_string(String c)
{

  memset(char_array, '\0', sizeof(char_array));
  c.trim();
  if (c.equals("\r\n"))
  {
    DEBUG_PRINTLN("----------------------No String----------------------");
  }
  else if (c.startsWith("+CMT:"))
  {
    // DEBUG_PRINT("Received Mobile Number : ");
    // print_eeprom_data(port);
    DEBUG_PRINTLN(c);
    // find_mobile_number(c);
    if (parse_vars(c) && find_mobile_number(c))
    {
      print_all_received_para();
      // print_eeprom_data(port);
      process_request();
    }
    DEBUG_PRINTLN("----------------------SMS Received----------------------");
  }
  else if (c.startsWith("+CMS ERROR:"))
  {
    DEBUG_PRINTLN("----------------------CMS Error----------------------");
  }
  else if (c.startsWith("+CMGS:"))
  {
    DEBUG_PRINTLN("----------------------SMS Sent----------------------");
  }
  else if (c.startsWith("NO CARRIER"))
  {
    DEBUG_PRINTLN("------------------CARRIER NOT FOUND-----------------");
    SIM7600.println("AT+CREG?"); // Signal quality test, value range is 0-31 , 31 is the best
    updateSerial();
  }
  else
  {
    DEBUG_PRINTLN("----------------------CMD NOT VALIDATED----------------------");
  }
}

void print_date_time_to_gsm()
{
  SIM7600.print("Date: ");
  if (date < 10)
    SIM7600.print("0");
  SIM7600.print(date);
  SIM7600.print('/');
  if (month < 10)
    SIM7600.print("0");
  SIM7600.print(month);
  SIM7600.print('/');
  SIM7600.print(year);
  SIM7600.print("\nTime: ");
  if (minute < 10)
    SIM7600.print("0");
  SIM7600.print(hour);
  SIM7600.print(':');
  if (minute < 10)
    SIM7600.print("0");

  SIM7600.print(minute);
  SIM7600.print(':');
  if (second < 10)
    SIM7600.print("0");
  SIM7600.print(second);
  SIM7600.print(" ");
}
void SendMessageDoorStatus(uint8_t mobile_number_index, uint8_t id, bool _is_door_open)
{
  String mbn, temp;
  SIM7600.println("AT+CMGF=1"); // Sets the GSM Module in Text Mode
  // delay(1000);                                                // Delay of 1000 milli seconds or 1 second
  // SIM7600.println("AT+CMGS=\"+91" + mobile_number1 + "\"\r"); // Replace x with mobile number

  delay(1000); // Delay of 1000 milli seconds or 1 secondii
  Serial.print("input index :: ");
  Serial.println(mobile_number_index);
  // for(int i=0;i<MAX_NUM_OF_USERS;i++){
  //   // mbn = "";
  //   Serial.print(i);
  //   Serial.print("  |  ");
  //   // mbn = ;
  //   Serial.print(String(mobile_number[mobile_number_index]).substring(0, 10));
  //   // Serial.print(String(mobile_number[i]));
  //   Serial.print("  |  ");
  //   for(int j=0;j<MOBILE_NUMBER_LENGTH;j++){
  //     Serial.print(mobile_number[i][j]);
  //   }
  //   Serial.println();
  // }
  if (mobile_number_index == RECEIVED_MOBILE_NUMBER_INDEX)
  {
    mbn = String(received_mobile_number_in_char);
  }
  else
  {
    mbn = String(mobile_number[mobile_number_index]).substring(0, 10);
  }
  temp = String(mobile_number[id - 1]).substring(0, 10);
  Serial.println("AT+CMGS=\"+91" + mbn + "\"\r");
  SIM7600.println("AT+CMGS=\"+91" + mbn + "\"\r"); // Replace x with mobile number

  delay(1000);
  SIM7600.print("The BMS System Door Has Been ");
  Serial.print("The BMS System Door Has Been ");
  if (_is_door_open)
  {
    SIM7600.print("Opened");
    Serial.print("Opened");
  }
  else
  {
    SIM7600.print("Closed");
    Serial.println("Closed");
  }
  SIM7600.print(" By User-");
  SIM7600.print(id);
  SIM7600.print(" (");
  SIM7600.print(temp);
  SIM7600.print(")");
  SIM7600.print("\nAt ");
  print_date_time_to_gsm();
  SIM7600.println();
  delay(100);
  SIM7600.println((char)26); // ASCII code of CTRL+Z
  delay(1000);
  Serial.println("Message Sent!!");
  // ReceiveMessage();
}
void SendMessageVibrationAlarmMessage(uint8_t mobile_number_index, char *_otp)
{
  String mbn;
  SIM7600.println("AT+CMGF=1"); // Sets the GSM Module in Text Mode
  delay(100);                   // Delay of 1000 milli seconds or 1 second
  // SIM7600.println("AT+CMGS=\"+91" + mobile_number1 + "\"\r"); // Replace x with mobile number
  if (mobile_number_index == RECEIVED_MOBILE_NUMBER_INDEX)
  {
    mbn = String(received_mobile_number_in_char);
  }
  else
  {
    mbn = String(mobile_number[mobile_number_index]).substring(0, 10);
  }
  Serial.println(mbn);
  SIM7600.println("AT+CMGS=\"+91" + mbn + "\"\r"); // Replace x with mobile number

  delay(100);
  SIM7600.print("The BMS System Door Has Sensed High Vibration.\n");
  SIM7600.print("OTP to Deactivate the sensor for your system is: ");
  SIM7600.print(generated_otp[0]);
  SIM7600.print(generated_otp[1]);
  SIM7600.print(generated_otp[2]);
  SIM7600.print(generated_otp[3]);
  SIM7600.print(generated_otp[4]);
  SIM7600.println(generated_otp[5]);

  delay(100);
  SIM7600.println((char)26); // ASCII code of CTRL+Z
  delay(100);
  Serial.println("Vib Alarm Message Sent!!");
  // ReceiveMessage();
}
void SendMessageTempAlarmMessage(uint8_t mobile_number_index, char *_otp)
{
  String mbn;
  SIM7600.println("AT+CMGF=1"); // Sets the GSM Module in Text Mode
  delay(100);                   // Delay of 1000 milli seconds or 1 second
  // SIM7600.println("AT+CMGS=\"+91" + mobile_number1 + "\"\r"); // Replace x with mobile number
  if (mobile_number_index == RECEIVED_MOBILE_NUMBER_INDEX)
  {
    mbn = String(received_mobile_number_in_char);
  }
  else
  {
    mbn = String(mobile_number[mobile_number_index]).substring(0, 10);
    ;
  }
  Serial.println(mbn);
  SIM7600.println("AT+CMGS=\"+91" + mbn + "\"\r"); // Replace x with mobile number

  delay(100);
  SIM7600.print("The BMS System Door Has Sensed High Temperature.\n");
  SIM7600.print("OTP to Deactivate the sensor for your system is: ");
  SIM7600.print(generated_otp[0]);
  SIM7600.print(generated_otp[1]);
  SIM7600.print(generated_otp[2]);
  SIM7600.print(generated_otp[3]);
  SIM7600.print(generated_otp[4]);
  SIM7600.println(generated_otp[5]);

  delay(100);
  SIM7600.println((char)26); // ASCII code of CTRL+Z
  delay(100);
  Serial.println("Temp Alarm Message Sent!!");
  // ReceiveMessage();
}
void SendMessageGunPointMessage(uint8_t mobile_number_index, char *_otp)
{
  String mbn;
  SIM7600.println("AT+CMGF=1"); // Sets the GSM Module in Text Mode
  delay(100);                   // Delay of 1000 milli seconds or 1 second
  // SIM7600.println("AT+CMGS=\"+91" + mobile_number1 + "\"\r"); // Replace x with mobile number
  if (mobile_number_index == RECEIVED_MOBILE_NUMBER_INDEX)
  {
    mbn = String(received_mobile_number_in_char);
  }
  else
  {
    mbn = String(mobile_number[mobile_number_index]).substring(0, 10);
  }
  Serial.println(mbn);
  SIM7600.println("AT+CMGS=\"+91" + mbn + "\"\r"); // Replace x with mobile number

  delay(100);
  // SIM7600.print("The BMS System Door Has Been Forced Open.\n");
  SIM7600.print("Duress Alert Is Activated in BMS System.\n");
  SIM7600.print("OTP to Deactivate the sensor for your system is: ");
  // SIM7600.println(String(_otp));
  SIM7600.print(generated_otp[0]);
  SIM7600.print(generated_otp[1]);
  SIM7600.print(generated_otp[2]);
  SIM7600.print(generated_otp[3]);
  SIM7600.print(generated_otp[4]);
  SIM7600.println(generated_otp[5]);
  delay(100);
  SIM7600.println((char)26); // ASCII code of CTRL+Z
  delay(100);
  Serial.println("Gun Point Message Sent!!");
  // ReceiveMessage();
}
void MakeCallWithNumber(uint8_t mobile_number_index)
{
  String mbn;
  if (mobile_number_index == RECEIVED_MOBILE_NUMBER_INDEX)
  {
    mbn = String(received_mobile_number_in_char);
  }
  else
  {
    mbn = String(mobile_number[mobile_number_index]).substring(0, 10);
    ;
  }
  Serial.println(mbn);
  // SIM7600.println("AT+CMGS=\"+91" + mbn + "\"\r"); // Replace x with mobile number
  SIM7600.println("ATD+91" + mbn + ";"); // ATDxxxxxxxxxx; -- watch out here for semicolon at the end!!
  DEBUG_PRINTLN("Calling  ");            // print response over serial port
  delay(100);
}
void SendMessageAuthFail(uint8_t mobile_number_index)
{
  String mbn;
  SIM7600.println("AT+CMGF=1"); // Sets the GSM Module in Text Mode
  delay(100);                   // Delay of 1000 milli seconds or 1 second
  if (mobile_number_index == RECEIVED_MOBILE_NUMBER_INDEX)
  {
    mbn = String(received_mobile_number_in_char);
  }
  else
  {
    mbn = String(mobile_number[mobile_number_index]).substring(0, 10);
  }
  Serial.println(mbn);
  SIM7600.println("AT+CMGS=\"+91" + mbn + "\"\r"); // Replace x with mobile number

  delay(100);
  SIM7600.print("Authentication Failed!\n");
  SIM7600.print("The BMS System has detected 2 consecutive failed authentication attempts on USER PASS/BIO screen.\n");
  print_date_time_to_gsm();
  SIM7600.println();

  delay(100);
  SIM7600.println((char)26); // ASCII code of CTRL+Z
  delay(100);
  Serial.println("Auth Fail Message Sent!!");
  // ReceiveMessage();
}

void SendMessageWithDesc(uint8_t mobile_number_index, uint8_t msg_index)
{
  if (mobile_number_index > RECEIVED_MOBILE_NUMBER_INDEX)
  {
    return;
  }

  String mbn, string_to_send;

  SIM7600.println("AT+CMGF=1"); // Sets the GSM Module in Text Mode
  delay(1000);                  // Delay of 1000 milli seconds or 1 second
  if (mobile_number_index == RECEIVED_MOBILE_NUMBER_INDEX)
  {
    mbn = String(received_mobile_number_in_char);
  }
  else
  {
    mbn = String(mobile_number[mobile_number_index]).substring(0, 10);
    ;
  }
  Serial.println(mbn);
  SIM7600.println("AT+CMGS=\"+91" + mbn + "\"\r"); // Replace x with mobile number
  delay(1000);
  switch (msg_index)
  {
  case USER_NO_REGISTERED:
    string_to_send = "User is not registered!"; // The SMS text you want to send
    // Serial.println("User is not registered!");
    break;
  case PW_LENGH_IS_NOT_IN_LIMIT:
    string_to_send = "Password length is greater than 15 or less than 4"; // The SMS text you want to send
    // Serial.println("User is not registered!");
    break;
  case PW_CHANGED:
    string_to_send = "Password Changed!!"; // The SMS text you want to send
    // Serial.println("Password Changed!!");
    break;
  case PW_IS_NO_VALID:
    string_to_send = "Password is not valid!!"; // The SMS text you want to send
    // Serial.println("Password is not valid!!");
    break;
  case PARA_MISSING:
    string_to_send = "Parameters are missing!!"; // The SMS text you want to send
    // Serial.println("Parameters are missing!!");
    break;
  case USER_REMOVED:
    string_to_send = "User Removed!"; // The SMS text you want to send
    // Serial.println("User Removed!");
    break;
  case USER_CREATED:
    string_to_send = "User Created!"; // The SMS text you want to send
    // Serial.println("User Created!");
    break;
  case USER_CREATED_ACK:
    string_to_send = "You're registerd for BMS Safe Lock!"; // The SMS text you want to send
    // Serial.println("User Created!");
    break;
  case USER_IS_ALREADY_REGISTERD:
    string_to_send = "User is already registered!"; // The SMS text you want to send
    // Serial.println("User is already registered!");
    break;
  case MOBILE_NUM_LEN_IS_INVALID:
    string_to_send = "Mobile number length is less or more"; // The SMS text you want to send
    // Serial.println("Mobile number length is less or more");
    break;
  case PW_IS_NOT_CONFIGURED:
    string_to_send = "Password is not configured!"; // The SMS text you want to send
    // Serial.println("Password is not configured!");
    break;
  case MOBILE_NUMBER_IS_NOT_REGISTERD:
    string_to_send = "Mobile Number is not registered"; // The SMS text you want to send
    // Serial.println("Mobile Number is not registered");
    break;
  case MASTER_RESET_DONE:
    string_to_send = "Master Reset Done!!";
    // Serial.println("Master Reset Done!!");
    break;
  case PARA_INVALID:
    string_to_send = "Invalid Parameters!";
    break;
  case IN_OUT_TIME_UPDATED:
    string_to_send = "In Out Time Updated!";
    break;
  case NO_ACCESS_ALLOWED:
    string_to_send = "No Access Allowed!";
    break;
  case DOOR_UNLOCK_CMD_ACCEPTED:
    string_to_send = "Door Unlock Command Accepted!";
    break;
  case DOOR_LOCK_CMD_ACCEPTED:
    string_to_send = "Door Lock Command Accepted!";
    break;
  case OTP_MATCHED:
    string_to_send = "OTP Successfully Applied!";
    break;
  case OTP_NOT_MATCHED:
    string_to_send = "OTP doesn't Matched!";
    break;
  }
  Serial.println(string_to_send);
  SIM7600.println(string_to_send);
  delay(100);
  SIM7600.println((char)26); // ASCII code of CTRL+Z
  delay(1000);
  ReceiveMessage();
}
void SendPWMessageWithDesc(uint8_t mobile_number_index, uint8_t index)
{
  if (mobile_number_index > RECEIVED_MOBILE_NUMBER_INDEX)
  {
    return;
  }

  String mbn, string_to_send;
  SIM7600.println("AT+CMGF=1"); // Sets the GSM Module in Text Mode
  delay(1000);                  // Delay of 1000 milli seconds or 1 second
  if (mobile_number_index == RECEIVED_MOBILE_NUMBER_INDEX)
  {
    mbn = String(received_mobile_number_in_char);
  }
  else
  {
    mbn = String(mobile_number[mobile_number_index]).substring(0, 10);
    ;
  }
  SIM7600.println("AT+CMGS=\"+91" + mbn + "\"\r"); // Replace x with mobile number

  // SIM7600.println("AT+CMGF=1");                               // Sets the GSM Module in Text Mode
  // delay(1000);                                                // Delay of 1000 milli seconds or 1 second
  // SIM7600.println("AT+CMGS=\"+91" + mobile_number1 + "\"\r"); // Replace x with mobile number
  delay(1000);
  // string_to_send = ("PW for user (") + String(index+1) + (") is ") + String(password_value[index]);
  // SendMessageWithDesc(return_string_from_uint8_t(received_mobile_number), string_to_send);
  SIM7600.print("PW for user (");
  SIM7600.print((index + 1));
  SIM7600.print(") is ");
  SIM7600.println(String(password_value[index]));
  // SIM7600.println(string_to_send); // The SMS text you want to send
  Serial.println("Password Sent!");
  delay(100);
  SIM7600.println((char)26); // ASCII code of CTRL+Z
  delay(1000);
  ReceiveMessage();
}
void SendMessage()
{
  SIM7600.println("AT+CMGF=1");                   // Sets the GSM Module in Text Mode
  delay(1000);                                    // Delay of 1000 milli seconds or 1 second
  SIM7600.println("AT+CMGS=\"+919428811350\"\r"); // Replace x with mobile number
  delay(1000);
  SIM7600.println("This is Batman!!"); // The SMS text you want to send
  delay(100);
  SIM7600.println((char)26); // ASCII code of CTRL+Z
  delay(1000);
}

void ReceiveMessage()
{
  SIM7600.println("AT+CNMI=2,2,0,0,0"); // AT Command to recieve a live SMS
  delay(500);
  SIM7600.println("AT+CMGF=1"); // AT Command to recieve a live SMS
  delay(500);
  // delay(1000);
  // if (SIM7600.available() > 0)
  // {
  //   msg = SIM7600.read();
  //   DEBUG_PRINT(msg);
  // }
  /*
  Sample string output for receiving message
  -> +CMT: "+919428811350","","21/12/24,11:48:33+22"
  -> This is test line 1
*/
}
void gsm_init()
{
  SIM7600.println("AT"); // Once the handshake test is successful, it will back to OK
  updateSerial();
  SIM7600.println("ATE0"); // Once the handshake test is successful, it will back to OK
  updateSerial();
  SIM7600.println("AT+CREG?"); // Signal quality test, value range is 0-31 , 31 is the best
  updateSerial();
  // process_string(a);
}
void updateSerial()
{
  delay(500);
  if (SIM7600.available())
  {
    call = SIM7600.read();
    DEBUG_PRINT(call);
  }
}
void MakeCall()
{
  SIM7600.println("ATD+919428811350;"); // ATDxxxxxxxxxx; -- watch out here for semicolon at the end!!
  DEBUG_PRINTLN("Calling  ");           // print response over serial port
  delay(1000);
}

void HangupCall()
{
  SIM7600.println("ATH");
  DEBUG_PRINTLN("Hangup Call");
  delay(1000);
}

void ReceiveCall()
{
  SIM7600.println("ATA");
  delay(1000);
  {
    call = SIM7600.read();
    DEBUG_PRINT(call);
  }
}

void RedialCall()
{
  SIM7600.println("ATDL");
  DEBUG_PRINTLN("Redialing");
  delay(1000);
}

void ResetModule()
{
  SIM7600.println("AT&F");
  DEBUG_PRINTLN("Resetting Module");
  delay(1000);
  SIM7600.println("AT&F1");
  DEBUG_PRINTLN("Resetting Module");
  delay(1000);
}
/*************** GSM CODE [END] ****************/
/******** BUZZER [START] *********/

void buzzer_task()
{
  if (b_buzzer_on)
  {
    // for (int thisNote = 0; thisNote < 8; thisNote++)
    // {

    //   // to calculate the note duration, take one second divided by the note type.
    //   // e.g. quarter note = 1000 / 4, eighth note = 1000/8, etc.
    //   int noteDuration = 1000 / noteDurations[thisNote];
    //   tone(45, pgm_read_word(&melody[thisNote]), noteDuration);

    //   // to distinguish the notes, set a minimum time between them.
    //   // the note's duration + 30% seems to work well:
    //   int pauseBetweenNotes = noteDuration * 1.30;
    //   delay(pauseBetweenNotes);
    //   // stop the tone playing:
    //   noTone(45);
    // }
    if (millis() - buzzer_timer > 1000)
    {
      if (b_sub_buzzer_on)
      {
        b_sub_buzzer_on = 0;
        tone(buzzer_pin, pgm_read_word(&melody[1]), 200);
      }
      else
      {
        b_sub_buzzer_on = 1;
        noTone(buzzer_pin);
      }
    }
  }
  else
  {
    noTone(buzzer_pin);
  }
}
/******** BUZZER [END] *********/
const int LCD_GND = A11;
const int LCD_VCC = A10;
void lcd_power_on()
{
  digitalWrite(LCD_GND, 0);
  digitalWrite(LCD_VCC, 1);
  display_screen = MAIN;
  is_displayed = 0;
  lcd_init();
}
void lcd_power_off()
{
  finger.LEDcontrol(FINGERPRINT_LED_ON, 0, FINGERPRINT_LED_OFF);
  // digitalWrite(LCD_GND, 1);
  // digitalWrite(LCD_VCC, 0);
  // digitalWrite(14,LOW);
}

void setup()
{

  Serial.begin(115200);
  SIM7600.begin(115200); // Setting the baud rate of GSM Module
  finger_print_sensor_init();
  wdt_disable();
  pinMode(LCD_GND, OUTPUT);
  pinMode(LCD_VCC, OUTPUT);
  rtc_begin();
  gpio_init();
  lcd_power_on();
  lcd_init();
  // wdt_enable(WDTO_8S);
  //  digitalWrite(LCD_GND,0);
  //  digitalWrite(LCD_VCC,0);
  // rtc_begin();
  sd_init();

  // lcd_power_off();

  temp_sen_init();
  Serial.println("Started");
  init_eeprom();
  // clear_eeprom();
  // return;
  port = &Serial;
  // print_eeprom_data(port);
  Serial.println("EEPROM Write Started");
  // convert_mobile_numbers_to_string();
  // convert_mobile_numbers_to_string();
  // convert_mobile_numbers_to_string();
  // return;
  // write_buzzer_timeout_to_eeprom(15);
  // write_door_open_count_to_eeprom(10);
  // write_alpha_speed_to_eeprom(20);
  // return;
  // update_eeprom_data_at_index(4, _mobile_number, _password, 15);
  // update_eeprom_data_at_index(3, _mobile_number, _password, 15);
  // update_eeprom_data_at_index(0, _mobile_number, _password, 4);
  // update_eeprom_data_at_index(1, _mobile_number, _password, 4);
  // update_eeprom_data_at_index(4, _mobile_number, _password, 15);
  // print_eeprom_data(port);
  // update_in_out_time_to_eeprom(2,10,15,20,15);
  // update_in_out_time_to_eeprom(1,12,0,22,18);
  // update_in_out_time_to_eeprom(0,14,0,23,16);
  // print_eeprom_data(port);
  // clear_password_in_eeprom(4);
  // update_eeprom_data_at_index(0, _mobile_number, _password, 15);
  // update_data_from_eeprom();
  print_eeprom_data(port);
  // clear_password_in_eeprom(1);
  // clear_password_in_eeprom(4);
  // print_eeprom_data(port);
  // return;
  // write_door_open_count_to_eeprom(10);
  // write_door_open_count_to_eeprom(15);
  Serial.println("EEPROM Write Complete");
  init_dc_motor();
  gsm_module_init();
  init_flash_drive();
  lcd_init_screen();
  //   uint8_t generated_otp[6] = {
  //     random(0, 9),
  //     random(0, 9),
  //     random(0, 9),
  //     random(0, 9),
  //     random(0, 9),
  //     random(0, 9),
  // };
  // for (uint8_t i = 0; i < 6; i++)
  // {
  //   generated_otp[i] = random(0, random(i+1,9));
  //   char_generated_otp[i] = generated_otp[i] + '0';
  // }
  // Serial.print("Generated OTP : ");
  // Serial.println(String(char_generated_otp));
  generate_random_otp();
  flash_drive_task();
  display_on_timer = millis();
  //  copy_data_from_sd_card_to_usb_flash_drive();
  // put your setup code here, to run once:
  // wdt_enable(WDTO_8S);
  // Master user (ID 1) existence is tracked by is_password_configured[0]
}
bool test = 1;
void loop()
{
  wdt_reset();
  if (is_door_close())
    temp_task();
  // put your main code here, to run repeatedly:
  rtc_task();
  lcd_task();
  gpio_task();
  dc_motor_task();
  gsm_module_task();
  gsm_housekeeping_task();
  gun_point_activation_fsm();
  flash_drive_task();
  buzzer_task();
}
