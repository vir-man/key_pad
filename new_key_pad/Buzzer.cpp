#include "Buzzer.h"
#include <avr/pgmspace.h>

// Original Melody Data in PROGMEM
// notes in the melody (store in flash to save SRAM)
const uint16_t MELODY_NOTES[] PROGMEM = {
    NOTE_C4, NOTE_G3, NOTE_G3, NOTE_A3, NOTE_G3, 0, NOTE_B3, NOTE_C4
};

// note durations: 4 = quarter note, 8 = eighth note, etc.:
const int MELODY_DURATIONS[] PROGMEM = {
    4, 8, 8, 4, 4, 4, 4, 4
};

Buzzer& Buzzer::getInstance() {
    static Buzzer instance;
    return instance;
}

Buzzer::Buzzer()
    : _muted(false), _playingMelody(false), _lastUpdate(0), _currentNote(0),
      _doorBeepActive(false), _doorBeepTimer(0), _beepState(false)
{
}

void Buzzer::begin() {
    pinMode(PIN_BUZZER, OUTPUT);
    noTone(PIN_BUZZER);
}

void Buzzer::update() {
    if (_muted) return;
    
    unsigned long currentMillis = millis();
    
    // Handle periodic door beep (from original buzzer_task)
    // Only if explicitly triggered by logic (e.g. Door open too long)
    if (_doorBeepActive) {
        if (currentMillis - _doorBeepTimer > TIMEOUT_Buzzer_Interval) {
            _doorBeepTimer = currentMillis;
            _beepState = !_beepState;
            
            if (_beepState) {
                // Play second note as tone, as per original logic?
                // tone(buzzer_pin, pgm_read_word(&melody[1]), BUZZER_TONE_DURATION_MS);
                tone(PIN_BUZZER, pgm_read_word(&MELODY_NOTES[1]), 200); 
            } else {
                noTone(PIN_BUZZER);
            }
        }
    }
}

void Buzzer::playTone(uint16_t freq, uint16_t duration) {
    if (_muted) return;
    tone(PIN_BUZZER, freq, duration);
}

void Buzzer::mute(bool enable) {
    _muted = enable;
    if (_muted) noTone(PIN_BUZZER);
}

void Buzzer::playMelodyStart() {
    // Enable the periodic beep behavior
    // This naming is confusing. Original `buzzer_task` just blinked a specific tone.
    // Let's call this "startAlert()"
    _doorBeepActive = true;
    _doorBeepTimer = millis();
}
