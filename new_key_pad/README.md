# Key Pad Security System - Comprehensive Documentation

## Table of Contents
1. [System Overview](#system-overview)
2. [Hardware Components](#hardware-components)
3. [System Architecture](#system-architecture)
4. [Authentication System](#authentication-system)
5. [State Machines](#state-machines)
6. [Key Functions](#key-functions)
7. [Message Queue System](#message-queue-system)
8. [Door Control System](#door-control-system)
9. [GSM Communication](#gsm-communication)
10. [EEPROM Storage](#eeprom-storage)
11. [Configuration](#configuration)
12. [Error Handling](#error-handling)
13. [User Interface](#user-interface)
14. [User Flow Guide](#user-flow-guide)

---

## System Overview

This is an Arduino-based security system called **"BMS SAFE"** for a keypad-controlled door lock with dual authentication (master + user), fingerprint recognition, GSM communication, temperature monitoring, vibration detection, and door control capabilities.

### Key Features
- **Dual Authentication**: Master user must authenticate first, then a regular user
- **Fingerprint Recognition**: Adafruit fingerprint sensor integration (up to 28 users)
- **Password-based Access**: User ID + password authentication
- **GSM Communication**: SMS alerts and notifications via SIM7600 module
- **Door Control**: DC motor-based door opening/closing with sensor feedback
- **Temperature Monitoring**: Dallas OneWire temperature sensor with alarm threshold
- **Vibration Detection**: Vibration sensor with alarm capability
- **Gun Point Activation**: Emergency activation system with alerts
- **Time-based Access Control**: Configurable in/out time slots per user
- **Holiday Management**: Configure holidays for access denial (up to 50 holidays)
- **Battery Monitoring**: Battery percentage display and monitoring
- **EEPROM Storage**: Persistent storage for user data, passwords, and settings
- **LCD Display**: 16x2 character LCD for user interface
- **OTP System**: One-time password for alarm deactivation

---

## Hardware Components

### Microcontroller
- **Arduino-compatible board** (likely Mega or similar with multiple serial ports)

### Peripherals
1. **LCD Display** (16x2)
   - Power control pins: `LCD_GND`, `LCD_VCC`
   - Display timeout: 30 seconds (auto power-off)
   - Power-on on keypad activity
   - Shows system name "BMS SAFE" with battery percentage

2. **Fingerprint Sensor** (Adafruit)
   - Connected via `Serial3` (`mySerial`)
   - Supports up to `MAX_NUM_OF_USERS` (28) fingerprints

3. **GSM Module** (SIM7600)
   - Connected via `Serial1` (`sim7600Serial`)
   - Baud rate: 115200
   - SMS and call functionality

4. **DC Motor** (Door Control)
   - Control pins: `dc_motor_pin[2] = {2, 5}`
   - Direction: CW (Clockwise) / CCW (Counter-clockwise)
   - Electromagnetic lock control: `em_lock_control_pin = 6`

5. **Door Sensors**
   - Open sensor: `sensor_pin[0] = 48`
   - Close sensor: `sensor_pin[1] = 47`
   - IR alignment sensor: `ir_rx_pin = A0`

6. **Temperature Sensor** (Dallas OneWire)
   - Bus pin: `ONE_WIRE_BUS = 42`
   - Threshold: `TEMPERATURE_THRESHOLD = 60°C`

7. **Buzzer**
   - Pin: `buzzer_pin = 45`
   - Melody-based alerts

8. **RTC** (Real-Time Clock)
   - I2C address: `0x68`
   - uRTCLib library

9. **Storage**
   - EEPROM for configuration
   - SD Card support
   - USB Flash Drive (CH376msc)

10. **Keypad** (Adafruit Keypad)
    - Character input and navigation
    - Special function keys (ENTER, CANCEL, LOCK, etc.)

11. **Battery Monitoring**
    - Analog input for battery voltage
    - Battery percentage calculation (0-100%)
    - Displayed on LCD initialization screen

12. **Siren System**
    - Dual siren outputs: `siren_pin[0]` and `siren_pin[1]`
    - Activated on alarms (temperature, vibration, gun point)

---

## System Architecture

### Main Program Flow

```
setup()
├── Serial communication initialization
├── Fingerprint sensor initialization
├── RTC initialization
├── GPIO initialization
├── LCD initialization
├── SD card initialization
├── Temperature sensor initialization
├── EEPROM initialization
├── DC motor initialization
├── GSM module initialization
└── Flash drive initialization

loop()
├── gsm_module_task()      - Handle GSM communication
└── gsm_housekeeping_task() - GSM maintenance tasks
```

### Task Execution
The system uses a task-based architecture where different subsystems are handled in separate functions:
- `lcd_task()` - LCD display and user interface
- `dc_motor_task()` - Door motor control
- `gsm_module_task()` - GSM communication
- `fingerprint_manager_fsm()` - Fingerprint authentication FSM
- `password_input_fsm()` - Password input handling
- `buzzer_task()` - Buzzer control

---

## Authentication System

### Dual Authentication Flow

The system implements a **two-stage authentication** process:

#### Stage 1: Master Authentication
1. User enters master password or scans master fingerprint (User ID = 1)
2. System validates master credentials
3. If valid, sets `first_user_verified = 1` and transitions to "USER PASS/BIO" screen

#### Stage 2: User Authentication
1. User enters their user ID + password OR scans their fingerprint
2. System validates user credentials
3. If valid, door access is granted
4. If invalid, failure count increments

### Authentication Functions

#### `verify_dual_password()`
- Validates dual authentication (master + user)
- Handles password-based authentication
- Manages `first_user_verified` flag
- Tracks `user_bio_auth_fail_count` for failure handling

**Flow:**
```
1. Parse user ID from password
2. Check if first_user_verified OR user_id == 1 (master)
3. Validate password
4. If first_user_verified:
   - If user_id == 1: Open master menu
   - Else: Grant door access
5. If user_id == 1 (master) and not first_user_verified:
   - Set first_user_verified = 1
   - Display "USER PASS/BIO" screen
6. On failure:
   - Increment user_bio_auth_fail_count
   - If count >= 1: Send AUTH_FAIL_MSG
   - Reset to MAIN screen
```

#### `fingerprint_manager_fsm()`
- Manages fingerprint-based authentication
- State machine with 5 states:
  - `FINGERPRINT_FSM_STATE_DEFAULT` - Waiting for master fingerprint
  - `FINGERPRINT_FSM_STATE_WRONG_MASTER` - Master fingerprint mismatch
  - `FINGERPRINT_FSM_STATE_ENTER_USER` - Waiting for user fingerprint
  - `FINGERPRINT_FSM_STATE_WRONG_USER` - User fingerprint mismatch
  - `FINGERPRINT_FSM_STATE_DOOR_UNLOCKED` - Authentication successful

**State Transitions:**
```
DEFAULT → ENTER_USER (if master fingerprint matches)
DEFAULT → WRONG_MASTER (if fingerprint doesn't match or invalid)
ENTER_USER → DOOR_UNLOCKED (if user fingerprint matches)
ENTER_USER → WRONG_USER (if user fingerprint doesn't match)
WRONG_USER → ENTER_USER (retry, if fail_count < 2)
WRONG_USER → DEFAULT (if fail_count >= 2, send alert)
```

#### `getFingerprintID()`
- Captures fingerprint image
- Converts image to template
- Searches for matching fingerprint in database
- Returns fingerprint ID if found, or error code

**Return Values:**
- `-1`: No finger detected, communication error, or unknown error
- `1 to MAX_NUM_OF_USERS`: Valid fingerprint ID
- `MAX_NUM_OF_USERS + 2`: Fingerprint not found in database

### Authentication Failure Handling

#### Second Authentication Failure
When the second authentication (user password/fingerprint) fails:

**Password-based Authentication (`verify_dual_password()`):**
1. **First Failure:**
   - `user_bio_auth_fail_count++` (increments by 1)
   - If `user_bio_auth_fail_count >= 1`: 
     - `update_queue(AUTH_FAIL_MSG, MASTER_USER_ID)` is called immediately
     - Alert message sent to master user
     - System returns to MAIN screen
     - `first_user_verified` reset to 0
     - Failure count reset to 0
   - User does NOT get a retry for password failures

**Fingerprint-based Authentication (`fingerprint_manager_fsm()`):**
1. **First Failure:**
   - `user_bio_auth_fail_count = user_bio_auth_fail_count + 2` (increments by 2)
   - If `user_bio_auth_fail_count >= 2`:
     - `update_queue(AUTH_FAIL_MSG, MASTER_USER_ID)` is called
     - Alert message sent to master user
     - System returns to MAIN screen
     - `first_user_verified` reset to 0
     - Failure count reset to 0
   - If `user_bio_auth_fail_count < 2`:
     - User stays on "USER PASS/BIO" screen for retry
     - No alert sent yet

**Note:** The failure count is reset to 0 when:
- Successfully entering the USER PASS/BIO screen
- Successful authentication
- Canceling from USER PASS/BIO screen
- MAIN screen timeout

---

## State Machines

### Display Screen States

```c
MAIN (0)                    - Main password entry screen
MASTER_MAIN (1)              - Master user main menu
MASTER_INPUT_STATE (11)      - Master input screen
LOCK_DOOR_STATE (12)         - Door locking state
INPUT_MOBILE_NUMBER (13)     - Mobile number input
MASTER_ADD_USER_MOBILE_NUMBER (14) - Add user mobile number
USER_INPUT_STATE (15)        - User input screen
BUZZER_SCREEN (16)           - Buzzer configuration
FINGERPRINT_SCREEN (17)      - Fingerprint menu
ADD_FINGERPRINT_SCREEN (18)  - Add fingerprint screen
USER_ID_INPUT_SCREEN (19)    - User ID input screen
USER_LOCK_DOOR_STATE (30)    - User door locking state
MASTER_ADD_USER (2)          - Master add user menu
MASTER_REMOVE_USER (3)       - Master remove user menu
MASTER_PASSWORD (4)          - Master password menu
MASTER_DAT_TIM (5)           - Master date/time menu
MASTER_BACKUP (6)            - Master backup menu
USER (7)                     - User main screen
USER_PASSWORD (8)            - User password screen
HOLIDAY_SCREEN (9)           - Holiday management screen
BACKUP_SCREEN (10)           - Backup screen
```

### Fingerprint FSM States

```c
FINGERPRINT_FSM_STATE_DEFAULT (1)       - Initial state, waiting for master
FINGERPRINT_FSM_STATE_WRONG_MASTER (2)  - Master fingerprint failed
FINGERPRINT_FSM_STATE_ENTER_USER (3)    - Waiting for user fingerprint
FINGERPRINT_FSM_STATE_WRONG_USER (4)     - User fingerprint failed
FINGERPRINT_FSM_STATE_DOOR_UNLOCKED (5) - Authentication successful
```

### Gun Point Activation States

```c
GPA_DO_NOTHING (0)    - No action
GPA_SEND_MESSAGE (1)  - Send alert message to all users
GPA_CALL (2)          - Make emergency call
```

**Gun Point Activation Flow:**
- Triggered by gun point activation, temperature alarm, or vibration alarm
- Sends alerts to all configured users (except triggering user for gun point)
- Activates sirens
- Generates OTP for deactivation
- Turns off LCD display
- Requires OTP input to deactivate alarms

---

## Key Functions

### Authentication Functions

#### `bool verify_dual_password()`
Validates dual authentication (master + user password).
- Parses user ID from password string
- Validates password against stored credentials
- Manages `first_user_verified` flag
- Handles failure counting and alerts

#### `bool verify_password()`
Legacy single-stage password verification (for backward compatibility).

#### `int8_t getFingerprintID()`
Captures and identifies fingerprint.
- Returns fingerprint ID (1-28) if match found
- Returns -1 for errors or no finger detected
- Returns MAX_NUM_OF_USERS + 2 if fingerprint not in database

#### `void fingerprint_manager_fsm()`
Manages fingerprint authentication state machine.
- Handles master and user fingerprint verification
- Manages state transitions
- Tracks authentication failures

### Door Control Functions

#### `void check_if_door_access_is_allowed(uint8_t user_id)`
Checks if user has permission to access door.
- Validates IR sensor alignment (via `is_door_aligned_by_ir()`)
- Checks time-based access restrictions (via `check_if_user_is_allowed_in_time_slot()`)
- Increments door open count
- Writes door open count to EEPROM
- Sets `b_command_open_door = 1` to initiate door opening
- Sets appropriate display screen (MASTER_MAIN for master, USER for regular users)

#### `bool check_if_user_is_allowed_in_time_slot(uint8_t _user_id)`
Validates if current time is within user's allowed access window.
- **First checks if today is a holiday** - if so, denies access immediately (returns 0)
- Checks if time-based access is configured for user
- Compares current time (hour*100 + minute) with `in_time` and `out_time`
- Returns true if access allowed, false if denied (holiday or outside time window)

#### `void open_door()`
Initiates door opening sequence.
- Sets `b_command_open_door = 1`
- Starts DC motor in opening direction

#### `void close_door()`
Initiates door closing sequence.
- Sets `b_command_close_door = 1`
- Starts DC motor in closing direction

#### `bool is_door_open()`
Checks if door is fully open (sensor state).

#### `bool is_door_close()`
Checks if door is fully closed (sensor state).

#### `bool is_door_aligned_by_ir()`
Validates IR sensor alignment before door operations.

### User Management Functions

#### `bool is_password_valid(uint8_t user_id, char *password, uint8_t pass_len)`
Validates user password against stored password in EEPROM.

#### `bool check_if_password_is_configured(uint8_t index)`
Checks if user has a password configured.

#### `void clear_password_in_eeprom(uint8_t index)`
Removes user password from EEPROM.

#### `uint8_t parse_user_id_from_password(char *password, uint8_t pass_len, uint8_t *user_id_length)`
Extracts user ID from password string format (e.g., "1XXXX" where 1 is user ID).

### Holiday Management Functions

#### `bool is_holiday(uint8_t date, uint8_t month, uint8_t year)`
Checks if a given date is configured as a holiday.
- Searches through stored holidays array
- Returns true if date matches a configured holiday

#### `bool add_holiday(uint8_t date, uint8_t month, uint8_t year)`
Adds a new holiday to the system.
- Validates date (1-31), month (1-12), year (0-99)
- Checks for duplicates before adding
- Stores in EEPROM if successful
- Returns true on success, false if invalid or duplicate

#### `bool remove_holiday(uint8_t date, uint8_t month, uint8_t year)`
Removes a holiday from the system.
- Searches for matching holiday
- Removes from array and updates EEPROM
- Returns true if found and removed, false if not found

#### `void write_holidays_to_eeprom()`
Writes all holidays to EEPROM storage.
- Stores holiday count and all holiday data
- Called automatically after add/remove operations

#### `void read_holidays_from_eeprom()`
Loads holidays from EEPROM into RAM.
- Called during system initialization
- Validates count and loads holiday data

#### `void holiday_menu_fsm()`
Holiday management menu state machine.
- **Main Menu**: Options 1 (Add), 2 (Remove), 3 (View)
- **Add Holiday**: Sequential input of date, month, year
- **Remove Holiday**: Input date, month, year to remove
- **View Holidays**: Browse all configured holidays with navigation
  - Keys: `1`/`4` (previous), `2`/`6` (next), `3` (first), `5` (last)

### EEPROM Functions

#### `void init_eeprom()`
Initializes EEPROM and loads configuration data.

#### `void update_eeprom_data_at_index(uint8_t index, char *mobile_number, char *password, uint8_t pass_len)`
Updates user data at specified index.

#### `void update_data_from_eeprom()`
Loads all user data from EEPROM into RAM.

#### `void write_door_open_count_to_eeprom(uint16_t count)`
Stores door open count persistently.

### GSM Functions

#### `void gsm_module_init()`
Initializes SIM7600 GSM module.

#### `void gsm_module_task()`
Main GSM task handler - processes message queue and sends SMS.

#### `void SendSMS(char *number, char *message)`
Sends SMS to specified number.

#### `void MakeCall(char *number)`
Initiates call to specified number.

#### `void update_queue(uint8_t message_type, uint8_t message)`
Adds message to GSM queue for sending.
- Queue size: 10 messages
- Message types: OPEN_DOOR_MSG, CLOSE_DOOR_MSG, AUTH_FAIL_MSG, etc.

### Fingerprint Functions

#### `uint8_t deleteFingerprint(uint8_t id)`
Deletes fingerprint from sensor database.

#### `uint8_t getFingerprintEnroll(uint8_t id)`
Enrolls new fingerprint to sensor database.

---

## Message Queue System

### Message Types

```c
OPEN_DOOR_MSG (1)        - Door opened notification
CLOSE_DOOR_MSG (2)       - Door closed notification
GUN_POINT_MSG (3)        - Gun point activation alert
GUN_POINT_CALL (4)       - Gun point emergency call
TEMP_ALARM_MSG (5)       - Temperature alarm alert
VIBRATION_ALARM_MSG (6)  - Vibration alarm alert
AUTH_FAIL_MSG (7)        - Authentication failure alert
```

### Queue Structure

```c
uint8_t type_list[10];        // Message types
uint8_t message_details[10];  // User IDs or additional data
uint8_t queue_index = 0;      // Current queue position
```

### Queue Operations

#### `void update_queue(uint8_t message_type, uint8_t message)`
- Adds message to queue
- Maximum 10 messages
- Automatically processed by `gsm_module_task()`

### Message Format

Messages are sent via SMS with format:
```
MSG_START_CHAR '&' + message_content + MSG_END_CHAR '#'
```

Example messages:
- "Authentication Failed!\n"
- "Door Opened!\n"
- "Door Closed!\n"
- "Temperature Alarm!\n"
- "Vibration Alarm!\n"
- "Gun Point Activated!\n"

---

## Door Control System

### Door States

1. **Closed** - Door fully closed, sensors indicate closed position
2. **Opening** - Motor running, door moving to open position
3. **Open** - Door fully open, sensors indicate open position
4. **Closing** - Motor running, door moving to closed position

### Door Control Flow

```
User Authentication → check_if_door_access_is_allowed()
  ├── Check IR alignment
  ├── Check time-based access
  ├── Increment door_open_count
  └── Set b_command_open_door = 1

dc_motor_task() processes commands:
  ├── b_command_open_door → open_door()
  │   ├── Start motor CW
  │   ├── Monitor sensors
  │   └── Stop when open sensor triggered
  │
  └── b_command_close_door → close_door()
      ├── Start motor CCW
      ├── Monitor sensors
      └── Stop when close sensor triggered
```

### Door Timeouts

- `DOOR_OPEN_TIMEOUT`: Maximum time allowed for door opening
- `DOOR_OPEN_ERROR_TIMEOUT`: Time to wait before attempting to close on error
- Error handling: If door doesn't open/close within timeout, system attempts recovery

### Safety Features

1. **IR Alignment Check**: Door operations require IR sensor alignment
2. **Sensor Feedback**: Door state verified by sensors before proceeding
3. **Timeout Protection**: Automatic error recovery on timeout
4. **Emergency Stop**: Motor can be stopped if sensors indicate misalignment

---

## GSM Communication

### Module Configuration
- **Module**: SIM7600
- **Baud Rate**: 115200
- **Serial Port**: Serial1

### Initialization

```c
void gsm_module_init()
{
  SIM7600.begin(115200);
  // AT command initialization
  // Network registration
  // SMS mode configuration
}
```

### SMS Sending

```c
void SendSMS(char *number, char *message)
{
  SIM7600.println("AT+CMGF=1");  // Text mode
  SIM7600.println("AT+CMGS=\"" + number + "\"");
  SIM7600.println(message);
  SIM7600.println((char)26);  // Ctrl+Z to send
}
```

### Message Queue Processing

`gsm_module_task()` processes the message queue:
1. Checks if queue has messages
2. Retrieves user mobile number from EEPROM
3. Formats message based on type
4. Sends SMS via `SendSMS()`
5. Removes message from queue

### Alert Recipients

- **Master User**: Receives all alerts (index 0, User ID 1)
- **Affected User**: Receives door open/close notifications for their own actions
- **Gun Point Alerts**: Sent to all configured users except the triggering user
- **Temperature Alarms**: Sent to all configured users
- **Vibration Alarms**: Sent to all configured users

---

## EEPROM Storage

### Data Structure

User data stored per index (0 to MAX_NUM_OF_USERS-1):
- Mobile number (10-12 digits)
- Password (4-15 characters)
- In/Out time configuration
- Password configuration flag

### Storage Layout

```
User Data Block (per user):
- Password configuration flag (1 byte)
- Mobile number (MOBILE_NUMBER_LENGTH bytes)
- Password length (1 byte)
- Password start marker (1 byte)
- Password value (PASSWORD_STORE_COUNT bytes)
- In/out time configuration flag (1 byte)
- In time (IN_OUT_TIME_LEN_COUNT bytes)
- Out time (IN_OUT_TIME_LEN_COUNT bytes)
- Padding (1 byte)

Global Parameters (after user data):
- Alpha speed (ALPHA_SPEED_LEN_COUNT = 4 bytes)
- Door open count (DOOR_OPEN_COUND_LEN_COUNT = 4 bytes)
- Buzzer timeout (BUZZER_TIMEOUT_LEN_COUNT = 4 bytes)
- Holiday count (HOLIDAY_COUNT_SIZE = 1 byte)
- Holiday data (MAX_HOLIDAYS * HOLIDAY_DATA_SIZE bytes)
  - Each holiday: date (1 byte) + month (1 byte) + year (1 byte)

Index 0: Master user (User ID 1)
Index 1: User 2
Index 2: User 3
...
Index 27: User 28
```

### Key EEPROM Functions

#### `void init_eeprom()`
- Initializes EEPROM
- Loads configuration data
- Validates stored data

#### `void update_eeprom_data_at_index(uint8_t index, char *mobile_number, char *password, uint8_t pass_len)`
- Updates user mobile number
- Updates user password
- Sets configuration flags

#### `void update_in_out_time_to_eeprom(uint8_t index, uint8_t in_hour, uint8_t in_min, uint8_t out_hour, uint8_t out_min)`
- Stores time-based access restrictions

#### `void write_door_open_count_to_eeprom(uint16_t count)`
- Persists door open counter

#### `void write_buzzer_timeout_to_eeprom(uint16_t timeout)`
- Stores buzzer timeout configuration

#### `void write_holidays_to_eeprom()`
- Writes all holidays to EEPROM
- Stores holiday count and holiday data array
- Called automatically after add/remove operations

#### `void read_holidays_from_eeprom()`
- Loads holidays from EEPROM into RAM
- Validates holiday count
- Called during system initialization

---

## OTP (One-Time Password) System

### OTP Generation
The system generates a random 6-digit OTP for alarm deactivation:
- Generated when temperature, vibration, or gun point alarms trigger
- Stored in `generated_otp[6]` array
- Also available as character array `char_generated_otp[6]`
- Master OTP: `{4, 5, 5, 5, 5, 6}` (hardcoded for testing/emergency)

### OTP Input Flow
1. When alarm triggers (temperature/vibration/gun point), system displays OTP input screen
2. User must enter correct OTP to deactivate alarms
3. OTP validation in `input_otp_fsm()` function
4. On successful match: Alarms deactivated, sirens turned off, system returns to normal
5. On failure: System remains in alarm state

### OTP States
- `OTP_MATCHED (19)`: OTP successfully validated
- `OTP_NOT_MATCHED (20)`: OTP validation failed

### OTP Generation Function
```c
void generate_random_otp()
```
- Generates 6 random digits based on current time
- Uses minute and hour values as seed
- Stores in both numeric and character formats

---

## Configuration

### User Configuration

#### Adding a User
1. Master authenticates
2. Navigate to "MASTER_ADD_USER"
3. Enter user ID (2-28)
4. Enter mobile number
5. Enter password (4-15 characters)
6. Optionally add fingerprint
7. Configure in/out time slots

#### Removing a User
1. Master authenticates
2. Navigate to "MASTER_REMOVE_USER"
3. Enter user ID
4. System deletes password and fingerprint

#### Changing Password
1. User authenticates
2. Navigate to password change menu
3. Enter new password
4. System updates EEPROM

### System Configuration

#### Master Reset Password
- Default: `9925366111`
- Clears all user passwords
- Resets door open count
- Restores default master configuration

#### Time-based Access
- Configure in/out time per user
- Format: HH:MM (24-hour)
- Access denied outside time window
- **Holiday Override**: Access is denied on configured holidays regardless of time slot

#### Buzzer Configuration
- Set buzzer timeout (stored in EEPROM)
- Configure alarm behavior
- Buzzer screen accessible from master menu (option 8)

#### Holiday Management
- Configure holidays for access denial
- Stored in EEPROM (up to 50 holidays)
- Access denied on configured holidays regardless of time slot
- Features:
  - Add holidays (date, month, year)
  - Remove holidays
  - View all configured holidays
  - Holiday screen accessible from master menu (option 6)

#### Display Timeout
- Configurable LCD auto-off timeout
- Default: 30 seconds
- Power-on on keypad activity

---

## Error Handling

### Authentication Errors

1. **Invalid Password**
   - Display: "Invld Password!!"
   - Return to MAIN screen
   - Send alert on 2nd failure

2. **Fingerprint Not Found**
   - Display: "USER FINGERPRNT NOT MATCHED!"
   - Increment failure count
   - Send alert on 2nd failure

3. **Master Fingerprint Mismatch**
   - Display: "MASTER FINGERPRNT NOT MATCHED!"
   - Send immediate alert
   - Return to DEFAULT state

### Door Control Errors

1. **Sensor Not Aligned**
   - Display: "Sensor Not Aligned!!"
   - Stop motor
   - Prevent door operation

2. **Door Open Timeout**
   - Display: "ERROR IN OPENING"
   - Attempt automatic close
   - Log error

3. **Door Close Timeout**
   - Display: "ERROR IN CLOSING!!"
   - Retry close operation
   - Alert master user

### Communication Errors

1. **GSM Module Not Responding**
   - Queue messages for later
   - Retry connection
   - Log errors

2. **Fingerprint Sensor Error**
   - Return error code
   - Display error message
   - Allow retry

### Recovery Mechanisms

- **Watchdog Timer**: System reset on hang (if enabled)
- **Queue Persistence**: Messages queued if GSM unavailable
- **State Recovery**: System returns to safe state on errors
- **EEPROM Validation**: Data integrity checks on startup

---

## User Interface

### LCD Display

**16x2 Character LCD** with power management:
- Auto power-off after timeout
- Power-on on keypad activity
- Status messages and prompts

### Keypad Input

**Special Keys:**
- `ENTER (#)`: Submit/Confirm
- `CANCEL (@)`: Cancel/Back
- `LOCK (*)`: Lock door
- `POWER (!)`: Power control
- `MUTE (^)`: Mute alerts
- `ALPHA_NUM (&)`: Switch input mode

**Numeric Keys (0-9)**: Password and ID input

### Display Screens

#### Initialization Screen
```
   BMS SAFE
....WELCOME....
```
(Shows battery percentage and date/time on startup)

#### Main Screen (MAIN)
```
PASSWORD:
[Input area]
```
(Displays "PASSWORD:" prompt, user enters master password)

#### Master Main Menu (MASTER_MAIN)
```
[Menu options displayed]
1. Add User
2. Remove User
3. Password
4. Date/Time
5. Mobile Number
6. Holiday
7. Backup
8. Buzzer
9. Fingerprint
```

#### User Pass/Bio Screen
```
USER PASS/BIO :
[Waiting for input]
```
(Displayed after master authentication, waiting for user password/fingerprint)

#### Door Status
```
OPENING DOOR [count]
DOOR OPENED  [count]
CLOSING DOOR [count]
ERROR IN OPENING
ERROR IN CLOSING!!
```

#### Holiday Management Menu
```
HOLIDAY MENU
1:ADD 2:REM 3:VIEW

ADD HOLIDAY
DATE: [input]
MONTH: [input]
YEAR: [input]

REMOVE HOLIDAY
DATE: [input]
MONTH: [input]
YEAR: [input]

HOLIDAY 1/5
01/12/24

NO HOLIDAYS
CONFIGURED
```

**Navigation in View Mode:**
- `1` or `4`: Previous holiday
- `2` or `6`: Next holiday
- `3`: Jump to first holiday
- `5`: Jump to last holiday
- `@` (CANCEL): Back to main menu

#### Error Messages
```
Invld Password!!
Sensor Not Aligned!!
USER FINGERPRNT NOT MATCHED!
MASTER FINGERPRNT NOT MATCHED!
Holiday - No Access Allowed!
```

### User Feedback

- **Visual**: LCD messages
- **Audio**: Buzzer alerts
- **Remote**: SMS notifications

---

## User Flow Guide

This section provides detailed step-by-step instructions for all user interactions with the BMS SAFE security system.

### 1. Initial System Startup

**Flow:**
1. System powers on
2. LCD displays: `BMS SAFE` with battery percentage
3. System initializes all components (fingerprint sensor, GSM, RTC, etc.)
4. After initialization, displays main password entry screen

**Display:**
```
   BMS SAFE
....WELCOME....
[Battery: XX%]
```

**Next:** System transitions to MAIN screen for password entry

---

### 2. Password-Based Authentication Flow

#### 2.1 Master Authentication (Step 1)

**Purpose:** Master user must authenticate first before any door access

**Steps:**
1. **Screen:** MAIN screen displays `PASSWORD:`
2. **Input:** Enter master password (User ID 1 password)
   - Format: Password can be 4-15 characters
   - Example: `12345678`
3. **Action:** Press `ENTER (#)` key
4. **Validation:**
   - ✅ **Success:** System displays `USER PASS/BIO :` screen
   - ❌ **Failure:** System displays `Invld Password!!` for 1 second, then returns to MAIN screen

**Display (Success):**
```
USER PASS/BIO :
[Waiting for input]
```

**Display (Failure):**
```
Invld Password!!
[Returns to MAIN]
```

**Next:** Proceed to User Authentication (Step 2)

---

#### 2.2 User Authentication (Step 2)

**Purpose:** Regular user must authenticate after master authentication

**Steps:**
1. **Screen:** `USER PASS/BIO :` screen is displayed
2. **Input Options:**
   - **Option A - Password:** Enter user ID + password
     - Format: `[UserID][Password]`
     - Example: `21234` (User ID 2, Password 1234)
   - **Option B - Fingerprint:** Place finger on fingerprint sensor
3. **Action:** 
   - For password: Press `ENTER (#)` key
   - For fingerprint: Wait for sensor to read
4. **Validation:**
   - ✅ **Success:** 
     - System checks time slot and holidays
     - If allowed: Door opens, displays door status
     - If denied: Displays `Holiday - No Access Allowed!` or `No Access Allowed!`
   - ❌ **Failure:** 
     - Displays `Invld Password!!` or `USER FINGERPRNT NOT MATCHED!`
     - Sends SMS alert to master user
     - Returns to MAIN screen

**Display (Success - Door Opening):**
```
OPENING DOOR [count]
```

**Display (Success - Door Open):**
```
DOOR OPENED  [count]
```

**Display (Failure):**
```
Invld Password!!
[Returns to MAIN]
```

**Next:** 
- If successful: User can access door or navigate menus
- If failed: Must restart from Step 1

---

### 3. Fingerprint-Based Authentication Flow

#### 3.1 Master Fingerprint (Step 1)

**Purpose:** Master user authenticates using fingerprint

**Steps:**
1. **Screen:** MAIN screen (or any screen)
2. **Action:** Place master's finger (User ID 1) on fingerprint sensor
3. **Validation:**
   - ✅ **Success:** 
     - System displays `USER PASS/BIO :` screen
     - Sets `first_user_verified = 1`
   - ❌ **Failure:** 
     - Displays `MASTER FINGERPRNT NOT MATCHED!`
     - Sends SMS alert to master
     - Returns to DEFAULT state

**Display (Success):**
```
USER PASS/BIO :
[Waiting for user]
```

**Display (Failure):**
```
MASTER FINGERPRNT
NOT MATCHED!
```

**Next:** Proceed to User Fingerprint (Step 2)

---

#### 3.2 User Fingerprint (Step 2)

**Purpose:** Regular user authenticates using fingerprint

**Steps:**
1. **Screen:** `USER PASS/BIO :` screen
2. **Action:** Place user's finger on fingerprint sensor
3. **Validation:**
   - ✅ **Success:** 
     - System checks time slot and holidays
     - If allowed: Door opens
     - If denied: Displays access denied message
   - ❌ **Failure:** 
     - Displays `USER FINGERPRNT NOT MATCHED!`
     - Increments failure count
     - If 2nd failure: Sends SMS alert, returns to MAIN screen
     - If 1st failure: Allows retry on same screen

**Display (Success):**
```
OPENING DOOR [count]
```

**Display (Failure - Retry):**
```
USER FINGERPRNT
NOT MATCHED!
[Retry allowed]
```

**Display (Failure - Alert Sent):**
```
USER FINGERPRNT
NOT MATCHED!
[Returns to MAIN]
```

**Next:**
- If successful: Door access granted
- If failed (1st time): Can retry
- If failed (2nd time): Must restart from Step 1

---

### 4. Master Menu Navigation

#### 4.1 Accessing Master Menu

**Prerequisites:** Master authentication completed (password or fingerprint)

**Steps:**
1. After successful master authentication, system displays `USER PASS/BIO :` screen
2. If master enters their own password again (User ID 1), system opens Master Main Menu
3. Master Main Menu displays:

**Display:**
```
1. Add User
2. Remove User
3. Password
4. Date/Time
5. Mobile Number
6. Holiday
7. Backup
8. Buzzer
9. Fingerprint
```

**Navigation:**
- Press number key (1-9) to select option
- Press `CANCEL (@)` to return to previous screen

---

#### 4.2 Adding a New User

**Menu Path:** Master Main Menu → Press `1` (Add User)

**Steps:**
1. **Screen:** `USER ID:` prompt appears
2. **Input:** Enter user ID (2-28, cannot be 1)
   - Example: `2` for User 2
3. **Action:** Press `ENTER (#)`
4. **Validation:**
   - ✅ **Valid:** Proceeds to mobile number input
   - ❌ **Invalid:** Displays `USER ALREADY EXISTS!!` if user already configured
5. **Mobile Number Input:**
   - Screen: `MOBILE NUMBER:`
   - Enter 10-digit mobile number
   - Press `ENTER (#)` to confirm
6. **Password Input:**
   - Screen: `PASSWORD:`
   - Enter password (4-15 characters)
   - Press `ENTER (#)` to confirm
7. **Confirmation:**
   - System saves user data to EEPROM
   - Displays success message
   - Returns to Master Main Menu

**Display (User ID Input):**
```
USER ID:
[Input area]
```

**Display (Mobile Number Input):**
```
MOBILE NUMBER:
[Input area]
```

**Display (Password Input):**
```
PASSWORD:
[Input area]
```

**Display (Success):**
```
USER-[ID] ADDED!!
[Returns to menu]
```

**Display (Error):**
```
USER ALREADY
EXISTS!!
```

**Optional:** After adding user, can add fingerprint (see Section 4.9)

---

#### 4.3 Removing a User

**Menu Path:** Master Main Menu → Press `2` (Remove User)

**Steps:**
1. **Screen:** `USER ID:` prompt appears
2. **Input:** Enter user ID to remove (2-28, cannot remove User 1)
   - Example: `2` for User 2
3. **Action:** Press `ENTER (#)`
4. **Validation:**
   - ✅ **Valid:** System deletes user
   - ❌ **Invalid:** Displays `USER NOT FOUND!!` if user doesn't exist
5. **Deletion Process:**
   - Displays `PLEASE WAIT...!!` and `DELETING USER-[ID]`
   - Deletes fingerprint from sensor
   - Clears password from EEPROM
   - Displays `USER-[ID] DELETED!!`
   - Returns to Master Main Menu

**Display (User ID Input):**
```
USER ID:
[Input area]
```

**Display (Deleting):**
```
PLEASE WAIT...!!
DELETING USER-2
```

**Display (Success):**
```
USER-2 DELETED!!
[Returns to menu]
```

**Display (Error):**
```
USER NOT
FOUND!!
```

---

#### 4.4 Changing Master Password

**Menu Path:** Master Main Menu → Press `3` (Password)

**Steps:**
1. **Screen:** Password change menu appears
2. **Input:** Enter new password (4-15 characters)
3. **Action:** Press `ENTER (#)` to confirm
4. **Confirmation:** System updates password in EEPROM
5. **Result:** Returns to Master Main Menu

**Note:** This changes the master user's (User ID 1) password

---

#### 4.5 Setting Date/Time

**Menu Path:** Master Main Menu → Press `4` (Date/Time)

**Steps:**
1. **Screen:** Date/Time input menu appears
2. **Date Input:**
   - Screen: `DATE:`
   - Enter date (1-31)
   - Press `ENTER (#)`
3. **Month Input:**
   - Screen: `MONTH:`
   - Enter month (1-12)
   - Press `ENTER (#)`
4. **Year Input:**
   - Screen: `YEAR:`
   - Enter year (2 digits, e.g., 24 for 2024)
   - Press `ENTER (#)`
5. **Hour Input:**
   - Screen: `HOUR:`
   - Enter hour (0-23, 24-hour format)
   - Press `ENTER (#)`
6. **Minute Input:**
   - Screen: `MINUTE:`
   - Enter minute (0-59)
   - Press `ENTER (#)`
7. **Second Input:**
   - Screen: `SECOND:`
   - Enter second (0-59)
   - Press `ENTER (#)`
8. **Confirmation:** System updates RTC with new date/time
9. **Result:** Returns to Master Main Menu

**Display (Date Input):**
```
DATE:
[Input area]
```

**Navigation:**
- Use `CANCEL (@)` to cancel and return to menu
- Each field must be entered sequentially

---

#### 4.6 Updating Mobile Number

**Menu Path:** Master Main Menu → Press `5` (Mobile Number)

**Steps:**
1. **Screen:** `USER ID:` prompt appears
2. **Input:** Enter user ID whose mobile number to update
3. **Action:** Press `ENTER (#)`
4. **Mobile Number Input:**
   - Screen: `MOBILE NUMBER:`
   - Enter new 10-digit mobile number
   - Press `ENTER (#)` to confirm
5. **Confirmation:** System updates mobile number in EEPROM
6. **Result:** Returns to Master Main Menu

---

#### 4.7 Holiday Management

**Menu Path:** Master Main Menu → Press `6` (Holiday)

**Steps:**
1. **Screen:** Holiday Management Main Menu appears

**Display:**
```
HOLIDAY MENU
1:ADD 2:REM 3:VIEW
```

**Options:**
- Press `1`: Add Holiday
- Press `2`: Remove Holiday
- Press `3`: View Holidays
- Press `CANCEL (@)`: Return to Master Main Menu

---

##### 4.7.1 Adding a Holiday

**Menu Path:** Holiday Menu → Press `1` (Add)

**Steps:**
1. **Screen:** `ADD HOLIDAY` and `DATE:` prompt
2. **Date Input:**
   - Enter date (1-31)
   - Press `ENTER (#)` to proceed
3. **Month Input:**
   - Screen: `ADD HOLIDAY` and `MONTH:` prompt
   - Enter month (1-12)
   - Press `ENTER (#)` to proceed
4. **Year Input:**
   - Screen: `ADD HOLIDAY` and `YEAR:` prompt
   - Enter year (2 digits, 0-99)
   - Press `ENTER (#)` to confirm
5. **Validation:**
   - ✅ **Success:** Holiday added, returns to Holiday Menu
   - ❌ **Failure:** Displays error if date invalid or duplicate
6. **Result:** Returns to Holiday Menu

**Display (Date Input):**
```
ADD HOLIDAY
DATE: [input]
```

**Display (Success):**
```
HOLIDAY ADDED
[Returns to menu]
```

**Navigation:**
- Use `CANCEL (@)` at any step to cancel and return to Holiday Menu

---

##### 4.7.2 Removing a Holiday

**Menu Path:** Holiday Menu → Press `2` (Remove)

**Steps:**
1. **Screen:** `REMOVE HOLIDAY` and `DATE:` prompt
2. **Date Input:**
   - Enter date (1-31)
   - Press `ENTER (#)` to proceed
3. **Month Input:**
   - Screen: `REMOVE HOLIDAY` and `MONTH:` prompt
   - Enter month (1-12)
   - Press `ENTER (#)` to proceed
4. **Year Input:**
   - Screen: `REMOVE HOLIDAY` and `YEAR:` prompt
   - Enter year (2 digits)
   - Press `ENTER (#)` to confirm
5. **Validation:**
   - ✅ **Success:** Holiday removed, returns to Holiday Menu
   - ❌ **Failure:** Displays error if holiday not found
6. **Result:** Returns to Holiday Menu

**Display (Date Input):**
```
REMOVE HOLIDAY
DATE: [input]
```

**Display (Success):**
```
HOLIDAY REMOVED
[Returns to menu]
```

**Display (Error):**
```
HOLIDAY NOT
FOUND!!
```

---

##### 4.7.3 Viewing Holidays

**Menu Path:** Holiday Menu → Press `3` (View)

**Steps:**
1. **Screen:** Displays first holiday (if any exist)

**Display (With Holidays):**
```
HOLIDAY 1/5
01/12/24
```

**Display (No Holidays):**
```
NO HOLIDAYS
CONFIGURED
```

**Navigation:**
- Press `1` or `4`: Previous holiday
- Press `2` or `6`: Next holiday
- Press `3`: Jump to first holiday
- Press `5`: Jump to last holiday
- Press `CANCEL (@)`: Return to Holiday Menu

**Features:**
- Shows current holiday index (e.g., "1/5" means holiday 1 of 5 total)
- Displays date in DD/MM/YY format
- Automatically wraps around at boundaries

---

#### 4.8 Backup Operations

**Menu Path:** Master Main Menu → Press `7` (Backup)

**Steps:**
1. **Screen:** Backup menu appears
2. **Options:** (Implementation may vary)
   - Backup user data
   - Restore from backup
   - Export to SD card/USB
3. **Action:** Follow on-screen prompts
4. **Result:** Returns to Master Main Menu

**Note:** Backup functionality may copy data to SD card or USB flash drive

---

#### 4.9 Buzzer Configuration

**Menu Path:** Master Main Menu → Press `8` (Buzzer)

**Steps:**
1. **Screen:** Buzzer configuration menu appears
2. **Input:** Enter buzzer timeout value (in seconds or milliseconds)
3. **Action:** Press `ENTER (#)` to confirm
4. **Confirmation:** System saves buzzer timeout to EEPROM
5. **Result:** Returns to Master Main Menu

**Purpose:** Configures how long buzzer sounds for alerts

---

#### 4.10 Fingerprint Management

**Menu Path:** Master Main Menu → Press `9` (Fingerprint)

**Steps:**
1. **Screen:** `USER ID:` prompt appears
2. **Input:** Enter user ID to add fingerprint for
3. **Action:** Press `ENTER (#)`
4. **Validation:**
   - ✅ **Valid:** Proceeds to fingerprint enrollment
   - ❌ **Invalid:** Displays `USER NOT CONFIGURED!!` if user doesn't exist
5. **Fingerprint Enrollment Process:**
   - Screen: `PLACE FINGER`
   - Place finger on sensor
   - Wait for: `IMAGE TAKEN`
   - Wait for: `IMAGE CONVERTED`
   - Screen: `REMOVE FINGER!`
   - Remove finger and wait 2 seconds
   - Screen: `CONFIRM FINGER!`
   - Place same finger again
   - Wait for: `IMAGE TAKEN`
   - Wait for: `IMAGE CONVERTED`
   - System creates fingerprint template
   - Screen: `FINGERPRINT ENROLLED!`
   - Returns to Master Main Menu

**Display (Place Finger):**
```
PLACE FINGER
[Waiting...]
```

**Display (Image Taken):**
```
IMAGE TAKEN
```

**Display (Remove Finger):**
```
REMOVE FINGER!
```

**Display (Confirm Finger):**
```
CONFIRM FINGER!
```

**Display (Success):**
```
FINGERPRINT
ENROLLED!
```

**Display (Error):**
```
USER NOT
CONFIGURED!!
```

**Note:** User must have password configured before adding fingerprint

---

### 5. User Menu Navigation

#### 5.1 Accessing User Menu

**Prerequisites:** Successful dual authentication (master + user)

**Steps:**
1. After successful user authentication, system displays User Main Screen
2. User Main Screen shows door status and options

**Display:**
```
DOOR OPENED  [count]
[or]
OPENING DOOR [count]
```

**Options:**
- Press `LOCK (*)`: Close door
- Press `1`: Change password
- Press `2`: (Reserved/Unused)

---

#### 5.2 User Changing Password

**Menu Path:** User Main Screen → Press `1` (Password)

**Steps:**
1. **Screen:** `OLD PASSWORD:` prompt appears
2. **Input:** Enter current password
3. **Action:** Press `ENTER (#)`
4. **Validation:**
   - ✅ **Valid:** Proceeds to new password input
   - ❌ **Invalid:** Displays `Invld Password!!` and returns to User Main Screen
5. **New Password Input:**
   - Screen: `NEW PASSWORD:`
   - Enter new password (4-15 characters)
   - Press `ENTER (#)` to confirm
6. **Confirmation:** System updates password in EEPROM
7. **Result:** Returns to User Main Screen

**Display (Old Password):**
```
OLD PASSWORD:
[Input area]
```

**Display (New Password):**
```
NEW PASSWORD:
[Input area]
```

**Display (Success):**
```
PASSWORD CHANGED
[Returns to menu]
```

---

#### 5.3 User Closing Door

**Menu Path:** User Main Screen → Press `LOCK (*)`

**Steps:**
1. **Action:** Press `LOCK (*)` key
2. **Screen:** Displays `CLOSING DOOR ...`
3. **Process:** System initiates door closing sequence
4. **Status:** Door closes, sensors confirm closed position
5. **Display:** Shows door closed status

**Display:**
```
CLOSING DOOR ...
```

**Display (Closed):**
```
DOOR CLOSED
```

---

### 6. SMS Command Flows

#### 6.1 Two-Step UNLOCK Command

**Purpose:** Unlock door remotely via SMS with dual authentication

**Prerequisites:** 
- Sender's mobile number must be registered in system
- Both master and user passwords must be known

---

##### Step 1: Master Verification

**Command Format:**
```
&UNLOCK,01,<master_password>#
```

**Example:**
```
&UNLOCK,01,9925366111#
```

**Parameters:**
- `01`: User ID 1 (Master)
- `<master_password>`: Master user's password (4-15 characters)

**Response:**
- ✅ **Success:** SMS reply: `"Door Unlock Command Accepted!"`
- ❌ **Failure:** SMS reply: `"Password is not valid"` or `"Parameters missing"`

**System Action:**
- Sets `sms_master_verified = true`
- Starts 60-second timeout timer
- Waits for Step 2 (user verification)

**Important:** Must complete Step 2 within 60 seconds, or master verification expires

---

##### Step 2: User Verification & Unlock

**Command Format:**
```
&UNLOCK,<user_id>,<user_password>#
```

**Example:**
```
&UNLOCK,02,1234#
```

**Parameters:**
- `<user_id>`: User ID (2-28, cannot be 1)
- `<user_password>`: User's password (4-15 characters)

**Response:**
- ✅ **Success:** SMS reply: `"Door Unlock Command Accepted!"`
- ❌ **Failure:** 
  - `"No Access Allowed"` - Master not verified, wrong password, or time/holiday restriction
  - `"Password is not valid"` - User password incorrect

**System Action:**
- Verifies master was authenticated in Step 1
- Validates user password
- Checks time slot restrictions
- Checks if today is a holiday
- If all checks pass: Unlocks door, resets master verification flag
- If any check fails: Returns error, resets master verification flag

**Complete Flow Example:**
```
Step 1: Send &UNLOCK,01,9925366111#
        → Receive: "Door Unlock Command Accepted!"
        
Step 2: Send &UNLOCK,02,1234# (within 60 seconds)
        → Receive: "Door Unlock Command Accepted!"
        → Door unlocks
```

**Error Scenarios:**
- **Master password wrong:** Step 1 fails, must retry Step 1
- **User tries to unlock without Step 1:** Step 2 fails with "No Access Allowed"
- **Timeout:** If Step 2 not sent within 60 seconds, master verification expires
- **User password wrong:** Step 2 fails, master verification reset, must restart from Step 1
- **Time slot restriction:** Step 2 fails with "No Access Allowed"
- **Holiday restriction:** Step 2 fails with "No Access Allowed"

---

#### 6.2 LOCK Command

**Command Format:**
```
&LOCK,<user_id>,<password>#
```

**Example:**
```
&LOCK,02,1234#
```

**Parameters:**
- `<user_id>`: User ID (1-28)
- `<password>`: User's password (4-15 characters)

**Response:**
- ✅ **Success:** SMS reply: `"Door Lock Command Accepted!"`
- ❌ **Failure:** 
  - `"Password is not valid"` - Wrong password
  - `"No Access Allowed"` - User not registered or invalid parameters

**System Action:**
- Verifies mobile number is registered
- Validates user ID and password
- Initiates door closing sequence
- Sends confirmation SMS

---

#### 6.3 ADDUSER Command

**Command Format:**
```
&ADDUSER,<user_id>,<mobile_number>,<password>#
```

**Example:**
```
&ADDUSER,02,9876543210,1234#
```

**Parameters:**
- `<user_id>`: User ID (2-28, cannot be 1)
- `<mobile_number>`: 10-digit mobile number
- `<password>`: Password (4-15 characters)

**Response:**
- ✅ **Success:** SMS reply: `"Command Executed"`
- ❌ **Failure:** 
  - `"User already exists"` - User ID already configured
  - `"Parameters missing"` - Missing required parameters
  - `"Invalid parameters"` - Invalid user ID or password length

**System Action:**
- Validates parameters
- Checks if user already exists
- Stores user data in EEPROM
- Sends confirmation SMS

---

#### 6.4 REMOVEUSER Command

**Command Format:**
```
&REMOVEUSER,<user_id>#
```

**Example:**
```
&REMOVEUSER,02#
```

**Parameters:**
- `<user_id>`: User ID to remove (2-28, cannot remove User 1)

**Response:**
- ✅ **Success:** SMS reply: `"Command Executed"`
- ❌ **Failure:** 
  - `"User not found"` - User ID doesn't exist
  - `"Parameters missing"` - Missing user ID

**System Action:**
- Validates user ID
- Deletes fingerprint from sensor
- Removes password from EEPROM
- Sends confirmation SMS

---

#### 6.5 CHANGEPW Command

**Command Format:**
```
&CHANGEPW,<user_id>,<old_password>,<new_password>#
```

**Example:**
```
&CHANGEPW,02,1234,5678#
```

**Parameters:**
- `<user_id>`: User ID (1-28)
- `<old_password>`: Current password
- `<new_password>`: New password (4-15 characters)

**Response:**
- ✅ **Success:** SMS reply: `"Command Executed"`
- ❌ **Failure:** 
  - `"Password is not valid"` - Old password incorrect
  - `"Parameters missing"` - Missing required parameters

**System Action:**
- Validates old password
- Updates password in EEPROM
- Sends confirmation SMS

---

#### 6.6 TIMESLOT Command

**Command Format:**
```
&TIMESLOT,<user_id>,<in_hour>,<in_minute>,<out_hour>,<out_minute>#
```

**Example:**
```
&TIMESLOT,02,09,00,17,30#
```

**Parameters:**
- `<user_id>`: User ID (1-28)
- `<in_hour>`: Access start hour (0-23)
- `<in_minute>`: Access start minute (0-59)
- `<out_hour>`: Access end hour (0-23)
- `<out_minute>`: Access end minute (0-59)

**Response:**
- ✅ **Success:** SMS reply: `"Command Executed"`
- ❌ **Failure:** 
  - `"Parameters missing"` - Missing required parameters
  - `"Invalid parameters"` - Invalid time values

**System Action:**
- Validates time values
- Stores time slot in EEPROM
- Sends confirmation SMS

**Note:** User can only access door during configured time window (unless no time slot configured)

---

#### 6.7 FACTRESET Command

**Command Format:**
```
&FACTRESET,<master_password>#
```

**Example:**
```
&FACTRESET,MyMasterPass123#
```

**Response:**
- ✅ **Success:** Fast flashes and reboots memory state
- ❌ **Failure:** Returns standard error if password fails

---

#### 6.8 OTP & LOSTPW Commands

**OTP Verification:**
- Format: `&OTP,<6_digit_code>#`
- Purpose: Disarms live alarm systems.

**Password Recovery (LOSTPW):**
- Standard Format: `&LOSTPW#` (Replies to sender with their own password)
- Master Override Format: `&LOSTPW,<user_id>#` (Master extracts another user's password)

---

### 7. Alarm Handling Flow

#### 7.1 Temperature Alarm

**Trigger Condition:** Temperature exceeds 60°C threshold

**Flow:**
1. **Detection:** Temperature sensor detects high temperature
2. **Alarm Activation:**
   - Sirens activate
   - LCD display turns off
   - OTP generated
   - SMS alerts sent to all configured users
3. **OTP Input Screen:**
   - System displays OTP input prompt
   - User must enter correct OTP to deactivate
4. **Deactivation:**
   - Enter correct OTP (6 digits)
   - System deactivates alarms
   - Sirens turn off
   - LCD display returns to normal
   - System returns to MAIN screen

**Display (OTP Input):**
```
[OTP input area]
```

**Master OTP:** `455556` (hardcoded for emergency)

---

#### 7.2 Vibration Alarm

**Trigger Condition:** Vibration sensor detects movement

**Flow:**
1. **Detection:** Vibration sensor triggers
2. **Alarm Activation:**
   - Sirens activate
   - LCD display turns off
   - OTP generated
   - SMS alerts sent to all configured users
3. **OTP Input:** Same as Temperature Alarm
4. **Deactivation:** Same as Temperature Alarm

---

#### 7.3 Gun Point Activation

**Trigger Condition:** Gun point button pressed for 3 seconds

**Flow:**
1. **Detection:** Gun point button held for timeout period
2. **Alarm Activation:**
   - Sirens activate
   - LCD display turns off
   - OTP generated
   - SMS alerts sent to all users (except triggering user)
   - Emergency calls made to all users
3. **OTP Input:** Same as Temperature Alarm
4. **Deactivation:** Same as Temperature Alarm

**Note:** This is an emergency activation system

---

### 8. Door Operation Flow

#### 8.1 Opening Door

**Prerequisites:** Successful dual authentication

**Flow:**
1. **Authentication:** Complete master + user authentication
2. **IR Alignment Check:**
   - ✅ **Aligned:** Proceeds to door opening
   - ❌ **Not Aligned:** Displays `Sensor Not Aligned!!`, stops operation
3. **Time/Holiday Check:**
   - ✅ **Allowed:** Proceeds to door opening
   - ❌ **Denied:** Displays `Holiday - No Access Allowed!` or `No Access Allowed!`
4. **Door Opening:**
   - Motor starts (CW direction)
   - Display: `OPENING DOOR [count]`
   - System monitors open sensor
5. **Completion:**
   - Open sensor triggered
   - Motor stops
   - Display: `DOOR OPENED [count]`
   - Door open count incremented
   - SMS notification sent

**Display (Opening):**
```
OPENING DOOR 15
```

**Display (Opened):**
```
DOOR OPENED 15
```

**Error Handling:**
- **Timeout:** If door doesn't open within timeout, displays `ERROR IN OPENING`
- **Sensor Error:** System attempts recovery

---

#### 8.2 Closing Door

**Methods:**
1. **Keypad:** Press `LOCK (*)` key from User Main Screen
2. **SMS:** Send `&LOCK,<user_id>,<password>#` command

**Flow:**
1. **Command:** User initiates close command
2. **Door Closing:**
   - Motor starts (CCW direction)
   - Display: `CLOSING DOOR ...`
   - System monitors close sensor
3. **Completion:**
   - Close sensor triggered
   - Motor stops
   - Display: `DOOR CLOSED`
   - SMS notification sent

**Display (Closing):**
```
CLOSING DOOR ...
```

**Display (Closed):**
```
DOOR CLOSED
```

**Error Handling:**
- **Timeout:** If door doesn't close within timeout, displays `ERROR IN CLOSING!!`
- **Retry:** System may attempt automatic retry

---

### 9. Error Recovery Flows

#### 9.1 Authentication Failure Recovery

**Scenario:** User enters wrong password

**Flow:**
1. **First Failure:**
   - Display: `Invld Password!!`
   - Wait 1 second
   - Return to MAIN screen
   - No SMS alert sent
2. **Second Failure (if applicable):**
   - Display: `Invld Password!!`
   - SMS alert sent to master user
   - Return to MAIN screen
   - User must restart authentication

**Recovery:** User must restart from Step 1 (Master Authentication)

---

#### 9.2 Door Operation Error Recovery

**Scenario:** Door fails to open/close

**Flow:**
1. **Error Detection:**
   - Timeout occurs
   - Display: `ERROR IN OPENING` or `ERROR IN CLOSING!!`
2. **Recovery Attempt:**
   - System may attempt automatic recovery
   - Motor stops
   - System returns to safe state
3. **User Action:**
   - Check sensor alignment
   - Check door mechanism
   - Retry operation

---

#### 9.3 GSM Communication Error Recovery

**Scenario:** GSM module not responding

**Flow:**
1. **Error Detection:**
   - GSM commands fail
   - Messages queued for later
2. **Recovery:**
   - System retries connection
   - Messages remain in queue
   - When GSM available, messages sent automatically

**Note:** Message queue can hold up to 10 messages

---

### 10. Quick Reference Guide

#### 10.1 Keypad Keys

| Key | Symbol | Function |
|-----|--------|----------|
| Enter | `#` | Submit/Confirm input |
| Cancel | `@` | Cancel/Go back |
| Lock | `*` | Lock/Close door |
| Power | `!` | Power control |
| Mute | `^` | Mute alerts |
| Alpha/Num | `&` | Switch input mode |
| 0-9 | `0-9` | Numeric input |

---

#### 10.2 Common Display Messages

| Message | Meaning | Action |
|---------|---------|--------|
| `PASSWORD:` | Enter password | Enter master password |
| `USER PASS/BIO :` | Enter user credentials | Enter user password or scan fingerprint |
| `Invld Password!!` | Wrong password | Re-enter password |
| `Sensor Not Aligned!!` | IR sensor misaligned | Check sensor alignment |
| `Holiday - No Access Allowed!` | Today is holiday | Access denied |
| `No Access Allowed!` | Outside time slot | Access denied |
| `OPENING DOOR [count]` | Door opening | Wait for completion |
| `DOOR OPENED [count]` | Door fully open | Access granted |
| `CLOSING DOOR ...` | Door closing | Wait for completion |
| `DOOR CLOSED` | Door fully closed | Operation complete |

---

#### 10.3 SMS Command Quick Reference

| Command | Format | Purpose |
|---------|--------|---------|
| UNLOCK (Step 1) | `&UNLOCK,01,<master_pw>#` | Verify master |
| UNLOCK (Step 2) | `&UNLOCK,<user_id>,<user_pw>#` | Verify user & unlock |
| LOCK | `&LOCK,<user_id>,<password>#` | Lock door |
| OTP | `&OTP,<otp_code>#` | Verify one-time password to clear alarms |
| ADDUSER | `&ADDUSER,<id>,<mobile>,<pw>#` | Add new user |
| REMOVEUSER | `&REMOVEUSER,<id>#` | Remove user |
| CHANGEPW | `&CHANGEPW,<id>,<old>,<new>#` | Change password |
| TIMESLOT | `&TIMESLOT,<id>,<in_h>,<in_m>,<out_h>,<out_m>#` | Set time slot restrictions |
| FACTRESET | `&FACTRESET,<master_pw>#` | Fully format EEPROM and clear users |
| LOSTPW | `&LOSTPW#` or `&LOSTPW,<id>#` | Request password via SMS return |

---

#### 10.4 Master Menu Quick Reference

| Option | Key | Function |
|--------|-----|----------|
| Add User | `1` | Add new user |
| Remove User | `2` | Remove existing user |
| Password | `3` | Change master password |
| Date/Time | `4` | Set system date/time |
| Mobile Number | `5` | Update user mobile number |
| Holiday | `6` | Manage holidays |
| Backup | `7` | Backup operations |
| Buzzer | `8` | Configure buzzer |
| Fingerprint | `9` | Add fingerprint |

---

### 11. Best Practices

#### 11.1 Authentication
- Always complete both authentication steps (master + user)
- Keep passwords secure and don't share
- Use fingerprint for faster access when available
- Remember: Master must authenticate first

#### 11.2 Door Operations
- Ensure IR sensor is aligned before operations
- Wait for door to fully open/close before proceeding
- Check door status on display
- Use `LOCK (*)` key to close door when done

#### 11.3 SMS Commands
- Complete UNLOCK in two steps within 60 seconds
- Verify mobile number is registered before sending commands
- Use correct command format with `&` start and `#` end
- Check SMS replies for confirmation

#### 11.4 User Management
- Master should regularly review user list
- Remove users who no longer need access
- Update mobile numbers when changed
- Configure time slots for security

#### 11.5 Holiday Management
- Add holidays in advance
- Review holiday list regularly
- Remove holidays that are no longer needed
- Remember: Holidays override time slots

#### 11.6 Alarm Handling
- Keep OTP safe and accessible
- Respond to alarms promptly
- Enter OTP correctly to deactivate
- Contact administrator if OTP doesn't work

---

## Security Features

### Dual Authentication
- Master must authenticate first (password or fingerprint)
- User must authenticate second (password or fingerprint)
- Prevents unauthorized access
- Two independent authentication methods supported

### Failure Tracking
- Tracks authentication failures via `user_bio_auth_fail_count`
- Password failures: Alert sent on first failure
- Fingerprint failures: Alert sent on first failure (count increments by 2)
- Automatic return to MAIN screen after failure
- Failure count resets on successful authentication or screen timeout

### Time-based Access
- Configurable access windows per user
- In/out time stored in EEPROM
- Format: HH:MM (24-hour format)
- Automatic denial outside configured window
- **Holiday Override**: Access denied on configured holidays regardless of time slot

### Holiday Management
- Configure up to 50 holidays for access denial
- Holidays stored in EEPROM (persistent across reboots)
- Access automatically denied on configured holidays
- Holiday check performed before time slot validation
- Master user can add, remove, and view holidays via menu (option 6)

### Alert System
- Real-time SMS alerts via GSM module
- Multiple alert types (7 different message types)
- Queue-based delivery (10 message queue)
- Automatic retry if GSM unavailable
- Alert recipients configurable per message type

### Alarm Systems
- **Temperature Alarm**: Triggers when temperature exceeds 60°C threshold
- **Vibration Alarm**: Detects vibration and triggers alarm
- **Gun Point Activation**: Emergency activation system
- **OTP System**: One-time password required to deactivate alarms
- **Siren Control**: Dual siren system (pins 0 and 1)

### Audit Trail
- Door open count tracking (stored in EEPROM)
- EEPROM logging of all user data
- Event timestamps via RTC
- Battery percentage monitoring
- SD card logging support (if SD card attached)

---

## Constants and Definitions

### User Limits
```c
#define MAX_NUM_OF_USERS 28  // Maximum users including master
#define MASTER_USER_ID 0     // Master user index
```

### Password Limits
```c
#define MASTER_PW_LEN 10     // Master reset password length
// Regular password: 4-15 characters
```

### Timeouts
```c
#define TEMPERATURE_THRESHOLD 60        // Temperature alarm threshold (°C)
#define DOOR_OPEN_TIMEOUT 10000         // Door opening timeout (10 seconds)
#define DOOR_OPEN_ERROR_TIMEOUT 5000    // Error recovery timeout (5 seconds)
#define GUN_POINT_PRESS_TIMEOUT 3000    // Gun point press timeout (3 seconds)
uint16_t display_on_timeout = 30000    // LCD auto-off timeout (30 seconds)
```

### Pin Definitions
```c
dc_motor_pin[2] = {2, 5}     // Motor control pins (CW/CCW)
sensor_pin[2] = {48, 47}     // Door sensors (open/close)
em_lock_control_pin = 6      // Electromagnetic lock control
ir_rx_pin = A0               // IR alignment sensor (analog)
buzzer_pin = 45              // Buzzer output
ONE_WIRE_BUS = 42            // Temperature sensor (OneWire bus)
siren_pin[2]                 // Dual siren outputs
LCD_GND, LCD_VCC             // LCD power control pins
battery_analog_input          // Battery voltage monitoring (analog)
```

---

## Development Notes

### TODO Items
- Complete door open/close FSM implementation (basic structure exists)
- Complete temperature sensor FSM implementation (monitoring active, FSM pending)
- Complete vibration sensor FSM implementation (monitoring active, FSM pending)
- Implement time-based access control logic (configuration exists, validation pending)
- Enhanced error recovery mechanisms
- Additional security features
- Complete gun point activation FSM (basic structure exists)

### Debugging
- Enable `DEBUG` define for serial output
- Use `Serial.print()` for debugging
- Monitor GSM module responses
- Check EEPROM data integrity

### Testing
1. Test authentication flows
2. Verify door control operations
3. Test GSM communication
4. Validate EEPROM operations
5. Test failure scenarios

---

## Version History

### Current Version
- Dual authentication system
- Fingerprint support
- GSM alert system
- Door control with sensors
- Time-based access control
- EEPROM persistence

### Recent Changes
- Overhauled GSM module initialization to massively increase SMS reliability
- Switched SMS receipt architecture from volatile push (`+CMT:`) to buffered retrieval (`AT+CMGR=`) via `+CMTI` pings, completely fixing message truncation errors
- Standardized remote SMS control API syntax (e.g. `ADDUSER`, `TIMESLOT`, `FACTRESET`)
- Integrated separate SMS API Reference file (`SMS_API_Documentation.md`)
- Fine-tuned EEPROM string parsers to securely ingest mobile numbers without over-allocating array buffers
- Implemented dual authentication system (master + user)
- Added fingerprint authentication support
- Integrated GSM alert system
- Added temperature and vibration alarm systems
- Implemented gun point activation system
- Added OTP system for alarm deactivation
- Battery monitoring and display
- Door control with sensor feedback
- Time-based access control

---

## Support and Maintenance

### Common Issues

1. **Door not opening/closing**
   - Check sensor alignment
   - Verify motor connections
   - Check timeout settings

2. **GSM not sending messages**
   - Verify SIM card
   - Check network registration
   - Review queue status

3. **Fingerprint not recognized**
   - Re-enroll fingerprint
   - Clean sensor surface
   - Check sensor connections

4. **Authentication failures**
   - Verify password format
   - Check user configuration
   - Review EEPROM data

### Maintenance Tasks
- Regular EEPROM backup
- Sensor cleaning
- Battery check (if applicable)
- Firmware updates
- User data review

---

## License and Credits

This documentation covers the Key Pad Security System implementation.
For technical support or questions, refer to the code comments and system logs.

---

**End of Documentation**

