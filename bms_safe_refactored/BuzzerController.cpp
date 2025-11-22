#include "BuzzerController.h"

const uint16_t BuzzerController::melody[] PROGMEM = {
  NOTE_C4, NOTE_G3, NOTE_G3, NOTE_A3, NOTE_G3, 0, NOTE_B3, NOTE_C4
};

const int BuzzerController::noteDurations[] PROGMEM = {
  4, 8, 8, 4, 4, 4, 4, 4
};

BuzzerController::BuzzerController() 
  : buzzer_timer(0), buzzer_on(false), sub_buzzer_on(false), buzzer_timeout(10) {
}

bool BuzzerController::initialize(uint16_t timeout) {
  pinMode(SystemConfig::BUZZER_PIN, OUTPUT);
  buzzer_timeout = timeout;
  turnOff();
  return true;
}

void BuzzerController::turnOn() {
  buzzer_on = true;
  buzzer_timer = millis();
  digitalWrite(SystemConfig::BUZZER_PIN, HIGH);
}

void BuzzerController::turnOff() {
  buzzer_on = false;
  sub_buzzer_on = false;
  digitalWrite(SystemConfig::BUZZER_PIN, LOW);
}

void BuzzerController::playMelody() {
  for (uint8_t i = 0; i < 8; i++) {
    int noteDuration = 1000 / pgm_read_word(&noteDurations[i]);
    uint16_t note = pgm_read_word(&melody[i]);
    
    if (note > 0) {
      tone(SystemConfig::BUZZER_PIN, note, noteDuration);
    }
    delay(noteDuration * 1.30);
    noTone(SystemConfig::BUZZER_PIN);
  }
}

void BuzzerController::task() {
  if (buzzer_on) {
    if (millis() - buzzer_timer > (buzzer_timeout * 1000)) {
      turnOff();
    }
  }
}

void BuzzerController::setTimeout(uint16_t timeout) {
  buzzer_timeout = timeout;
}

uint16_t BuzzerController::getTimeout() {
  return buzzer_timeout;
}

