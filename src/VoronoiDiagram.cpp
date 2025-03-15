#include "VoronoiDiagram.h"
#include <esp_random.h>
#include <algorithm>
#include <esp_log.h>

static const char* TAG = "VoronoiDiagram";

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

// Initialize JFA buffers in PSRAM
void VoronoiDiagram::initJFABuffers() {
    // Free existing buffers if any
    freeJFABuffers();
    
    // Allocate buffers in PSRAM
    jfaBufferA = (SeedPoint*)heap_caps_malloc(screenSize * sizeof(SeedPoint), MALLOC_CAP_SPIRAM);
    jfaBufferB = (SeedPoint*)heap_caps_malloc(screenSize * sizeof(SeedPoint), MALLOC_CAP_SPIRAM);
    
    if (!jfaBufferA || !jfaBufferB) {
        ESP_LOGE(TAG, "Failed to allocate JFA buffers in PSRAM");
        // Fallback to regular memory if PSRAM allocation fails
        freeJFABuffers();
        jfaBufferA = (SeedPoint*)malloc(screenSize * sizeof(SeedPoint));
        jfaBufferB = (SeedPoint*)malloc(screenSize * sizeof(SeedPoint));
        
        if (!jfaBufferA || !jfaBufferB) {
            ESP_LOGE(TAG, "Failed to allocate JFA buffers in regular memory");
        }
    }
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
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int idx = y * width + x;
            jfaBufferA[idx].x = -1;
            jfaBufferA[idx].y = -1;
            jfaBufferA[idx].idx = -1;
        }
    }
    
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
            for (int x = 0; x < width; ++x) {
                const int idx = y * width + x;
                const SeedPoint& current = srcBuffer[idx];
                
                // Copy current value to destination buffer
                dstBuffer[idx] = current;
                
                // Check 8 neighboring pixels at distance 'step'
                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        const int nx = x + dx * step;
                        const int ny = y + dy * step;
                        
                        // Skip if outside screen
                        if (nx < 0 || nx >= width || ny < 0 || ny >= height) {
                            continue;
                        }
                        
                        const int nidx = ny * width + nx;
                        const SeedPoint& neighbor = srcBuffer[nidx];
                        
                        // Skip if neighbor has no seed point
                        if (neighbor.idx < 0) {
                            continue;
                        }
                        
                        // Calculate distance to neighbor's seed point
                        int dx1 = x - neighbor.x;
                        int dy1 = y - neighbor.y;
                        int distSquared1 = dx1 * dx1 + dy1 * dy1;
                        
                        // Calculate distance to current seed point (if any)
                        int distSquared2 = INT_MAX;
                        if (dstBuffer[idx].idx >= 0) {
                            int dx2 = x - dstBuffer[idx].x;
                            int dy2 = y - dstBuffer[idx].y;
                            distSquared2 = dx2 * dx2 + dy2 * dy2;
                        }
                        
                        // Update if neighbor's seed point is closer
                        if (distSquared1 < distSquared2) {
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
