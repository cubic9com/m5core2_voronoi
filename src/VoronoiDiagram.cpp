#include "VoronoiDiagram.h"
#include <esp_random.h>
#include <algorithm>

// Mutex lock class using RAII pattern
class MutexLock {
public:
    MutexLock(SemaphoreHandle_t mutex) : mutex_(mutex) {
        locked_ = (xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE);
    }
    
    ~MutexLock() {
        if (locked_) {
            xSemaphoreGive(mutex_);
        }
    }
    
    bool isLocked() const { return locked_; }
    
private:
    SemaphoreHandle_t mutex_;
    bool locked_;
};

// Constructor
VoronoiDiagram::VoronoiDiagram(M5Canvas& buffer, SemaphoreHandle_t mutex)
    : screenBuffer(buffer), drawMutex(mutex) {
    // Pre-allocate memory for point list
    points.reserve(MAX_POINT_COUNT);
}

// Add a point
void VoronoiDiagram::addPoint(int x, int y) {
    // Adjust coordinates if outside screen
    x = std::clamp(x, 0, (int)M5.Display.width());
    y = std::clamp(y, 0, (int)M5.Display.height());

    // If exceeding maximum number of points, remove the first point
    if (points.size() >= MAX_POINT_COUNT) {
        points.erase(points.begin());
    }

    // Randomly select a color from the palette
    const uint16_t color = COLOR_PALETTE[esp_random() % std::size(COLOR_PALETTE)];

    // Add new point to the list
    points.push_back({x, y, color});

    // Draw a white circle at the point position
    MutexLock lock(drawMutex);
    if (!lock.isLocked()) {
        return;
    }
    
    M5.Display.fillCircle(x, y, 3, WHITE);
}

// Draw Voronoi diagram
void VoronoiDiagram::draw() {
    // Do nothing if there are no points
    if (points.empty()) {
        return;
    }

    // Lock mutex to prevent other tasks from drawing
    MutexLock lock(drawMutex);
    if (!lock.isLocked()) {
        return;
    }

    // Apply repulsive force to move points
    applyRepulsiveForce();

    // Draw Voronoi diagram
    renderVoronoiDiagram();
    
    // Draw points
    renderPoints();

    // Push off-screen buffer to display
    screenBuffer.pushSprite(&M5.Display, 0, 0);
}

// Render Voronoi diagram
void VoronoiDiagram::renderVoronoiDiagram() {
    const int width = M5.Display.width();
    const int height = M5.Display.height();

    // Calculate and draw color for each pixel based on the nearest point
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            // Calculate the nearest point for each pixel
            int nearestIndex = getNearestPointIndex(x, y);

            // Draw pixel with the color of the nearest point
            if (nearestIndex != -1) {
                screenBuffer.drawPixel(x, y, points[nearestIndex].color);
            }
        }
    }
}

// Draw points
void VoronoiDiagram::renderPoints() {
    const size_t numPoints = points.size();
    
    // Draw white circles at point positions
    for (size_t i = 0; i < numPoints; ++i) {
        screenBuffer.fillCircle(points[i].x, points[i].y, 3, WHITE);
    }
}

// Apply repulsive force to move points
void VoronoiDiagram::applyRepulsiveForce() {
    const size_t numPoints = points.size();
    const float radiusSquared = REPULSION_RADIUS * REPULSION_RADIUS;
    const int displayWidth = M5.Display.width();
    const int displayHeight = M5.Display.height();
    
    // Calculate forces for each point
    std::vector<std::pair<float, float>> forces(numPoints, {0.0f, 0.0f});
    
    for (size_t i = 0; i < numPoints; ++i) {
        for (size_t j = i + 1; j < numPoints; ++j) {
            // Calculate distance and direction between points
            int dx = points[i].x - points[j].x;
            int dy = points[i].y - points[j].y;
            float distSquared = dx * dx + dy * dy;
            
            // Apply repulsive force if within certain radius
            if (distSquared > 0 && distSquared < radiusSquared) {
                float dist = sqrtf(distSquared); // Calculate square root only here
                float force = REPULSION_STRENGTH / distSquared; // Divide by square of distance
                
                float fx = force * (dx / dist);
                float fy = force * (dy / dist);
                
                // Apply force to both points (action-reaction)
                forces[i].first += fx;
                forces[i].second += fy;
                forces[j].first -= fx;
                forces[j].second -= fy;
            }
        }
    }
    
    // Apply calculated forces to move points
    for (size_t i = 0; i < numPoints; ++i) {
        points[i].x = std::clamp((int)(points[i].x + forces[i].first), 0, displayWidth);
        points[i].y = std::clamp((int)(points[i].y + forces[i].second), 0, displayHeight);
    }
}

// Get index of the nearest point
int VoronoiDiagram::getNearestPointIndex(int x, int y) const {
    if (points.empty()) {
        return -1;
    }
    
    int nearestIndex = 0;
    int nearestDistSquared = INT_MAX;
    const size_t numPoints = points.size();

    for (size_t i = 0; i < numPoints; ++i) {
    // Calculate squared Euclidean distance (avoid square root calculation)
        int dx = x - points[i].x;
        int dy = y - points[i].y;
        int distSquared = dx * dx + dy * dy;

        if (distSquared < nearestDistSquared) {
            nearestDistSquared = distSquared;
            nearestIndex = i;
        }
    }

    return nearestIndex;
}
