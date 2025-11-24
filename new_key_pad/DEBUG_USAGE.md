# Debug Level System Usage Guide

## Overview
The debug system allows you to control Serial.print/println output based on severity levels (1, 2, 3).

## Configuration
Set `DEBUG_LEVEL` in `new_key_pad.ino`:
```cpp
#define DEBUG_LEVEL 0  // No debug (production)
#define DEBUG_LEVEL 1  // Critical only
#define DEBUG_LEVEL 2  // Critical + Warnings
#define DEBUG_LEVEL 3  // All debug output
```

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

### Level 3 - Verbose (Lowest Priority)
Use for:
- Detailed tracing
- Variable values
- Function entry/exit
- Development debugging

**Example:**
```cpp
DBG_L3_PRINTLN("Entering function: process_user_input()");
DBG_L3("User ID: ");
DBG_L3_PRINTLN(user_id);
DBG_L3("Password length: ");
DBG_L3_PRINTLN(pass_length);
```

## Available Macros

### Level 1 Macros
- `DBG_L1(x)` - Print string (no newline)
- `DBG_L1_PRINT(x)` - Print variable/value
- `DBG_L1_PRINTLN(x)` - Print with newline

### Level 2 Macros
- `DBG_L2(x)` - Print string (no newline)
- `DBG_L2_PRINT(x)` - Print variable/value
- `DBG_L2_PRINTLN(x)` - Print with newline

### Level 3 Macros
- `DBG_L3(x)` - Print string (no newline)
- `DBG_L3_PRINT(x)` - Print variable/value
- `DBG_L3_PRINTLN(x)` - Print with newline

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

### After (Level 3 - Verbose):
```cpp
DBG_L3_PRINTLN("Function: verify_password() called");
DBG_L3("Password length: ");
DBG_L3_PRINTLN(pass_length);
```

## Best Practices

1. **Use Level 1 for critical errors** that need immediate attention
2. **Use Level 2 for warnings** that are important but not fatal
3. **Use Level 3 for development** debugging and detailed tracing
4. **Use SERIAL_PRINT/PRINTLN** for production logs that must always be visible
5. **Set DEBUG_LEVEL to 0** for production deployments to disable all debug output

## Example Usage in Code

```cpp
void verify_password() {
  DBG_L3_PRINTLN("Entering verify_password()");
  
  if (pass_length < 4) {
    DBG_L1_PRINTLN("ERROR: Password too short!");
    return false;
  }
  
  DBG_L3("Checking password for user: ");
  DBG_L3_PRINTLN(user_id);
  
  if (!is_password_valid(user_id, password, pass_length)) {
    DBG_L2_PRINTLN("WARNING: Invalid password attempt");
    return false;
  }
  
  DBG_L3_PRINTLN("Password verified successfully");
  return true;
}
```

## Performance Note
When DEBUG_LEVEL is set to 0, all debug macros expand to nothing (empty), so there's zero performance overhead. The compiler completely removes the debug code.

