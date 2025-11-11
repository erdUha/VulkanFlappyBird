#include "cfg.hxx"
#include <iostream>
#include <fstream>

#define STB_IMAGE_IMPLEMENTATION
#include "lib/stb_image.h"
#ifdef IS_WINDOWS
	#define VK_USE_PLATFORM_WIN32_KHR
	#define GLFW_EXPOSE_NATIVE_WIN32
  #include <io.h>
  #include <process.h>
  #include <windows.h>
#else
  #include <unistd.h>
#endif
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#ifdef __APPLE__
  #include <MoltenVK/mvk_vulkan.h>
  #include <vulkan/vulkan_beta.h>
#else
  #include <vulkan/vk_platform.h>
#endif
#include <thread>
#include <array>
#include <vector>
#include <unordered_map>
#include <queue>
//#include <time.h>
#include <chrono>
#include <string>
#include <limits>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <cmath>
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/scalar_constants.hpp>

#include "lib/base64.hxx"
#include "lib/validationLayers.hxx"
#include "lib/3d.hxx"
#include "lib/physics.hxx"
#include "lib/vulkanInit.hxx"

int main()
{
  std::cout << "VulkanFlappyBird v"
    << VulkanFlappyBird_VERSION_MAJOR << "."
    << VulkanFlappyBird_VERSION_MINOR << "\n\n";
  //fetchObj();
	Run();

	return 0;
}
