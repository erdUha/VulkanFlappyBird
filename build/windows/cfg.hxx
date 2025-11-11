#define VulkanFlappyBird_VERSION_MAJOR 0
#define VulkanFlappyBird_VERSION_MINOR 1

#if (1 == 1)
  #define IS_WINDOWS
#endif

#if (0 == 1)
  #define NODEBUG
#endif

#define WIDTH 800
#define HEIGHT 600
#define VSYNC 1 // Default 1
#define MSAA_SAMPLES 4 // Default 8
#define PHYSICS_FPS 480 // Default 480
#define MAX_FPS_INIT 240 // Default 240
#if MAX_FPS_INIT == 0
  #define MAX_FPS 8192
#else
  #define MAX_FPS MAX_FPS_INIT
#endif
#define SHADOW_MAP_RESOLUTION 4096
#define MAX_FRAMES_IN_FLIGHT 2 // Just leave at 2

