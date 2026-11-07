#include <M5Unified.h>
#include "VoronoiDiagram.h"
#include "TouchHandler.h"
#include "TaskManager.h"
#include "SoundManager.h"

// Off-screen buffers (double buffer: while one is transferred to the
// display, the next frame is rendered into the other)
static M5Canvas screenBuffers[2];

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
    
    // Create sprites. Keep the buffers in internal SRAM (76,800 bytes
    // each), which both cores can read and write faster than PSRAM;
    // fall back to PSRAM if internal SRAM is insufficient
    for (int i = 0; i < 2; ++i) {
        screenBuffers[i].setColorDepth(8);  // Set to 8-bit color depth
        screenBuffers[i].setPsram(false);
        if (screenBuffers[i].createSprite(width, height) == nullptr) {
            screenBuffers[i].setPsram(true);
            screenBuffers[i].createSprite(width, height);
        }
    }

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
    voronoiDiagram = new VoronoiDiagram(screenBuffers[0], screenBuffers[1], drawMutex);

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
