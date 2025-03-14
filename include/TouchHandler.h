#pragma once

#include <M5Unified.h>
#include "VoronoiDiagram.h"
#include "SoundManager.h"

// Class for handling touch input
class TouchHandler {
public:
    // Constructor
    TouchHandler(VoronoiDiagram& voronoi, SoundManager& sound);

    // Process touch input
    void handleInput();

private:
    // Voronoi diagram
    VoronoiDiagram& voronoiDiagram;
    
    // Sound manager
    SoundManager& soundManager;

    // Initial touch position (set to invalid coordinates)
    m5::touch_detail_t initialTouchPosition = {};

    // Drag threshold
    static constexpr int DRAG_THRESHOLD = 10;
};
