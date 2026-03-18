# BMS Safe Lock - SMS API Reference Guide

This document defines the structure and syntax for controlling your BMS Safe Lock system remotely via SMS.

## General Command Format
All SMS commands sent to the SIM7600 number must strictly follow the format:  
`&[COMMAND_NAME],[PARAM_1],[PARAM_2],...,[PARAM_N]#`

*   **`&`** (Ampersand): Indicates the **start** of a command.
*   **`#`** (Hash): Indicates the **end** of the command.
*   **`,`** (Comma): Separates parameters. Do not include spaces between parameters.

## User IDs
*   **Master User:** `01`
*   **Standard Users:** `02` through `05`

---

## 🚪 Door Control Commands

### 1. UNLOCK DOOR
Unlocks the safe door. This follows a two-step verification if done by standard users (must be master verified first) or a single step if done by the master.
*   **API:** `UNLOCK`
*   **Syntax:** `&UNLOCK,<user_id>,<password>#`
*   **Example (Master):** `&UNLOCK,01,MyMasterPass123#`
*   **Example (User):** `&UNLOCK,02,UserPass456#`

### 2. LOCK DOOR
Locks the safe door without needing to check time-slots.
*   **API:** `LOCK`
*   **Syntax:** `&LOCK,<user_id>,<password>#`
*   **Example:** `&LOCK,01,MyMasterPass123#`

### 3. ONE-TIME PASSWORD (OTP)
Verifies a generated 6-digit OTP to silence alarms, reset states, and unlock the vault.
*   **API:** `OTP`
*   **Syntax:** `&OTP,<6_digit_otp>#`
*   **Example:** `&OTP,688108#`
*   *Note: Only the registered mobile number associated with the lock can execute this successfully.*

---

## 👥 User Management Commands (Master Only)
*The following commands can **only** be executed by the Master's registered mobile number (User Index 1 / ID `01`).*

### 4. ADD USER
Registers a new user allowing them access to the safe via Keypad or SMS.
*   **API:** `ADDUSER`
*   **Syntax:** `&ADDUSER,<user_id>,<10_digit_mobile_number>,<password>#`
*   *Parameters:*
    *   `user_id`: Target slot (e.g., `02` through `05`)
    *   `10_digit_mobile_number`: The new user's phone number without country code (e.g., `9876543210`)
    *   `password`: 4 to 15 character string
*   **Example:** `&ADDUSER,02,9876543210,NewUserPass99#`

### 5. REMOVE USER
Revokes access and deletes user credentials from EEPROM.
*   **API:** `REMOVEUSER`
*   **Syntax:** `&REMOVEUSER,<user_id>#`
*   *Parameters:* 
    *   `user_id`: Target slot to erase (e.g., `02`). Master (`01`) cannot be removed.
*   **Example:** `&REMOVEUSER,02#`

### 6. UPDATE TIME SLOT
Restricts a specific user's physical access to a daily window of time (24-hour formal)
*   **API:** `TIMESLOT`
*   **Syntax:** `&TIMESLOT,<user_id>,<in_hour>,<in_minute>,<out_hour>,<out_minute>#`
*   *Parameters:* All times must be strictly two digits (e.g., `09` instead of `9`).
*   **Example:** `&TIMESLOT,02,09,00,17,30#` *(User 02 is now allowed from 09:00 AM to 05:30 PM)*

### 7. FACTORY RESET
Erases all EEPROM states, removes all users, and factory resets the BMS system.
*   **API:** `FACT_RESET`
*   **Syntax:** `&FACT_RESET,<master_password>#`
*   **Example:** `&FACT_RESET,MyMasterPass123#`

---

## 🔒 Security Commands

### 8. CHANGE PASSWORD
Changes a user's password. Master can change anyone's password. Standard users can only change their own.
*   **API:** `CHANGEPW`
*   **Syntax:** `&CHANGEPW,<user_id>,<old_password>,<new_password>#`
*   **Example:** `&CHANGEPW,02,OldPass456,NewPass789#`

### 9. LOST PASSWORD (RECOVER)
Triggers the lock to send an SMS containing a forgotten password.
*   **API:** `LOSTPW`
*   **Syntax (User):** `&LOSTPW#` *(Responds to the sender's registered mobile with their password)*
*   **Syntax (Master Override):** `&LOSTPW,<user_id>#` *(Master requests the password of a specific user)*
*   **Example:** `&LOSTPW,02#`
