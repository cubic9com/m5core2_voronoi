#pragma once

#include <M5Unified.h>
#include <Arduino.h>

// Class for managing sound effects
class SoundManager {
public:
    // Sound types
    enum class SoundType {
        TOUCH,
        STARTUP
    };

    // Constructor
    SoundManager();

    // Initialize sound system
    void initialize();

    // Play a sound effect
    void playSound(SoundType type);

    // Play startup sequence
    void playStartupSequence();

private:
    // Sound settings
    static constexpr uint8_t DEFAULT_VOLUME = 48U;
    
    // Touch feedback sound settings
    static constexpr float TOUCH_FREQUENCY = 659.26F;
    static constexpr uint32_t TOUCH_DURATION = 50U;
    
    // Startup sound settings
    static constexpr float STARTUP_FREQUENCY = 659.26F;
    static constexpr uint32_t STARTUP_DURATION = 50U;
    static constexpr uint32_t STARTUP_DELAY = 150U;
};
