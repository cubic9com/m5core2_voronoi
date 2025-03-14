#include "TouchHandler.h"

// Constructor
TouchHandler::TouchHandler(VoronoiDiagram& voronoi, SoundManager& sound)
    : voronoiDiagram(voronoi), soundManager(sound) {
}

// Process touch input
void TouchHandler::handleInput() {
    // Update M5Stack state
    M5.update();

    // Check for touch input
    const uint8_t touchCount = M5.Touch.getCount();
    
    if (touchCount > 0) {
        // Get touch position (using first touch point)
        m5::touch_detail_t pos = M5.Touch.getDetail(0);
        
        // If valid coordinates (-1, -1 are invalid coordinates)
        const bool isValidPosition = (pos.x != -1 && pos.y != -1);
        
        if (isValidPosition) {
            // Set initial touch position if not set
            const bool isInitialTouchNotSet = (initialTouchPosition.x == -1);
            
            if (isInitialTouchNotSet) {
                initialTouchPosition = pos;
            }
        }
    } else {
        // When touch ends
        const bool hasPreviousTouch = (initialTouchPosition.x != -1);
        
        if (hasPreviousTouch) {
            // Add point
            voronoiDiagram.addPoint(initialTouchPosition.x, initialTouchPosition.y);

            // Play feedback sound
            soundManager.playSound(SoundManager::SoundType::TOUCH);

            // Draw Voronoi diagram
            voronoiDiagram.draw();

            // Reset touch position (set to invalid coordinates)
            initialTouchPosition = {};
        }
    }
}
