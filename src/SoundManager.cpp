#include "SoundManager.h"

// Constructor
SoundManager::SoundManager() {
}

// Initialize sound system
void SoundManager::initialize() {
    // Set volume
    M5.Speaker.setVolume(DEFAULT_VOLUME);
}

// Play a sound effect
void SoundManager::playSound(SoundType type) {
    switch (type) {
        case SoundType::TOUCH:
            M5.Speaker.tone(TOUCH_FREQUENCY, TOUCH_DURATION);
            break;
        case SoundType::STARTUP:
            M5.Speaker.tone(STARTUP_FREQUENCY, STARTUP_DURATION);
            break;
    }
}

// Play startup sequence
void SoundManager::playStartupSequence() {
    // Play three tones in sequence
    playSound(SoundType::STARTUP);
    vTaskDelay(STARTUP_DELAY);
    playSound(SoundType::STARTUP);
    vTaskDelay(STARTUP_DELAY);
    playSound(SoundType::STARTUP);
}
