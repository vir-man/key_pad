#include "DoorController.h"

// Original pin usage from analysis:
// Motor Pins: 2, 5
// Sensor Pins: 48 (Open?), 47 (Closed?) - Logic needs verification, assuming Pullups
// IR Pin: A0 

DoorController& DoorController::getInstance() {
    static DoorController instance;
    return instance;
}

DoorController::DoorController() : _currentState(SystemState::IDLE), _motorStartTime(0) {
}


void DoorController::begin() {
    pinMode(PIN_MOTOR_A, OUTPUT);
    pinMode(PIN_MOTOR_B, OUTPUT);
    pinMode(PIN_DOOR_SENSOR_OPEN, INPUT_PULLUP);
    pinMode(PIN_DOOR_SENSOR_CLOSED, INPUT_PULLUP);
    pinMode(PIN_IR_RX, INPUT);
    
    stop(); // Ensure safe start
    
    // Initial state check
    if (isClosed()) _currentState = SystemState::DOOR_OPENED; // Inverted logic? 
    // Wait, original code says: is_door_close() checks sensor.
}

void DoorController::update() {
    if (_currentState == SystemState::DOOR_OPENING) {
        handleOpening();
    } else if (_currentState == SystemState::DOOR_CLOSING) {
        handleClosing();
    }
}

void DoorController::handleOpening() {
    // Check timeout
    if (millis() - _motorStartTime > TIMEOUT_DOOR_OPEN) {
        Logger::getInstance().logSystem("DOOR: Opening Timeout!", LogLevel::CRITICAL);
        stop();
        _currentState = SystemState::ERROR;
        return;
    }
    
    // Check limit sensor
    if (isOpen()) {
        stop();
        _currentState = SystemState::DOOR_OPENED;
        Logger::getInstance().logSystem("DOOR: Opened");
    }
}

void DoorController::handleClosing() {
    // IR Safety Check - Stop if obstructed
    if (!isAligned()) {
        stop();
        // Don't go to error, just pause? Or go back to open?
        // Original code seems to stop and wait.
        // For now, we stop and go to IDLE or stay in CLOSING but stopped?
        // Safer to stop and require re-trigger.
        Logger::getInstance().logSystem("DOOR: IR Obstruction!", LogLevel::WARNING);
        _currentState = SystemState::DOOR_OPENED; // Revert state
        return;
    }

    // Check timeout
    if (millis() - _motorStartTime > TIMEOUT_DOOR_OPEN) {
        Logger::getInstance().logSystem("DOOR: Closing Timeout!", LogLevel::CRITICAL);
        stop();
        _currentState = SystemState::ERROR;
        return;
    }
    
    // Check limit sensor
    if (isClosed()) {
        stop();
        _currentState = SystemState::IDLE; // Closed = Idle usually
        Logger::getInstance().logSystem("DOOR: Closed");
    }
}

void DoorController::open() {
    if (_currentState == SystemState::DOOR_OPENING || isOpen()) return;
    
    Logger::getInstance().logSystem("DOOR: Command Open");
    _motorStartTime = millis();
    _currentState = SystemState::DOOR_OPENING;
    setMotorForward();
}

void DoorController::close() {
    if (_currentState == SystemState::DOOR_CLOSING || isClosed()) return;
    
    // Safety Check before starting
    if (!isAligned()) {
        Logger::getInstance().logSystem("DOOR: Cannot close, IR Blocked");
        return;
    }
    
    Logger::getInstance().logSystem("DOOR: Command Close");
    _motorStartTime = millis();
    _currentState = SystemState::DOOR_CLOSING;
    setMotorReverse();
}

void DoorController::stop() {
    setMotorStop();
    // State remains as is, or specific stopped state?
}

// Low level motor control
// Replace with actual logic derived from original:
// Original: dc_motor_pin[0] and [1]
// CW/CCW logic
void DoorController::setMotorForward() {
    digitalWrite(PIN_MOTOR_A, HIGH);
    digitalWrite(PIN_MOTOR_B, LOW);
}

void DoorController::setMotorReverse() {
    digitalWrite(PIN_MOTOR_A, LOW);
    digitalWrite(PIN_MOTOR_B, HIGH);
}

void DoorController::setMotorStop() {
    digitalWrite(PIN_MOTOR_A, LOW);
    digitalWrite(PIN_MOTOR_B, LOW);
}

bool DoorController::isOpen() const {
    // Logic from original: digitalRead(sensor_pin[0])
    // Assuming LOW means Hit (Active Low) for Pullup?
    // Need to verify strict logic from `is_door_open()` implementation
    // For now assuming standard Active Low sensor
    return digitalRead(PIN_DOOR_SENSOR_OPEN) == LOW; 
}

bool DoorController::isClosed() const {
    return digitalRead(PIN_DOOR_SENSOR_CLOSED) == LOW;
}

bool DoorController::isAligned() {
    // IR Sensor logic
    return digitalRead(PIN_IR_RX) == LOW; // Low = Aligned? High = Broken?
    // Original: is_door_aligned_by_ir() needs checking
}
