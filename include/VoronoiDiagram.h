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

    // Constructor (the two canvases form a double buffer: while one is
    // being transferred to the display, the next frame is rendered into
    // the other one)
    VoronoiDiagram(M5Canvas& bufferA, M5Canvas& bufferB, SemaphoreHandle_t mutex);

    // Destructor
    ~VoronoiDiagram();

    // Add a point
    void addPoint(int x, int y);

    // Render one frame (if anything changed) and hand it to the push task
    void draw();

    // Push task body (never returns): waits for rendered frames and
    // transfers them to the display
    void pushTaskLoop();

    // Render worker task body (never returns): renders the bottom half of
    // a frame on the other core when asked by draw()
    void renderWorkerLoop();

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

    // Render rows [y0, y1) of the diagram into a canvas buffer: assign
    // each pixel the color of its nearest point (from snapshot arrays)
    void renderRows(uint8_t* canvasData, const int* pointX, const int* pointY,
                    const uint8_t* pointColor, std::size_t count, int y0, int y1);

    // Probe the canvas pixel values of the palette colors for direct
    // buffer writes
    void initPaletteCanvasColors();

    // Draw points onto the given canvas (positions from the frame
    // snapshot taken by draw())
    void renderPoints(M5Canvas& target, const int* pointX, const int* pointY,
                      std::size_t count);

    // Get index of the nearest point (used as fallback)
    int getNearestPointIndex(int x, int y) const;

    // List of points
    std::vector<Point> points;

    // Current point index for circular overwrite
    size_t currentPointIndex = 0;

    // Drawing buffers (double buffer)
    M5Canvas* canvases[2];

    // Index of the canvas the next frame is rendered into
    int backIndex = 0;

    // Mutex for the point list (passed in from outside)
    SemaphoreHandle_t drawMutex;

    // Serializes the render pipeline: draw() can be called from both the
    // draw task and the touch task
    SemaphoreHandle_t frameMutex = nullptr;

    // Serializes direct display writes (the push task's transfers and the
    // touch feedback dot drawn by addPoint())
    SemaphoreHandle_t displayMutex = nullptr;

    // One binary semaphore per canvas: given when the canvas is free to
    // render into, taken by draw() before rendering into it
    SemaphoreHandle_t canvasFree[2] = {nullptr, nullptr};

    // Queue of canvas indices holding a rendered frame that waits to be
    // transferred (FIFO, so frames are shown in rendering order)
    QueueHandle_t pushQueue = nullptr;

    // Render worker synchronization: draw() gives halfStart to ask the
    // worker to render rows [workerY0, workerY1) of the current frame,
    // and waits for halfDone before using the result
    SemaphoreHandle_t halfStart = nullptr;
    SemaphoreHandle_t halfDone = nullptr;

    // Current frame parameters shared with the render worker. Written by
    // draw() before giving halfStart and stable until halfDone is taken,
    // because the frame mutex allows only one draw() at a time and the
    // worker only touches them between halfStart and halfDone
    uint8_t* workerBuffer = nullptr;
    const int* workerPointX = nullptr;
    const int* workerPointY = nullptr;
    const uint8_t* workerPointColor = nullptr;
    std::size_t workerCount = 0;
    int workerY0 = 0;
    int workerY1 = 0;

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
