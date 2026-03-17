#ifndef BUZZER_H
#define BUZZER_H

#include <Arduino.h>
#include "Config.h"
#include "Types.h"
#include "pitches.h"

// Define a simple melody structure or use legacy PROGMEM arrays manually?
// Cleanest is to expose high-level triggers: "playError", "playSuccess", "playDoorOpen"

class Buzzer {
public:
    static Buzzer& getInstance();
    
    void begin();
    void update(); // Handle non-blocking playback and periodic beeps
    
    // API
    void playTone(uint16_t freq, uint16_t duration);
    void playMelodyStart(); // Starts the "Door Open" melody
    void mute(bool enable);
    
    // Status
    bool isMuted() const { return _muted; }
    
private:
    Buzzer();
    
    bool _muted;
    bool _playingMelody;
    unsigned long _lastUpdate;
    uint8_t _currentNote;
    
    // Periodic Beep for Door Open
    bool _doorBeepActive;
    unsigned long _doorBeepTimer;
    bool _beepState;
};

#endif // BUZZER_H
