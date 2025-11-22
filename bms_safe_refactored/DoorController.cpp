#include "DoorController.h"
#include <avr/wdt.h>

// Motor directions
#define CW 0   // Clockwise (open)
#define CCW 1  // Counter-clockwise (close)

DoorController::DoorController(EEPROMStorage* storage) 
  : eeprom(storage), current_state(DoorState::CLOSED),
    is_door_opening(false), is_door_closing(false),
    b_error_in_door_open(false), b_command_open_door(false),
    b_command_close_door(false), door_open_start_time(0),
    door_close_start_time(0) {
  door_sensor_state[0] = false;
  door_sensor_state[1] = false;
}

bool DoorController::initialize() {
  if (eeprom == nullptr) {
    return false;
  }
  
  // Initialize motor pins
  pinMode(SystemConfig::DC_MOTOR_PIN_0, OUTPUT);
  pinMode(SystemConfig::DC_MOTOR_PIN_1, OUTPUT);
  pinMode(SystemConfig::EM_LOCK_CONTROL_PIN, OUTPUT);
  
  // Initialize sensor pins
  pinMode(SystemConfig::SENSOR_PIN_OPEN, INPUT_PULLUP);
  pinMode(SystemConfig::SENSOR_PIN_CLOSE, INPUT_PULLUP);
  pinMode(SystemConfig::IR_RX_PIN, INPUT);
  
  // Stop motor initially
  dc_motor_stop();
  
  // Read initial sensor states
  updateSensorStates();
  
  // Determine initial state
  if (isDoorClosed()) {
    current_state = DoorState::CLOSED;
  } else if (isDoorOpen()) {
    current_state = DoorState::OPEN;
  } else {
    current_state = DoorState::CLOSED;  // Default to closed
  }
  
  return true;
}

void DoorController::updateSensorStates() {
  door_sensor_state[0] = digitalRead(SystemConfig::SENSOR_PIN_OPEN);
  door_sensor_state[1] = digitalRead(SystemConfig::SENSOR_PIN_CLOSE);
}

bool DoorController::is_door_aligned_by_ir() {
  int ir_value = analogRead(SystemConfig::IR_RX_PIN);
  // Adjust threshold as needed
  return (ir_value > 500);  // Example threshold
}

void DoorController::dc_motor_stop() {
  digitalWrite(SystemConfig::DC_MOTOR_PIN_0, HIGH);
  digitalWrite(SystemConfig::DC_MOTOR_PIN_1, HIGH);
}

void DoorController::dc_motor_on(int direction) {
  if (direction == CW) {
    // Open door (clockwise)
    digitalWrite(SystemConfig::DC_MOTOR_PIN_0, LOW);
    digitalWrite(SystemConfig::DC_MOTOR_PIN_1, HIGH);
  } else if (direction == CCW) {
    // Close door (counter-clockwise)
    digitalWrite(SystemConfig::DC_MOTOR_PIN_0, HIGH);
    digitalWrite(SystemConfig::DC_MOTOR_PIN_1, LOW);
  }
}

bool DoorController::isDoorOpen() {
  updateSensorStates();
  return !door_sensor_state[0];  // Active low
}

bool DoorController::isDoorClosed() {
  updateSensorStates();
  return !door_sensor_state[1];  // Active low
}

bool DoorController::isDoorOpening() {
  return is_door_opening;
}

bool DoorController::isDoorClosing() {
  return is_door_closing;
}

DoorController::DoorState DoorController::getState() {
  return current_state;
}

ErrorCode DoorController::checkDoorAccess(uint8_t user_id) {
  // Check IR alignment
  if (!is_door_aligned_by_ir()) {
    return ErrorCode::DOOR_SENSOR_NOT_ALIGNED;
  }
  
  // Check if door is already open
  if (isDoorOpen()) {
    return ErrorCode::DOOR_ALREADY_OPEN;
  }
  
  return ErrorCode::SUCCESS;
}

ErrorCode DoorController::openDoor(uint8_t user_id) {
  // Check access
  ErrorCode err = checkDoorAccess(user_id);
  if (err != ErrorCode::SUCCESS) {
    return err;
  }
  
  // Check if already opening
  if (is_door_opening) {
    return ErrorCode::SUCCESS;  // Already in progress
  }
  
  // Start opening
  is_door_opening = true;
  is_door_closing = false;
  b_error_in_door_open = false;
  current_state = DoorState::OPENING;
  door_open_start_time = millis();
  
  dc_motor_on(CW);
  
  return ErrorCode::SUCCESS;
}

ErrorCode DoorController::closeDoor(uint8_t user_id) {
  // Check if door is already closed
  if (isDoorClosed()) {
    return ErrorCode::DOOR_ALREADY_CLOSED;
  }
  
  // Check if already closing
  if (is_door_closing) {
    return ErrorCode::SUCCESS;  // Already in progress
  }
  
  // Start closing
  is_door_closing = true;
  is_door_opening = false;
  current_state = DoorState::CLOSING;
  door_close_start_time = millis();
  
  dc_motor_on(CCW);
  
  return ErrorCode::SUCCESS;
}

void DoorController::stopDoor() {
  dc_motor_stop();
  is_door_opening = false;
  is_door_closing = false;
}

uint16_t DoorController::getDoorOpenCount() {
  if (eeprom != nullptr) {
    return eeprom->readDoorOpenCount();
  }
  return 0;
}

void DoorController::task() {
  updateSensorStates();
  
  // Handle door opening
  if (is_door_opening) {
    if (isDoorOpen()) {
      // Door fully open
      dc_motor_stop();
      is_door_opening = false;
      current_state = DoorState::OPEN;
      
      // Increment door open count
      if (eeprom != nullptr) {
        uint16_t count = eeprom->readDoorOpenCount();
        eeprom->writeDoorOpenCount(count + 1);
      }
    } else if (millis() - door_open_start_time > SystemConfig::DOOR_OPEN_TIMEOUT) {
      // Timeout
      dc_motor_stop();
      is_door_opening = false;
      b_error_in_door_open = true;
      current_state = DoorState::ERROR;
    } else if (!is_door_aligned_by_ir()) {
      // Sensor misaligned
      dc_motor_stop();
      is_door_opening = false;
      b_error_in_door_open = true;
      current_state = DoorState::ERROR;
    }
  }
  
  // Handle door closing
  if (is_door_closing) {
    if (isDoorClosed()) {
      // Door fully closed
      dc_motor_stop();
      is_door_closing = false;
      current_state = DoorState::CLOSED;
    } else if (millis() - door_close_start_time > SystemConfig::DOOR_OPEN_TIMEOUT) {
      // Timeout
      dc_motor_stop();
      is_door_closing = false;
      current_state = DoorState::ERROR;
    } else if (!is_door_aligned_by_ir()) {
      // Sensor misaligned
      dc_motor_stop();
      is_door_closing = false;
      current_state = DoorState::ERROR;
    }
  }
  
  // Process commands
  if (b_command_open_door) {
    b_command_open_door = false;
    dc_motor_stop();
    openDoor(0);  // User ID not needed for command
  }
  
  if (b_command_close_door) {
    b_command_close_door = false;
    dc_motor_stop();
    closeDoor(0);  // User ID not needed for command
  }
}

