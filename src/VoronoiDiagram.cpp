#include "VoronoiDiagram.h"
#include <esp_random.h>
#include <algorithm>
#include <esp_log.h>

// Custom clamp function (since std::clamp requires C++17)
template<typename T>
T clamp(const T& value, const T& min, const T& max) {
    return (value < min) ? min : ((value > max) ? max : value);
}

static const char* TAG = "VoronoiDiagram";

// Define color palette
const uint16_t VoronoiDiagram::COLOR_PALETTE[20] = {
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
    
    // Get screen dimensions
    screenWidth = M5.Display.width();
    screenHeight = M5.Display.height();
    
    // Probe the canvas pixel values of the palette colors, so the
    // renderer can write pixels directly into the canvas buffer
    initPaletteCanvasColors();
}

// Destructor
VoronoiDiagram::~VoronoiDiagram() {
}

// Probe the canvas pixel values of the palette colors
void VoronoiDiagram::initPaletteCanvasColors() {
    paletteCanvasColorsReady = false;
    
    uint8_t* canvasData = reinterpret_cast<uint8_t*>(screenBuffer.getBuffer());
    if (!canvasData) {
        ESP_LOGW(TAG, "Canvas buffer unavailable, using drawPixel rendering");
        return;
    }
    
    // drawPixel() converts an RGB565 color into the canvas pixel format.
    // Draw one pixel per palette color and read the stored values back,
    // so the direct buffer writes below produce exactly the same pixels
    // without depending on the details of the 8-bit pixel format.
    // (The probed pixels are overwritten by the first full rendering.)
    for (size_t i = 0; i < PALETTE_COLOR_COUNT; ++i) {
        screenBuffer.drawPixel(static_cast<int>(i), 0, COLOR_PALETTE[i]);
        paletteCanvasColors[i] = canvasData[i];
    }
    paletteCanvasColorsReady = true;
}

// Add a point
void VoronoiDiagram::addPoint(int x, int y) {
    // Adjust coordinates if outside screen
    x = clamp(x, 0, (int)M5.Display.width());
    y = clamp(y, 0, (int)M5.Display.height());

    // Randomly select a color from the palette
    const size_t paletteIndex = esp_random() % PALETTE_COLOR_COUNT;
    const uint16_t color = COLOR_PALETTE[paletteIndex];
    const uint8_t canvasColor = paletteCanvasColorsReady ? paletteCanvasColors[paletteIndex] : 0;

    // Hold the lock for the whole update: the draw task reads the point
    // list, so it must not change while being rendered
    MutexLock lock(drawMutex);
    if (!lock.isLocked()) {
        return;
    }

    // Use circular buffer approach for better performance
    if (points.size() < MAX_POINT_COUNT) {
        // Add new point if not at capacity
        points.push_back({x, y, color, canvasColor});
    } else {
        // Overwrite oldest point (circular buffer)
        points[currentPointIndex] = {x, y, color, canvasColor};
        currentPointIndex = (currentPointIndex + 1) % MAX_POINT_COUNT;
    }

    // The point set has changed
    needsRedraw = true;

    // Draw a white circle at the point position
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
    const bool moved = applyRepulsiveForce();

    // Skip frames in which nothing changed: the displayed image would
    // be identical, so rendering and transferring it again is wasted work
    if (!moved && !needsRedraw) {
        return;
    }

    // Draw Voronoi diagram
    renderVoronoiDiagram();
    
    // Draw points
    renderPoints();

    // Push off-screen buffer to display
    screenBuffer.pushSprite(&M5.Display, 0, 0);

    needsRedraw = false;
}

// Render Voronoi diagram: assign each pixel the color of its nearest point
void VoronoiDiagram::renderVoronoiDiagram() {
    uint8_t* canvasData = paletteCanvasColorsReady
        ? reinterpret_cast<uint8_t*>(screenBuffer.getBuffer())
        : nullptr;
    
    // Fallback to drawPixel if direct buffer access is unavailable
    if (!canvasData) {
        for (int y = 0; y < screenHeight; ++y) {
            for (int x = 0; x < screenWidth; ++x) {
                int nearestIndex = getNearestPointIndex(x, y);
                if (nearestIndex != -1) {
                    screenBuffer.drawPixel(x, y, points[nearestIndex].color);
                }
            }
        }
        return;
    }
    
    // Copy the point data into local arrays (at most MAX_POINT_COUNT
    // entries, always cache resident)
    const size_t count = points.size();
    int pointX[MAX_POINT_COUNT];
    int pointY[MAX_POINT_COUNT];
    uint8_t pointColor[MAX_POINT_COUNT];
    for (size_t i = 0; i < count; ++i) {
        pointX[i] = points[i].x;
        pointY[i] = points[i].y;
        pointColor[i] = points[i].canvasColor;
    }
    
    // For each pixel, find the nearest point and write its color byte
    // directly into the canvas buffer
    for (int y = 0; y < screenHeight; ++y) {
        // The vertical distance to each point is constant within a row
        int distYSquared[MAX_POINT_COUNT];
        for (size_t i = 0; i < count; ++i) {
            const int dy = y - pointY[i];
            distYSquared[i] = dy * dy;
        }
        
        uint8_t* row = canvasData + y * screenWidth;
        for (int x = 0; x < screenWidth; ++x) {
            int nearestIndex = 0;
            int nearestDistSquared = INT_MAX;
            for (size_t i = 0; i < count; ++i) {
                const int dx = x - pointX[i];
                const int distSquared = dx * dx + distYSquared[i];
                if (distSquared < nearestDistSquared) {
                    nearestDistSquared = distSquared;
                    nearestIndex = static_cast<int>(i);
                }
            }
            row[x] = pointColor[nearestIndex];
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

// Apply repulsive force to move points (returns true if any point moved)
bool VoronoiDiagram::applyRepulsiveForce() {
    const size_t numPoints = points.size();
    const float radiusSquared = REPULSION_RADIUS * REPULSION_RADIUS;
    
    // Clear the force accumulators (fixed-size member arrays, no allocation)
    for (size_t i = 0; i < numPoints; ++i) {
        forceX[i] = 0.0f;
        forceY[i] = 0.0f;
    }
    
    // Calculate forces for each point
    for (size_t i = 0; i < numPoints; ++i) {
        for (size_t j = i + 1; j < numPoints; ++j) {
            // Calculate distance and direction between points
            const int dx = points[i].x - points[j].x;
            const int dy = points[i].y - points[j].y;
            const float distSquared = dx * dx + dy * dy;
            
            // Apply repulsive force if within certain radius
            if (distSquared > 0 && distSquared < radiusSquared) {
                // Optimized force calculation: avoid extra division
                // Original: force * (dx/dist) = (STRENGTH/distSquared) * (dx/dist) 
                //         = STRENGTH * dx / (distSquared * dist)
                //         = STRENGTH * dx / dist^3
                // dist^3 = distSquared * sqrt(distSquared)
                const float distCubed = distSquared * sqrtf(distSquared);
                const float forceFactor = REPULSION_STRENGTH / distCubed;
                
                const float fx = forceFactor * dx;
                const float fy = forceFactor * dy;
                
                // Apply force to both points (action-reaction)
                forceX[i] += fx;
                forceY[i] += fy;
                forceX[j] -= fx;
                forceY[j] -= fy;
            }
        }
    }
    
    // Apply calculated forces to move points, tracking whether any
    // point actually changed its (integer) position
    bool moved = false;
    for (size_t i = 0; i < numPoints; ++i) {
        const int newX = clamp((int)(points[i].x + forceX[i]), 0, screenWidth);
        const int newY = clamp((int)(points[i].y + forceY[i]), 0, screenHeight);
        if (newX != points[i].x || newY != points[i].y) {
            points[i].x = newX;
            points[i].y = newY;
            moved = true;
        }
    }
    return moved;
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
