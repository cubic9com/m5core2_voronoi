#include "TaskManager.h"

// Constructor
TaskManager::TaskManager(VoronoiDiagram& voronoi, TouchHandler& touch)
    : voronoiDiagram(voronoi), touchHandler(touch) {
}

// Destructor
TaskManager::~TaskManager() {
    // Delete tasks if they are running
    if (touchTaskHandle != nullptr) {
        vTaskDelete(touchTaskHandle);
        touchTaskHandle = nullptr;
    }

    if (drawTaskHandle != nullptr) {
        vTaskDelete(drawTaskHandle);
        drawTaskHandle = nullptr;
    }

    if (pushTaskHandle != nullptr) {
        vTaskDelete(pushTaskHandle);
        pushTaskHandle = nullptr;
    }

    if (renderWorkerTaskHandle != nullptr) {
        vTaskDelete(renderWorkerTaskHandle);
        renderWorkerTaskHandle = nullptr;
    }

    // Delete mutex if it exists
    if (drawMutex != nullptr) {
        vSemaphoreDelete(drawMutex);
        drawMutex = nullptr;
    }
}

// Initialize tasks
void TaskManager::initializeTasks() {
    // Task stack size
    static constexpr uint32_t TASK_STACK_SIZE = 4096;

    // Create Touch task (runs on CPU0)
    BaseType_t touchTaskResult = xTaskCreatePinnedToCore(
        &TaskManager::touchTaskFunction,
        "TouchTask",
        TASK_STACK_SIZE,
        this,
        2,  // Higher priority
        &touchTaskHandle,
        0   // Run on CPU0
    );

    // If task creation fails
    if (touchTaskResult != pdPASS) {
        // Error handling (in a real application, would log or perform recovery)
        Serial.println("Failed to create Touch task");
        return;
    }

    // Verify created task handle
    if (touchTaskHandle == nullptr) {
        Serial.println("Touch task handle is null");
        return;
    }

    // Create Draw task (runs on CPU1)
    BaseType_t drawTaskResult = xTaskCreatePinnedToCore(
        &TaskManager::drawTaskFunction,
        "DrawTask",
        TASK_STACK_SIZE,
        this,
        1,  // Lower priority
        &drawTaskHandle,
        1   // Run on CPU1
    );

    // If task creation fails
    if (drawTaskResult != pdPASS) {
        // Error handling (delete Touch task to free resources)
        Serial.println("Failed to create Draw task");
        if (touchTaskHandle != nullptr) {
            vTaskDelete(touchTaskHandle);
            touchTaskHandle = nullptr;
        }
        return;
    }

    // Verify created task handle
    if (drawTaskHandle == nullptr) {
        Serial.println("Draw task handle is null");
        if (touchTaskHandle != nullptr) {
            vTaskDelete(touchTaskHandle);
            touchTaskHandle = nullptr;
        }
        return;
    }

    // Create Push task (runs on CPU1): transfers rendered frames to the
    // display while the next frame is being rendered
    BaseType_t pushTaskResult = xTaskCreatePinnedToCore(
        &TaskManager::pushTaskFunction,
        "PushTask",
        TASK_STACK_SIZE,
        this,
        1,  // Same priority as the Draw task
        &pushTaskHandle,
        1   // Run on CPU1
    );

    if (pushTaskResult != pdPASS || pushTaskHandle == nullptr) {
        Serial.println("Failed to create Push task");
        vTaskDelete(touchTaskHandle);
        touchTaskHandle = nullptr;
        vTaskDelete(drawTaskHandle);
        drawTaskHandle = nullptr;
        pushTaskHandle = nullptr;
        return;
    }

    // Create Render worker task (runs on CPU0): renders the bottom half
    // of each frame in parallel with the Draw task
    BaseType_t renderWorkerTaskResult = xTaskCreatePinnedToCore(
        &TaskManager::renderWorkerTaskFunction,
        "RenderWorkerTask",
        TASK_STACK_SIZE,
        this,
        2,  // Same priority as the Touch task
        &renderWorkerTaskHandle,
        0   // Run on CPU0
    );

    if (renderWorkerTaskResult != pdPASS || renderWorkerTaskHandle == nullptr) {
        Serial.println("Failed to create Render worker task");
        vTaskDelete(touchTaskHandle);
        touchTaskHandle = nullptr;
        vTaskDelete(drawTaskHandle);
        drawTaskHandle = nullptr;
        vTaskDelete(pushTaskHandle);
        pushTaskHandle = nullptr;
        renderWorkerTaskHandle = nullptr;
        return;
    }

    Serial.println("Tasks initialized successfully");
}

// Touch task function (static)
void TaskManager::touchTaskFunction(void* args) {
    // Early return if args is null
    if (args == nullptr) {
        Serial.println("TouchTask args is null");
        vTaskDelete(nullptr);
        return;
    }

    // Get this pointer
    TaskManager* self = static_cast<TaskManager*>(args);

    Serial.println("TouchTask started");

    // Task main loop
    for (;;) {
        // Process touch input
        self->touchHandler.handleInput();

        // Prevent watchdog timeout (wait 1ms)
        vTaskDelay(1);
    }

    // Delete task (should never reach here)
    vTaskDelete(nullptr);
}

// Draw task function (static)
void TaskManager::drawTaskFunction(void* args) {
    // Early return if args is null
    if (args == nullptr) {
        Serial.println("Draw task args is null");
        vTaskDelete(nullptr);
        return;
    }

    // Get this pointer
    TaskManager* self = static_cast<TaskManager*>(args);

    // Drawing interval (milliseconds)
    static constexpr uint32_t DRAW_INTERVAL_MS = 10;

    Serial.println("Draw task started");

    // Task main loop
    for (;;) {
        // Draw Voronoi diagram
        self->voronoiDiagram.draw();

        // Wait for specified interval
        vTaskDelay(pdMS_TO_TICKS(DRAW_INTERVAL_MS));
    }

    // Delete task (should never reach here)
    vTaskDelete(nullptr);
}

// Push task function (static)
void TaskManager::pushTaskFunction(void* args) {
    // Early return if args is null
    if (args == nullptr) {
        Serial.println("Push task args is null");
        vTaskDelete(nullptr);
        return;
    }

    // Get this pointer
    TaskManager* self = static_cast<TaskManager*>(args);

    Serial.println("Push task started");

    // Transfer rendered frames to the display (never returns)
    self->voronoiDiagram.pushTaskLoop();

    // Delete task (should never reach here)
    vTaskDelete(nullptr);
}

// Render worker task function (static)
void TaskManager::renderWorkerTaskFunction(void* args) {
    // Early return if args is null
    if (args == nullptr) {
        Serial.println("Render worker task args is null");
        vTaskDelete(nullptr);
        return;
    }

    // Get this pointer
    TaskManager* self = static_cast<TaskManager*>(args);

    Serial.println("Render worker task started");

    // Render frame halves on request from draw() (never returns)
    self->voronoiDiagram.renderWorkerLoop();

    // Delete task (should never reach here)
    vTaskDelete(nullptr);
}
