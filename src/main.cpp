#include <M5Unified.h>
#include "VoronoiDiagram.h"
#include "TouchHandler.h"
#include "TaskManager.h"
#include "SoundManager.h"

// Off-screen buffer
static M5Canvas screenBuffer;

// Global sound manager
static SoundManager soundManager;

// Application setup
static bool setupApplication() {
    // Initialize M5Stack
    auto cfg = M5.config();
    M5.begin(cfg);

    // Display settings (landscape orientation)
    M5.Display.setRotation(1);
    M5.Display.startWrite();

    // Create off-screen buffer
    screenBuffer.setPsram(true);
    
    const int width = M5.Display.width();
    const int height = M5.Display.height();
    
    // Create sprite
    screenBuffer.createSprite(width, height);

    // Initialize sound manager and play startup sound
    soundManager.initialize();
    soundManager.playStartupSequence();
    
    return true;
}

// Application entry point
extern "C" void app_main() {
    // Initial setup
    if (!setupApplication()) {
        // Exit if setup fails
        return;
    }

    // Create mutex for drawing
    SemaphoreHandle_t drawMutex = xSemaphoreCreateMutex();
    
    // If mutex creation fails
    if (drawMutex == nullptr) {
        return;
    }
    
    configASSERT(drawMutex);

    // Create Voronoi diagram
    VoronoiDiagram voronoiDiagram(screenBuffer, drawMutex);

    // Create touch handler
    TouchHandler touchHandler(voronoiDiagram, soundManager);

    // Create task manager
    TaskManager taskManager(voronoiDiagram, touchHandler);

    // Initialize tasks
    taskManager.initializeTasks();

    // Main thread waits idle (checking alive every 1 second)
    static constexpr uint32_t MAIN_THREAD_DELAY_MS = 1000;
    
    while (true) {
        vTaskDelay(MAIN_THREAD_DELAY_MS / portTICK_PERIOD_MS);
    }
}
