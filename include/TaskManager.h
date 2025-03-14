#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
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
    TaskHandle_t mainTaskHandle = nullptr;
    TaskHandle_t voronoiTaskHandle = nullptr;

    // Mutex for drawing
    SemaphoreHandle_t drawMutex = nullptr;

    // Main task function (static)
    static void mainTaskFunction(void* args);

    // Voronoi task function (static)
    static void voronoiTaskFunction(void* args);
};
