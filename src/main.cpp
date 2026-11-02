#include <M5Unified.h>
#include "VoronoiDiagram.h"
#include "TouchHandler.h"
#include "TaskManager.h"
#include "SoundManager.h"

// Off-screen buffer
static M5Canvas screenBuffer;

// Global sound manager
static SoundManager soundManager;

// Global objects
VoronoiDiagram* voronoiDiagram = nullptr;
TouchHandler* touchHandler = nullptr;
TaskManager* taskManager = nullptr;
SemaphoreHandle_t drawMutex = nullptr;

// Application setup
static bool setupApplication() {
    // Initialize M5Stack
    auto cfg = M5.config();
    M5.begin(cfg);

    // Display settings (landscape orientation)
    M5.Display.setRotation(1);
    M5.Display.setColorDepth(8);  // Set to 8-bit color depth
    M5.Display.startWrite();

    const int width = M5.Display.width();
    const int height = M5.Display.height();
    
    // Create sprite
    screenBuffer.setColorDepth(8);  // Set to 8-bit color depth
    screenBuffer.createSprite(width, height);

    // Show a start prompt in the center of the screen. Nothing else is
    // drawn until the first touch adds a point, and the first rendered
    // frame covers the whole screen, so the prompt only ever appears
    // at startup and disappears on the first touch
    M5.Display.setTextColor(WHITE);
    M5.Display.setTextSize(2);
    M5.Display.drawCenterString("Touch to start", width / 2,
                                height / 2 - M5.Display.fontHeight() / 2);

    // Initialize sound manager and play startup sound
    soundManager.initialize();
    soundManager.playStartupSequence();
    
    return true;
}

// Arduino setup function
void setup() {
    // Initial setup
    if (!setupApplication()) {
        // Exit if setup fails
        return;
    }

    // Create mutex for drawing
    drawMutex = xSemaphoreCreateMutex();
    
    // If mutex creation fails
    if (drawMutex == nullptr) {
        return;
    }
    
    configASSERT(drawMutex);

    // Create Voronoi diagram
    voronoiDiagram = new VoronoiDiagram(screenBuffer, drawMutex);

    // Create touch handler
    touchHandler = new TouchHandler(*voronoiDiagram, soundManager);

    // Create task manager
    taskManager = new TaskManager(*voronoiDiagram, *touchHandler);

    // Initialize tasks
    taskManager->initializeTasks();
}

// Arduino loop function
void loop() {
  // Main loop does nothing (all processing is done in separate tasks)
  vTaskDelay(1000 / portTICK_PERIOD_MS);
}
