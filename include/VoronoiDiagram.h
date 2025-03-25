#pragma once

#include <M5Unified.h>
#include <Arduino.h>
#include <vector>
#include <cmath>
#include <memory>
#include <esp_heap_caps.h>
#include <algorithm>

// Class for managing Voronoi diagram
class VoronoiDiagram {
public:
    // Point structure
    struct Point {
        int x;
        int y;
        uint16_t color;
    };

    // Seed point structure for Jump Flooding Algorithm
    struct SeedPoint {
        int16_t x;      // x-coordinate
        int16_t y;      // y-coordinate
        int16_t idx;    // index of the original point
    };

    // Constructor
    VoronoiDiagram(M5Canvas& buffer, SemaphoreHandle_t mutex);
    
    // Destructor
    ~VoronoiDiagram();

    // Add a point
    void addPoint(int x, int y);

    // Draw Voronoi diagram
    void draw();

private:
    // Apply repulsive force to move points
    void applyRepulsiveForce();

    // Render Voronoi diagram using Jump Flooding Algorithm
    void renderVoronoiDiagram();
    
    // Initialize JFA buffers in internal SRAM (with fallback to PSRAM)
    void initJFABuffers();
    
    // Free JFA buffers
    void freeJFABuffers();
    
    // Execute Jump Flooding Algorithm
    void executeJFA();

    // Render points
    void renderPoints();

    // Get index of the nearest point (used as fallback)
    int getNearestPointIndex(int x, int y) const;

    // List of points
    std::vector<Point> points;

    // Drawing buffer
    M5Canvas& screenBuffer;

    // Mutex for drawing
    SemaphoreHandle_t drawMutex;

    // JFA buffers (allocated in internal SRAM when possible, with fallback to PSRAM)
    SeedPoint* jfaBufferA = nullptr;
    SeedPoint* jfaBufferB = nullptr;
    
    // Screen dimensions
    int screenWidth = 0;
    int screenHeight = 0;
    int screenSize = 0;  // width * height

    // Maximum number of points
    static constexpr std::size_t MAX_POINT_COUNT = 16U;

    // Repulsion force parameters
    static constexpr float REPULSION_STRENGTH = 15000.0F;
    static constexpr float REPULSION_RADIUS = 150.0F;

    // Color palette (20 pastel colors) - RGB565 format
    static const uint16_t COLOR_PALETTE[20];
};
