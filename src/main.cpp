#include <M5Unified.h>
#include <cmath>
#include <vector>
#include <esp_random.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Initial touch position (set to invalid coordinates)
m5::touch_detail_t initialTouchPosition = {};

// Dragging threshold and repulsion parameters
const int DRAG_THRESHOLD = 10;
const float REPULSION_STRENGTH = 15000.0F;
const float REPULSION_RADIUS = 150.0F;

// Tone settings for feedback sound
const uint8_t TONE_VOLUME = 90U;//48U;
const float TONE_FREQUENCY = 659.26F;
const uint32_t TONE_DURATION = 50U;

// Task handles for FreeRTOS tasks
TaskHandle_t mainTaskHandle = nullptr;
TaskHandle_t voronoiTaskHandle = nullptr;

// Mutex to protect the drawing process
SemaphoreHandle_t drawMutex = nullptr;

// Structure to represent a point with color
struct Point
{
  int x;
  int y;
  uint16_t color;
};

// Vector to store points and set a maximum limit
std::vector<Point> points;
const std::size_t MAX_POINT_COUNT = 16U;

// Off-screen buffer for drawing
M5Canvas screenBuffer;

// Color palette (20 pale tone colors)
constexpr uint16_t COLOR_PALETTE[] = {
    screenBuffer.color565(238, 175, 206),
    screenBuffer.color565(251, 180, 196),
    screenBuffer.color565(250, 182, 181),
    screenBuffer.color565(253, 205, 183),
    screenBuffer.color565(251, 216, 176),
    screenBuffer.color565(254, 230, 170),
    screenBuffer.color565(252, 241, 175),
    screenBuffer.color565(254, 255, 179),
    screenBuffer.color565(238, 250, 178),
    screenBuffer.color565(230, 245, 176),
    screenBuffer.color565(217, 246, 192),
    screenBuffer.color565(204, 234, 196),
    screenBuffer.color565(192, 235, 205),
    screenBuffer.color565(179, 226, 216),
    screenBuffer.color565(180, 221, 223),
    screenBuffer.color565(180, 215, 221),
    screenBuffer.color565(181, 210, 224),
    screenBuffer.color565(179, 206, 227),
    screenBuffer.color565(180, 194, 221),
    screenBuffer.color565(178, 182, 217)};

// Applies a repulsive force to keep points apart
void applyRepulsiveForce()
{
  for (size_t i = 0, size = points.size(); i < size; ++i)
  {
    float dx_total = 0;
    float dy_total = 0;
    for (size_t j = 0, size = points.size(); j < size; ++j)
    {
      if (i == j)
      {
        // Skip self Comparison
        continue;
      }

      // Calculate distance and direction between points
      int dx = points[i].x - points[j].x;
      int dy = points[i].y - points[j].y;
      float dist = sqrtf(dx * dx + dy * dy);

      // Apply repulsive force if within a certain radius
      if (dist > 0 && dist < REPULSION_RADIUS)
      {
        float force = REPULSION_STRENGTH / (dist * dist);
        dx_total += force * (dx / dist);
        dy_total += force * (dy / dist);
      }
    }

    // Keep points within screen bounds
    points[i].x = std::clamp((int)(points[i].x + dx_total), 0, (int)M5.Display.width());
    points[i].y = std::clamp((int)(points[i].y + dy_total), 0, (int)M5.Display.height());
  }
}

// Calculate the nearest point for each pixel
int getNearestPointIndex(int x, int y) {
  int nearestIndex = -1;
  int nearestDist = INT_MAX;

  for (size_t i = 0, size = points.size(); i < size; ++i) {
    int dx = x - points[i].x;
    int dy = y - points[i].y;
    int dist = dx * dx + dy * dy;

    if (dist < nearestDist) {
      nearestDist = dist;
      nearestIndex = i;
    }
  }

  return nearestIndex;
}

// Draws the Voronoi diagram
void drawVoronoiDiagram()
{
  // Lock the mutex to prevent other tasks from drawing
  if (xSemaphoreTake(drawMutex, portMAX_DELAY) == pdTRUE)
  {
    // Applies a repulsive force to keep points apart
    applyRepulsiveForce();

    // Calculate and draw each pixel based on the nearest point
    for (int y = 0; y < M5.Display.height(); ++y)
    {
      for (int x = 0; x < M5.Display.width(); ++x)
      {
        // Calculate the nearest point for each pixel
        int nearestIndex = getNearestPointIndex(x, y);

        // Draw the pixel with the color of the nearest point
        if (nearestIndex != -1)
        {
          screenBuffer.drawPixel(x, y, points[nearestIndex].color);
        }
      }
    }

    // Draw white circles at the point positions
    for (size_t i = 0, size = points.size(); i < size; ++i)
    {
      screenBuffer.fillCircle(points[i].x, points[i].y, 3, WHITE);
    }

    // Push the off-screen buffer to the display
    screenBuffer.pushSprite(&M5.Display, 0, 0);

    // Unlock the mutex after drawing
    xSemaphoreGive(drawMutex);
  }
}

// Initial setup function
void setup()
{
  auto cfg = M5.config();
  M5.begin(cfg);

  M5.Speaker.setVolume(TONE_VOLUME);

  M5.Display.setRotation(1);
  M5.Display.startWrite();

  screenBuffer.setPsram(true);
  screenBuffer.createSprite(M5.Display.width(), M5.Display.height());

  // Preallocate space for the points vector
  points.reserve(MAX_POINT_COUNT);
}

// Main loop to handle touch input and draw the Voronoi diagram
void handleTouchInput()
{
  M5.update();

  // Handle touch input
  if (M5.Touch.getCount())
  {
    m5::touch_detail_t pos = M5.Touch.getDetail(0);
    if (pos.x != -1 && pos.y != -1)
    {
      if (initialTouchPosition.x == -1)
      {
        initialTouchPosition = pos;
      }
    }
  }
  else
  {
    if (initialTouchPosition.x != -1)
    {
      // If the point count exceed limit, remove the first point
      if (points.size() >= MAX_POINT_COUNT)
      {
        points.erase(points.begin());
      }
      // Randomly select a color from the pallete
      uint16_t color = COLOR_PALETTE[esp_random() % 20];
      // Add the new point to the list
      points.push_back({initialTouchPosition.x, initialTouchPosition.y, color});

      // Lock the mutex to prevent other tasks from drawing
      if (xSemaphoreTake(drawMutex, portMAX_DELAY) == pdTRUE)
      {
        M5.Display.fillCircle(initialTouchPosition.x, initialTouchPosition.y, 3, WHITE);

        // Unlock the mutex after drawing
        xSemaphoreGive(drawMutex);
      }
      // Play a tone as feedback
      M5.Speaker.tone(TONE_FREQUENCY, TONE_DURATION);

      // Draw the Voronoi diagram
      drawVoronoiDiagram();

      // Reset the touch position
      initialTouchPosition = {};
    }
  }
}

// Task to draw the Voronoi diagram periodically
void voronoiTask(void *args)
{
  for (;;)
  {
    drawVoronoiDiagram();

    // Delay for 200ms
    vTaskDelay(pdMS_TO_TICKS(200));
  }
  vTaskDelete(voronoiTaskHandle);
}

// Initializes the Voronoi drawing task
void initializeVoronoiTask()
{
  xTaskCreatePinnedToCore(&voronoiTask, "task2-voronoi", 4096, nullptr, 1, &voronoiTaskHandle, 1);
  configASSERT(voronoiTaskHandle);
}

// Main task that runs the setup and input loop
void runMainTask(void *args)
{
  for (;;)
  {
    handleTouchInput();

    // Prevent watchdog timeout
    vTaskDelay(1);
  }
  vTaskDelete(mainTaskHandle);
}

// Initializes the main loop task
void initializeMainTask()
{
  xTaskCreatePinnedToCore(&runMainTask, "task1-main", 4096, nullptr, 2, &mainTaskHandle, 0);
  configASSERT(mainTaskHandle);
}

// Entry point for the application
extern "C" void app_main()
{
  setup();

  // Create the semaphore (mutex) to control drawing access
  drawMutex = xSemaphoreCreateMutex();
  // Ensure mutex was created successfully
  configASSERT(drawMutex);

  // Initialize tasks
  initializeMainTask();
  initializeVoronoiTask();
}
