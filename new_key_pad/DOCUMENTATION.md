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
ALPHA_SCREEN (9)             - Alpha input screen
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
- Checks if time-based access is configured for user
- Compares current time (hour*100 + minute) with `in_time` and `out_time`
- Returns true if access allowed (currently always returns 1 - time check logic needs implementation)

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

#### Buzzer Configuration
- Set buzzer timeout (stored in EEPROM)
- Configure alarm behavior
- Buzzer screen accessible from master menu (option 8)

#### Alpha Speed Configuration
- Configure alpha input speed
- Stored in EEPROM
- Affects character input rate

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
6. Alpha
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

#### Error Messages
```
Invld Password!!
Sensor Not Aligned!!
USER FINGERPRNT NOT MATCHED!
MASTER FINGERPRNT NOT MATCHED!
```

### User Feedback

- **Visual**: LCD messages
- **Audio**: Buzzer alerts
- **Remote**: SMS notifications

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
- Automatic denial outside configured window (implementation pending)

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
- Implemented dual authentication system (master + user)
- Added fingerprint authentication support
- Integrated GSM alert system
- Added temperature and vibration alarm systems
- Implemented gun point activation system
- Added OTP system for alarm deactivation
- Battery monitoring and display
- Door control with sensor feedback
- Time-based access control (configuration ready)

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

