#pragma once

#include <M5Unified.h>
#include <Arduino.h>
#include <vector>
#include <cmath>
#include <memory>
#include <algorithm>

// Class for managing Voronoi diagram
class VoronoiDiagram {
public:
    // Point structure
    struct Point {
        int x;
        int y;
        uint16_t color;
        uint8_t canvasColor;  // Color converted to the canvas pixel format
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
    // Maximum number of points
    static constexpr std::size_t MAX_POINT_COUNT = 16U;

    // Number of colors in the palette
    static constexpr std::size_t PALETTE_COLOR_COUNT = 20U;

    // Repulsion force parameters
    static constexpr float REPULSION_STRENGTH = 15000.0F;
    static constexpr float REPULSION_RADIUS = 150.0F;

    // Apply repulsive force to move points (returns true if any point moved)
    bool applyRepulsiveForce();

    // Render Voronoi diagram (nearest point per pixel)
    void renderVoronoiDiagram();
    
    // Probe the canvas pixel values of the palette colors for direct
    // buffer writes
    void initPaletteCanvasColors();
    
    // Draw points
    void renderPoints();

    // Get index of the nearest point (used as fallback)
    int getNearestPointIndex(int x, int y) const;

    // List of points
    std::vector<Point> points;
    
    // Current point index for circular overwrite
    size_t currentPointIndex = 0;

    // Drawing buffer
    M5Canvas& screenBuffer;

    // Mutex for drawing
    SemaphoreHandle_t drawMutex;
    
    // Screen dimensions
    int screenWidth = 0;
    int screenHeight = 0;

    // Redraw flag: set when the point set changes, cleared after drawing.
    // Frames in which no point moved and this flag is clear are skipped.
    bool needsRedraw = true;

    // Scratch arrays for the repulsion forces (fixed size, no allocation)
    float forceX[MAX_POINT_COUNT];
    float forceY[MAX_POINT_COUNT];

    // Palette colors converted to the canvas pixel format (probed at
    // construction time), indexed like COLOR_PALETTE
    uint8_t paletteCanvasColors[PALETTE_COLOR_COUNT];
    bool paletteCanvasColorsReady = false;

    // Color palette (20 pastel colors) - RGB565 format
    static const uint16_t COLOR_PALETTE[20];
};
