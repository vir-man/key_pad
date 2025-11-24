# Debug Level System Usage Guide

## Overview
The debug system allows you to control Serial.print/println output based on severity levels (1, 2, 3, 4). This provides granular control over debug output verbosity, allowing you to see only the information you need for troubleshooting.

## Configuration
Set `DEBUG_LEVEL` in `new_key_pad.ino`:
```cpp
#define DEBUG_LEVEL 0  // No debug (production) - all debug macros disabled
#define DEBUG_LEVEL 1  // Critical errors only
#define DEBUG_LEVEL 2  // Critical + Warnings
#define DEBUG_LEVEL 3  // Critical + Warnings + Informational
#define DEBUG_LEVEL 4  // All debug output (most verbose)
```

**Note:** When `DEBUG_LEVEL` is set to a value, it enables that level and all lower levels. For example, `DEBUG_LEVEL 3` enables levels 1, 2, and 3.

## Debug Levels

### Level 1 - Critical (Highest Priority)
Use for:
- Critical errors
- System failures
- Authentication failures
- Hardware errors

**Example:**
```cpp
DBG_L1_PRINTLN("FATAL: Fingerprint sensor not found!");
DBG_L1("Error code: ");
DBG_L1_PRINTLN(error_code);
```

### Level 2 - Warnings (Medium Priority)
Use for:
- Warnings
- State changes
- Important events
- Non-critical errors

**Example:**
```cpp
DBG_L2_PRINTLN("WARNING: Temperature threshold exceeded");
DBG_L2("State changed to: ");
DBG_L2_PRINTLN(state_name);
```

### Level 3 - Informational (Medium-Low Priority)
Use for:
- General information messages
- State transitions
- Sensor readings
- Configuration changes
- Status updates

**Example:**
```cpp
DBG_L3_PRINTLN("Temperature value is : ");
DBG_L3_PRINTLN(temperature_value);
DBG_L3_PRINTLN("LCD_ON");
DBG_L3_PRINTLN(F("Moving CW"));
```

### Level 4 - Verbose/Trace (Lowest Priority)
Use for:
- Detailed tracing
- Variable values in loops
- Function entry/exit
- Parameter dumps
- Development debugging
- Raw data output

**Example:**
```cpp
DBG_L4_PRINTLN("Entering function: process_user_input()");
DBG_L4("User ID: ");
DBG_L4_PRINTLN(user_id);
DBG_L4("Password length: ");
DBG_L4_PRINTLN(pass_length);
DBG_L4("CMD:");
DBG_L4_PRINTLN(cmd);
```

## Available Macros

All debug macros support variadic arguments, meaning they can accept multiple parameters just like `Serial.print()` and `Serial.println()`. This is useful for formatting options like `HEX`, `DEC`, `BIN`, etc.

### Level 1 Macros
- `DBG_L1(...)` - Print string/variable (no newline), supports multiple arguments
- `DBG_L1_PRINT(...)` - Print variable/value, supports multiple arguments
- `DBG_L1_PRINTLN(...)` - Print with newline, supports multiple arguments

**Example with multiple arguments:**
```cpp
DBG_L1_PRINTLN(finger.status_reg, HEX);  // Prints in hexadecimal
DBG_L1_PRINTLN(p, HEX);  // Prints error code in hex
```

### Level 2 Macros
- `DBG_L2(...)` - Print string/variable (no newline), supports multiple arguments
- `DBG_L2_PRINT(...)` - Print variable/value, supports multiple arguments
- `DBG_L2_PRINTLN(...)` - Print with newline, supports multiple arguments

### Level 3 Macros
- `DBG_L3(...)` - Print string/variable (no newline), supports multiple arguments
- `DBG_L3_PRINT(...)` - Print variable/value, supports multiple arguments
- `DBG_L3_PRINTLN(...)` - Print with newline, supports multiple arguments

**Example with multiple arguments:**
```cpp
DBG_L3_PRINTLN(finger.status_reg, HEX);
DBG_L3_PRINTLN(finger.system_id, HEX);
DBG_L3_PRINTLN(finger.device_addr, HEX);
```

### Level 4 Macros
- `DBG_L4(...)` - Print string/variable (no newline), supports multiple arguments
- `DBG_L4_PRINT(...)` - Print variable/value, supports multiple arguments
- `DBG_L4_PRINTLN(...)` - Print with newline, supports multiple arguments

## Production Logging (Always Active)
These macros are NOT affected by DEBUG_LEVEL and always output:
- `SERIAL_PRINT(s)` - Always prints (uses F() macro for PROGMEM)
- `SERIAL_PRINTLN(s)` - Always prints with newline

Use these for production logging that should always be visible.

## Migration Examples

### Before:
```cpp
Serial.println("Error: Invalid password");
Serial.print("User ID: ");
Serial.println(user_id);
```

### After (Level 1 - Critical):
```cpp
DBG_L1_PRINTLN("Error: Invalid password");
DBG_L1("User ID: ");
DBG_L1_PRINTLN(user_id);
```

### After (Level 2 - Warning):
```cpp
DBG_L2_PRINTLN("Warning: Temperature rising");
DBG_L2("Current temp: ");
DBG_L2_PRINTLN(temperature);
```

### After (Level 3 - Informational):
```cpp
DBG_L3_PRINTLN("Temperature value is : ");
DBG_L3_PRINTLN(temperature_value);
DBG_L3_PRINTLN(F("Moving CW"));
```

### After (Level 4 - Verbose/Trace):
```cpp
DBG_L4_PRINTLN("Function: verify_password() called");
DBG_L4("Password length: ");
DBG_L4_PRINTLN(pass_length);
DBG_L4("CMD:");
DBG_L4_PRINTLN(cmd);
```

## Best Practices

1. **Use Level 1 for critical errors** that need immediate attention (system failures, hardware errors, authentication failures)
2. **Use Level 2 for warnings** that are important but not fatal (access denied, state changes, important events)
3. **Use Level 3 for informational messages** (sensor readings, state transitions, configuration changes)
4. **Use Level 4 for verbose debugging** (detailed tracing, variable dumps, function entry/exit, development debugging)
5. **Use SERIAL_PRINT/PRINTLN** for production logs that must always be visible (these are not affected by DEBUG_LEVEL)
6. **Set DEBUG_LEVEL to 0** for production deployments to disable all debug output (zero performance overhead)
7. **Set DEBUG_LEVEL to 1-2** for field troubleshooting to see only critical issues
8. **Set DEBUG_LEVEL to 3** for normal development to see important information
9. **Set DEBUG_LEVEL to 4** for deep debugging when you need maximum visibility

## Example Usage in Code

```cpp
void verify_password() {
  DBG_L4_PRINTLN("Entering verify_password()");  // Level 4: Function entry
  
  if (pass_length < 4) {
    DBG_L1_PRINTLN("ERROR: Password too short!");  // Level 1: Critical error
    return false;
  }
  
  DBG_L4("Checking password for user: ");  // Level 4: Detailed trace
  DBG_L4_PRINTLN(user_id);
  
  if (!is_password_valid(user_id, password, pass_length)) {
    DBG_L2_PRINTLN("WARNING: Invalid password attempt");  // Level 2: Warning
    return false;
  }
  
  DBG_L3_PRINTLN("Password verified successfully");  // Level 3: Informational
  return true;
}

void read_fingerprint_sensor() {
  DBG_L3_PRINTLN(F("Reading sensor parameters"));  // Level 3: Info
  
  DBG_L3(F("Status: 0x"));
  DBG_L3_PRINTLN(finger.status_reg, HEX);  // Multiple arguments supported
  
  DBG_L3(F("Sys ID: 0x"));
  DBG_L3_PRINTLN(finger.system_id, HEX);
  
  if (finger.status_reg == 0) {
    DBG_L1_PRINTLN("Did not find fingerprint sensor :(");  // Level 1: Critical
  } else {
    DBG_L2_PRINTLN("Found fingerprint sensor!");  // Level 2: Important event
  }
}
```

## Performance Note
When `DEBUG_LEVEL` is set to 0, all debug macros expand to nothing (empty), so there's **zero performance overhead**. The compiler completely removes the debug code during compilation. This means you can leave debug statements in your production code without any impact on performance or memory usage.

## Debug Level Summary

| Level | When to Use | Example Messages |
|-------|-------------|-----------------|
| **1** | Critical errors requiring immediate attention | "ERROR IN OPENING DOOR", "Did not find fingerprint sensor", "Communication error" |
| **2** | Warnings and important events | "Access Allowed!", "No Access Allowed!", "Holiday - No Access Allowed!", "Message Sent!!" |
| **3** | Informational messages and status updates | "Temperature value is :", "LCD_ON", "Moving CW", "Generated OTP :" |
| **4** | Verbose debugging and detailed tracing | "Entering function:", "CMD:", "Para[0]:", variable dumps, loop iterations |

## Quick Reference

- **Production (no debug)**: `#define DEBUG_LEVEL 0`
- **Field troubleshooting**: `#define DEBUG_LEVEL 1` or `2`
- **Normal development**: `#define DEBUG_LEVEL 3`
- **Deep debugging**: `#define DEBUG_LEVEL 4`

