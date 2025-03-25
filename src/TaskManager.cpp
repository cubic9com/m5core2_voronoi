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
    
    Serial.println("Tasks initialized successfully");
}

// Create mutex for drawing
SemaphoreHandle_t TaskManager::createDrawMutex() {
    // Delete mutex if it already exists
    if (drawMutex != nullptr) {
        vSemaphoreDelete(drawMutex);
        drawMutex = nullptr;
    }
    
    // Create new mutex
    drawMutex = xSemaphoreCreateMutex();
    
    // Verify mutex creation
    if (drawMutex == nullptr) {
        Serial.println("Failed to create draw mutex");
        return nullptr;
    }
    
    return drawMutex;
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
