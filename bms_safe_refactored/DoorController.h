#ifndef DOOR_CONTROLLER_H
#define DOOR_CONTROLLER_H

#include "SystemConfig.h"
#include "ErrorCodes.h"
#include "EEPROMStorage.h"

/**
 * Door Controller Class
 * Handles door motor control and sensor management
 */
class DoorController {
private:
  EEPROMStorage* eeprom;
  
  // Door state
  enum class DoorState {
    CLOSED,
    OPENING,
    OPEN,
    CLOSING,
    ERROR
  };
  
  DoorState current_state;
  bool is_door_opening;
  bool is_door_closing;
  bool b_error_in_door_open;
  bool b_error_in_door_close;
  bool b_command_open_door;
  bool b_command_close_door;
  
  unsigned long door_open_start_time;
  unsigned long door_close_start_time;
  
  // Sensor states
  bool door_sensor_state[2];  // [0] = open, [1] = close
  
  // Helper functions
  void dc_motor_stop();
  void dc_motor_on(int direction);
  bool is_door_aligned_by_ir();
  void updateSensorStates();
  
  // Friend function to access private members for UIStateMachine
  friend class UIStateMachine;
  
public:
  DoorController(EEPROMStorage* storage);
  bool initialize();
  
  // Door operations
  ErrorCode openDoor(uint8_t user_id);
  ErrorCode closeDoor(uint8_t user_id);
  void stopDoor();
  
  // State queries
  bool isDoorOpen();
  bool isDoorClosed();
  bool isDoorOpening();
  bool isDoorClosing();
  DoorState getState();
  
  // Task function (call in loop)
  void task();
  
  // Access control
  ErrorCode checkDoorAccess(uint8_t user_id);
  
  // Getters
  uint16_t getDoorOpenCount();
  bool hasDoorError();
  bool hasDoorCloseError();
};

#endif // DOOR_CONTROLLER_H

