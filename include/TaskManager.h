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

    // Create mutex for drawing
    SemaphoreHandle_t createDrawMutex();

private:
    // Voronoi diagram
    VoronoiDiagram& voronoiDiagram;

    // Touch handler
    TouchHandler& touchHandler;

    // Task handles
    TaskHandle_t touchTaskHandle = nullptr;
    TaskHandle_t drawTaskHandle = nullptr;

    // Mutex for drawing
    SemaphoreHandle_t drawMutex = nullptr;

    // Main task function (static)
    static void touchTaskFunction(void* args);

    // Voronoi task function (static)
    static void drawTaskFunction(void* args);
};
