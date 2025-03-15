#include "TaskManager.h"

// Constructor
TaskManager::TaskManager(VoronoiDiagram& voronoi, TouchHandler& touch)
    : voronoiDiagram(voronoi), touchHandler(touch) {
}

// Destructor
TaskManager::~TaskManager() {
    // Delete tasks if they are running
    if (mainTaskHandle != nullptr) {
        vTaskDelete(mainTaskHandle);
        mainTaskHandle = nullptr;
    }
    
    if (voronoiTaskHandle != nullptr) {
        vTaskDelete(voronoiTaskHandle);
        voronoiTaskHandle = nullptr;
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
    
    // Create main task (runs on CPU0)
    BaseType_t mainTaskResult = xTaskCreatePinnedToCore(
        &TaskManager::mainTaskFunction,
        "task1-main",
        TASK_STACK_SIZE,
        this,
        2,  // Higher priority
        &mainTaskHandle,
        0   // Run on CPU0
    );
    
    // If task creation fails
    if (mainTaskResult != pdPASS) {
        // Error handling (in a real application, would log or perform recovery)
        return;
    }
    
    // Verify created task handle
    configASSERT(mainTaskHandle);

    // Create Voronoi task (runs on CPU1)
    BaseType_t voronoiTaskResult = xTaskCreatePinnedToCore(
        &TaskManager::voronoiTaskFunction,
        "task2-voronoi",
        TASK_STACK_SIZE,
        this,
        1,  // Lower priority
        &voronoiTaskHandle,
        1   // Run on CPU1
    );
    
    // If task creation fails
    if (voronoiTaskResult != pdPASS) {
        // Error handling (delete main task to free resources)
        if (mainTaskHandle != nullptr) {
            vTaskDelete(mainTaskHandle);
            mainTaskHandle = nullptr;
        }
        return;
    }
    
    // Verify created task handle
    configASSERT(voronoiTaskHandle);
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
        return nullptr;
    }
    
    configASSERT(drawMutex);
    return drawMutex;
}

// Main task function (static)
void TaskManager::mainTaskFunction(void* args) {
    // Early return if args is null
    if (args == nullptr) {
        vTaskDelete(nullptr);
        return;
    }
    
    // Get this pointer
    TaskManager* self = static_cast<TaskManager*>(args);
    
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

// Voronoi task function (static)
void TaskManager::voronoiTaskFunction(void* args) {
    // Early return if args is null
    if (args == nullptr) {
        vTaskDelete(nullptr);
        return;
    }
    
    // Get this pointer
    TaskManager* self = static_cast<TaskManager*>(args);
    
    // Drawing interval (milliseconds)
    static constexpr uint32_t DRAW_INTERVAL_MS = 100;
    
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
