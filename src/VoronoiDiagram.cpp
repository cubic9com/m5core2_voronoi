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
    screenSize = screenWidth * screenHeight;
    
    // Initialize JFA buffers in PSRAM
    initJFABuffers();
}

// Destructor
VoronoiDiagram::~VoronoiDiagram() {
    // Free JFA buffers
    freeJFABuffers();
}

// Initialize JFA buffers in internal SRAM
void VoronoiDiagram::initJFABuffers() {
    // Free existing buffers if any
    freeJFABuffers();
    
    // Try to allocate buffers in internal SRAM (DMA capable memory for faster access)
    jfaBufferA = (SeedPoint*)heap_caps_malloc(screenSize * sizeof(SeedPoint), MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    jfaBufferB = (SeedPoint*)heap_caps_malloc(screenSize * sizeof(SeedPoint), MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
}

// Free JFA buffers
void VoronoiDiagram::freeJFABuffers() {
    if (jfaBufferA) {
        free(jfaBufferA);
        jfaBufferA = nullptr;
    }
    
    if (jfaBufferB) {
        free(jfaBufferB);
        jfaBufferB = nullptr;
    }
}

// Add a point
void VoronoiDiagram::addPoint(int x, int y) {
    // Adjust coordinates if outside screen
    x = clamp(x, 0, (int)M5.Display.width());
    y = clamp(y, 0, (int)M5.Display.height());

    // Randomly select a color from the palette
    const uint16_t color = COLOR_PALETTE[esp_random() % (sizeof(COLOR_PALETTE) / sizeof(COLOR_PALETTE[0]))];

    // Use circular buffer approach for better performance
    if (points.size() < MAX_POINT_COUNT) {
        // Add new point if not at capacity
        points.push_back({x, y, color});
    } else {
        // Overwrite oldest point (circular buffer)
        points[currentPointIndex] = {x, y, color};
        currentPointIndex = (currentPointIndex + 1) % MAX_POINT_COUNT;
    }

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

// Render Voronoi diagram using Jump Flooding Algorithm
void VoronoiDiagram::renderVoronoiDiagram() {
    // Check if buffers are available
    if (!jfaBufferA || !jfaBufferB || points.empty()) {
        // Fallback to traditional method if buffers are not available
        const int width = screenWidth;
        const int height = screenHeight;

        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                int nearestIndex = getNearestPointIndex(x, y);
                if (nearestIndex != -1) {
                    screenBuffer.drawPixel(x, y, points[nearestIndex].color);
                }
            }
        }
        return;
    }

    // Execute Jump Flooding Algorithm
    executeJFA();

    // Render the result to the screen buffer
    for (int y = 0; y < screenHeight; ++y) {
        for (int x = 0; x < screenWidth; ++x) {
            const int idx = y * screenWidth + x;
            const int pointIdx = jfaBufferA[idx].idx;
            
            if (pointIdx >= 0 && pointIdx < static_cast<int>(points.size())) {
                screenBuffer.drawPixel(x, y, points[pointIdx].color);
            }
        }
    }
}

// Execute Jump Flooding Algorithm
void VoronoiDiagram::executeJFA() {
    const int width = screenWidth;
    const int height = screenHeight;
    const size_t numPoints = points.size();
    
    // Initialize buffers
    const size_t bufferSize = screenSize * sizeof(SeedPoint);
    memset(jfaBufferA, -1, bufferSize);
    
    // Set seed points
    for (size_t i = 0; i < numPoints; ++i) {
        const int x = points[i].x;
        const int y = points[i].y;
        
        if (x >= 0 && x < width && y >= 0 && y < height) {
            const int idx = y * width + x;
            jfaBufferA[idx].x = x;
            jfaBufferA[idx].y = y;
            jfaBufferA[idx].idx = i;
        }
    }
    
    // Jump flooding steps
    SeedPoint* srcBuffer = jfaBufferA;
    SeedPoint* dstBuffer = jfaBufferB;
    
    // Start with step size = width/2 and reduce by half each iteration
    for (int step = width / 2; step > 0; step /= 2) {
        // For each pixel
        for (int y = 0; y < height; ++y) {
            const int rowOffset = y * width;
            
            for (int x = 0; x < width; ++x) {
                const int idx = rowOffset + x;
                
                // Copy current value to destination buffer
                dstBuffer[idx] = srcBuffer[idx];
                
                // Get current best distance (if any)
                int bestDistSquared = INT_MAX;
                if (dstBuffer[idx].idx >= 0) {
                    const int dx = x - dstBuffer[idx].x;
                    const int dy = y - dstBuffer[idx].y;
                    bestDistSquared = dx * dx + dy * dy;
                }
                
                // Check 8 neighboring pixels at distance 'step'
                for (int dy = -1; dy <= 1; ++dy) {
                    const int ny = y + dy * step;
                    
                    // Skip if outside screen
                    if (ny < 0 || ny >= height) {
                        continue;
                    }
                    
                    const int neighborRowOffset = ny * width;
                    
                    for (int dx = -1; dx <= 1; ++dx) {
                        const int nx = x + dx * step;
                        
                        // Skip if outside screen
                        if (nx < 0 || nx >= width) {
                            continue;
                        }
                        
                        const int nidx = neighborRowOffset + nx;
                        const SeedPoint& neighbor = srcBuffer[nidx];
                        
                        // Skip if neighbor has no seed point
                        if (neighbor.idx < 0) {
                            continue;
                        }
                        
                        // Calculate distance to neighbor's seed point
                        const int dx1 = x - neighbor.x;
                        const int dy1 = y - neighbor.y;
                        const int distSquared = dx1 * dx1 + dy1 * dy1;
                        
                        // Update if neighbor's seed point is closer
                        if (distSquared < bestDistSquared) {
                            bestDistSquared = distSquared;
                            dstBuffer[idx] = neighbor;
                        }
                    }
                }
            }
        }
        
        // Swap buffers for next iteration
        std::swap(srcBuffer, dstBuffer);
    }
    
    // Ensure final result is in jfaBufferA
    if (srcBuffer != jfaBufferA) {
        memcpy(jfaBufferA, srcBuffer, screenSize * sizeof(SeedPoint));
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
                forces[i].first += fx;
                forces[i].second += fy;
                forces[j].first -= fx;
                forces[j].second -= fy;
            }
        }
    }
    
    // Apply calculated forces to move points
    for (size_t i = 0; i < numPoints; ++i) {
        points[i].x = clamp((int)(points[i].x + forces[i].first), 0, displayWidth);
        points[i].y = clamp((int)(points[i].y + forces[i].second), 0, displayHeight);
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
