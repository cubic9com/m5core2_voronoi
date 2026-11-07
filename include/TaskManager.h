#pragma once

#include <Arduino.h>
#include "VoronoiDiagram.h"
#include "TouchHandler.h"
#include "SoundManager.h"

// Class for managing FreeRTOS tasks
class TaskManager {
public:
    // Constructor
    TaskManager(VoronoiDiagram& voronoi, TouchHandler& touch);

    // Destructor
    ~TaskManager();

    // Initialize tasks
    void initializeTasks();

private:
    // Voronoi diagram
    VoronoiDiagram& voronoiDiagram;

    // Touch handler
    TouchHandler& touchHandler;

    // Task handles
    TaskHandle_t touchTaskHandle = nullptr;
    TaskHandle_t drawTaskHandle = nullptr;
    TaskHandle_t pushTaskHandle = nullptr;
    TaskHandle_t renderWorkerTaskHandle = nullptr;

    // Mutex for drawing
    SemaphoreHandle_t drawMutex = nullptr;

    // Main task function (static)
    static void touchTaskFunction(void* args);

    // Voronoi task function (static)
    static void drawTaskFunction(void* args);

    // Display transfer task function (static)
    static void pushTaskFunction(void* args);

    // Render worker task function (static)
    static void renderWorkerTaskFunction(void* args);
};
