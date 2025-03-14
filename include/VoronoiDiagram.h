#pragma once

#include <M5Unified.h>
#include <vector>
#include <cmath>

// Class for managing Voronoi diagram
class VoronoiDiagram {
public:
    // Point structure
    struct Point {
        int x;
        int y;
        uint16_t color;
    };

    // Constructor
    VoronoiDiagram(M5Canvas& buffer, SemaphoreHandle_t mutex);

    // Add a point
    void addPoint(int x, int y);

    // Draw Voronoi diagram
    void draw();

private:
    // Apply repulsive force to move points
    void applyRepulsiveForce();

    // Render Voronoi diagram
    void renderVoronoiDiagram();

    // Render points
    void renderPoints();

    // Get index of the nearest point
    int getNearestPointIndex(int x, int y) const;

    // List of points
    std::vector<Point> points;

    // Drawing buffer
    M5Canvas& screenBuffer;

    // Mutex for drawing
    SemaphoreHandle_t drawMutex;

    // Maximum number of points
    static constexpr std::size_t MAX_POINT_COUNT = 16U;

    // Repulsion force parameters
    static constexpr float REPULSION_STRENGTH = 15000.0F;
    static constexpr float REPULSION_RADIUS = 150.0F;

    // Color palette (20 pastel colors) - RGB565 format
    static constexpr uint16_t COLOR_PALETTE[20] = {
        0xED79, // RGB(238, 175, 206)
        0xFDB8, // RGB(251, 180, 196)
        0xFDB6, // RGB(250, 182, 181)
        0xFE76, // RGB(253, 205, 183)
        0xFED6, // RGB(251, 216, 176)
        0xFF35, // RGB(254, 230, 170)
        0xFF95, // RGB(252, 241, 175)
        0xFFF6, // RGB(254, 255, 179)
        0xEFD6, // RGB(238, 250, 178)
        0xE7F6, // RGB(230, 245, 176)
        0xDFB8, // RGB(217, 246, 192)
        0xCF58, // RGB(204, 234, 196)
        0xC759, // RGB(192, 235, 205)
        0xB71B, // RGB(179, 226, 216)
        0xB6FB, // RGB(180, 221, 223)
        0xB6BB, // RGB(180, 215, 221)
        0xB69C, // RGB(181, 210, 224)
        0xB67C, // RGB(179, 206, 227)
        0xB61B, // RGB(180, 194, 221)
        0xB5BB  // RGB(178, 182, 217)
    };
};
