#include <vector>
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <vulkan/vulkan.h>
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/scalar_constants.hpp>

GLFWwindow* window;
VkInstance instance;
VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
VkDevice device;
VkQueue graphicsQueue;
VkQueue presentQueue;
VkSurfaceKHR surface;
#define DEVICE_EXTENSION_COUNT 1
const char* deviceExtensions[DEVICE_EXTENSION_COUNT] = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

VkSwapchainKHR swapChain;

// glm stuff
struct SceneUBO
{
  alignas(16) glm::mat4 model;
	alignas(16) glm::mat4 view;
	alignas(16) glm::mat4 proj;
	alignas(16) glm::mat4 normalMatrix;
	alignas(16) glm::mat4 normalViewMatrix;
	alignas(16) glm::mat4 lightSpaceMatrix;
  alignas(16) glm::vec3 materialSpecular; 
	alignas(16) glm::vec3 lightDir;
	alignas(16) glm::vec3 viewPos;
  alignas(16) glm::vec3 shadowMapResolution;
  alignas(16) glm::vec3 biasFactor;
};

struct ShadowUBO
{
	alignas(16) glm::mat4 lightSpaceMatrix;
};

glm::vec3 lightPos;
glm::vec3 lightDirection;
glm::mat4 sharedLightProjViewMatrix;
float biasFactor;

VkDescriptorPool shadowMapDescriptorPool;
VkDescriptorPool descriptorPool;

// swap chain stuff
uint32_t swapChainImageCount = 0;
VkImage* swapChainImages;
void cleanSwapChainImages() { free(swapChainImages); }

VkFormat swapChainImageFormat;
VkExtent2D swapChainExtent;

uint32_t swapChainImageViewCount = 0;
VkImageView* swapChainImageViews;

// shadow map stuff
VkImage shadowMapImage;
VkImageView shadowMapImageView;
VkDeviceMemory shadowMapImageMemory;
VkSampler shadowMapSampler;

VkFormat shadowMapImageFormat;
VkExtent2D shadowMapExtent = {
  SHADOW_MAP_RESOLUTION,
  SHADOW_MAP_RESOLUTION
};

// render pass
VkRenderPass renderPass;
VkRenderPass shadowMapRenderPass;
VkDescriptorSetLayout shadowMapDescriptorSetLayout;
VkDescriptorSetLayout descriptorSetLayout;
VkPipelineLayout objectPipelineLayout;
VkPipeline objectGraphicsPipeline;
VkPipelineLayout shadowMapPipelineLayout;
VkPipeline shadowMapGraphicsPipeline;

uint32_t swapChainFramebufferCount = 0;
VkFramebuffer* swapChainFramebuffers;

VkFramebuffer shadowMapFramebuffer;

VkCommandPool commandPool;

uint32_t commandBufferCount = MAX_FRAMES_IN_FLIGHT;
VkCommandBuffer commandBuffers[MAX_FRAMES_IN_FLIGHT] = {};

uint32_t imageAvailableSemaphoreCount = MAX_FRAMES_IN_FLIGHT;
VkSemaphore imageAvailableSemaphores[MAX_FRAMES_IN_FLIGHT] = {};

uint32_t renderFinishedSemaphoreCount = MAX_FRAMES_IN_FLIGHT;
VkSemaphore renderFinishedSemaphores[MAX_FRAMES_IN_FLIGHT] = {};

uint32_t inFlightFenceCount = MAX_FRAMES_IN_FLIGHT;
VkFence inFlightFences[MAX_FRAMES_IN_FLIGHT] = {};

bool framebufferResized = false;
static void framebufferResizeCallback();

uint32_t currentFrame = 0;

uint32_t mipLevels;

VkImage textureImage;
VkDeviceMemory textureImageMemory;

VkImageView textureImageView;
VkSampler textureSampler;

VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT;
VkImage colorImage;
VkDeviceMemory colorImageMemory;
VkImageView colorImageView;

VkSampleCountFlagBits getMaxUsableSampleCount()
{
  VkPhysicalDeviceProperties physicalDeviceProperties;
  vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);

  VkSampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts & physicalDeviceProperties.limits.framebufferDepthSampleCounts;
  //if (counts & VK_SAMPLE_COUNT_64_BIT) { return VK_SAMPLE_COUNT_64_BIT; }
  //if (counts & VK_SAMPLE_COUNT_32_BIT) { return VK_SAMPLE_COUNT_32_BIT; }
  //if (counts & VK_SAMPLE_COUNT_16_BIT) { return VK_SAMPLE_COUNT_16_BIT; }
  if (counts & VK_SAMPLE_COUNT_8_BIT && MSAA_SAMPLES >= 8) { return VK_SAMPLE_COUNT_8_BIT; }
  if (counts & VK_SAMPLE_COUNT_4_BIT && MSAA_SAMPLES >= 4) { return VK_SAMPLE_COUNT_4_BIT; }
  if (counts & VK_SAMPLE_COUNT_2_BIT && MSAA_SAMPLES >= 2) { return VK_SAMPLE_COUNT_2_BIT; }

  return VK_SAMPLE_COUNT_1_BIT;
}

VkImage depthImage;
VkDeviceMemory depthImageMemory;
VkImageView depthImageView;

void cleanResources()
{
  vkDestroyImageView(device, colorImageView, NULL);
  vkDestroyImage(device, colorImage, NULL);
  vkFreeMemory(device, colorImageMemory, NULL);

  //vkDestroyImageView(device, shadowMapImageView, NULL);
  //vkDestroyImage(device, shadowMapImage, NULL);
  //vkFreeMemory(device, shadowMapImageMemory, NULL);

  vkDestroyImageView(device, depthImageView, NULL);
  vkDestroyImage(device, depthImage, NULL);
  vkFreeMemory(device, depthImageMemory, NULL);
}

struct SwapChainSupportDetails
{
	VkSurfaceCapabilitiesKHR capabilities;
	uint32_t formatCount;
	uint32_t presentModeCount;
	VkSurfaceFormatKHR* formats;
	VkPresentModeKHR* presentModes;
} swapChainSupport;

void querySwapChainSupport(VkPhysicalDevice device, struct SwapChainSupportDetails *details)
{
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details->capabilities);

	uint32_t formatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

	details->formatCount = formatCount;

	if (formatCount != 0)
	{
		details->formats = (VkSurfaceFormatKHR*)calloc(formatCount, sizeof(VkSurfaceFormatKHR));
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details->formats);
	}

	uint32_t presentModeCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

	details->presentModeCount = presentModeCount;

	if (presentModeCount != 0)
	{
		details->presentModes = (VkPresentModeKHR*)calloc(presentModeCount, sizeof(VkPresentModeKHR));
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details->presentModes);
	}
}

VkSurfaceFormatKHR chooseSwapSurfaceFormat(VkSurfaceFormatKHR* availableFormats, uint32_t formatCount)
{
	for (uint32_t i = 0; i < formatCount; i++)
	{
		if (availableFormats[i].format == VK_FORMAT_B8G8R8A8_SRGB && 
				availableFormats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
		{
			return availableFormats[i];
		}
	}

	return availableFormats[0];
}

VkPresentModeKHR chooseSwapPresentMode(VkPresentModeKHR* availablePresentModes, uint32_t presentModeCount)
{
	if (VSYNC == 1)
	{
		for (uint32_t i = 0; i < presentModeCount; i++)
		{
			if (availablePresentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
			{
				return availablePresentModes[i];
			}
		}
		return VK_PRESENT_MODE_FIFO_KHR;
	}
	else 
	{
		return VK_PRESENT_MODE_IMMEDIATE_KHR;
	}
}

VkExtent2D chooseSwapExtent(VkSurfaceCapabilitiesKHR capabilities)
{
	if (capabilities.currentExtent.width != UINT32_MAX)
	{
		return capabilities.currentExtent;
	}
	else
	{
		int width, height;
		glfwGetFramebufferSize(window, &width, &height);

		if (static_cast<uint32_t>(width) < capabilities.minImageExtent.width) { width = capabilities.minImageExtent.width; }
		if (static_cast<uint32_t>(height) < capabilities.minImageExtent.height) { height = capabilities.minImageExtent.height; }

		if (static_cast<uint32_t>(width) > capabilities.maxImageExtent.width) { width = capabilities.maxImageExtent.width; }
		if (static_cast<uint32_t>(height) > capabilities.maxImageExtent.height) { height = capabilities.maxImageExtent.height; }

		VkExtent2D actualExtent = {
			static_cast<uint32_t>(width),
			static_cast<uint32_t>(height)
		};

		return actualExtent;
	}
}

void initWindow();
void initVulkan();
void Cleanup();

void loadModels();
void createObjects();

void Run()
{
	initWindow();
  loadModels();
  createObjects();
	initVulkan();
	Cleanup();
}

void createInstance();
void createSurface();
void pickPhysicalDevice();
void createLogicalDevice();
void createSwapChain();
void createImageViews();
void createRenderPass();
void createShadowMapRenderPass();
void createShadowMapDescriptorSetLayout();
void createDescriptorSetLayout();
void createObjectGraphicsPipeline();
void createShadowMapGraphicsPipeline();
void createFramebuffers();
void createCommandPool();
void createColorResources();
void createShadowMapResources();
void createShadowMapSampler();
void createFramebufferForShadowMap();
void createDepthResources();
void createTextureImage();
void createTextureImageView();
void createTextureSampler();
void createVertexBuffers();
void createIndexBuffer();
void createShadowMapUniformBuffers();
void createUniformBuffers();
void createShadowMapDescriptorPool();
void createDescriptorPool();
void createShadowMapDescriptorSets();
void createDescriptorSets();
void createCommandBuffers();
void createSyncObjects();
void setupInput();
void mainLoop();

void initVulkan()
{
	createInstance();
	createSurface();
	pickPhysicalDevice();
	createLogicalDevice();
	createSwapChain();
	createImageViews();
	createRenderPass();
	createShadowMapRenderPass();
  createShadowMapDescriptorSetLayout();
	createDescriptorSetLayout();
	createObjectGraphicsPipeline();
	createShadowMapGraphicsPipeline();
	createCommandPool();
  createColorResources();
  createShadowMapResources();
  createShadowMapSampler();
  createFramebufferForShadowMap();
  createDepthResources();
	createFramebuffers();
  createTextureImage();
  createTextureImageView();
  createTextureSampler();
	createVertexBuffers();
	createIndexBuffer();
  createShadowMapUniformBuffers();
	createUniformBuffers();
  createShadowMapDescriptorPool();
	createDescriptorPool();
	createShadowMapDescriptorSets();
	createDescriptorSets();
	createCommandBuffers();
	createSyncObjects();
  setupInput();
  createPhysicsThread();
  
	mainLoop();
}

// createInstance

void createInstance()
{
	if (enableValidationLayers && !checkValidationLayerSupport())
	{
		printf("\033[31mERR:\033[0m Validation layers requested, but not available\n");
		exit(-1);
	}
	VkApplicationInfo appInfo;
  appInfo.pNext = NULL;
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "Vulkan";
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = "RQW";
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_0;

	VkInstanceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;

	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions;

	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
	glfwExtensionCount++;

	// One more extension for mac os support
	std::vector<const char*> requiredExtensions;
  requiredExtensions.resize(glfwExtensionCount);
	for (unsigned short i = 0; i < glfwExtensionCount-1; i++)
	{
		requiredExtensions[i] = glfwExtensions[i];
	}
	requiredExtensions[glfwExtensionCount-1] = VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME;
	// End
	
	if (enableValidationLayers)
	{
		puts("Required extensions:");
		for (int i = 0; i < requiredExtensions.size(); i++)
		{
			printf("\t%s\n", requiredExtensions[i]);
		}
	}

	createInfo.flags = 0;
	createInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;

	createInfo.enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size());
	createInfo.ppEnabledExtensionNames = requiredExtensions.data();

	if (enableValidationLayers)
	{
		createInfo.enabledLayerCount = validationLayerCount;
		createInfo.ppEnabledLayerNames = validationLayers;
	}
	else
	{
		createInfo.enabledLayerCount = 0;
	}

	uint32_t extensionCount = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
	//std::cout << extensionCount << "\n";
	std::vector<VkExtensionProperties> extensions;
  extensions.resize(extensionCount);
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

	if (enableValidationLayers)
	{
		puts("Available extensions:");
		for (int i = 0; i < extensions.size(); i++)
		{
			printf("\t%s\n", extensions[i].extensionName);
		}
	}
	
	VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);
	
	if (result != VK_SUCCESS)
	{
		printf("\033[31mERR:\033[0m Failed to create Vulkan instance(%d)\n", result);
		exit(-1);
	}
}

// createSurface

void createSurface()
{
#if defined(IS_WINDOWS)
		struct VkWin32SurfaceCreateInfoKHR createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
		createInfo.hwnd = glfwGetWin32Window(window);
		createInfo.hinstance = GetModuleHandle(nullptr);
		VkResult result = vkCreateWin32SurfaceKHR(instance, &createInfo, nullptr, &surface);
		if (result != VK_SUCCESS)
		{
			printf("\033[31mERR:\033[0m Failed to create window surface\n");
			exit(-1);
		}
#elif defined(__APPLE__)
    VkMetalSurfaceCreateInfoEXT createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT;
    createInfo.pLayer = (CAMetalLayer*)layer; // Your window's metal layer
    
    VkResult result = vkCreateMetalSurfaceEXT(instance, &createInfo, nullptr, &surface);

		if (result != VK_SUCCESS)
		{
			printf("\033[31mERR:\033[0m Failed to create window surface\n");
			exit(-1);
		}
#else
		VkResult result = glfwCreateWindowSurface(instance, window, nullptr, &surface);
		if (result != VK_SUCCESS)
		{
			printf("\033[31mERR:\033[0m Failed to create window surface\n");
			exit(-1);
		}
#endif
}

// pickPhysicalDevice

bool isDeviceSuitable(VkPhysicalDevice device);

void pickPhysicalDevice()
{
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
	if (deviceCount == 0)
	{
		printf("\033[31mERR:\033[0m Failed to find GPUs with Vulkan support!\n");
		exit(-1);
	}
	std::vector<VkPhysicalDevice> devices(deviceCount);
	vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
	for (const auto& device : devices)
	{
		if (isDeviceSuitable(device))
		{
			physicalDevice = device;
      msaaSamples = getMaxUsableSampleCount();
			break;
		}
	}

	if (physicalDevice == VK_NULL_HANDLE)
	{
		printf("\033[31mERR:\033[0m Failed to find a suitable GPU\n");
		exit(-1);
	}
}

struct QueueFamilyIndices
{
	bool has_value_g;
	bool has_value_p;
	uint32_t graphicsFamily;
	uint32_t presentFamily;
};

struct QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
bool checkDeviceExtensionSupport(VkPhysicalDevice device);

bool isDeviceSuitable(VkPhysicalDevice device)
{
	struct QueueFamilyIndices indices = findQueueFamilies(device);

	bool extensionSupported = checkDeviceExtensionSupport(device);

	bool swapChainAdequate = false;
	if (extensionSupported)
	{
		querySwapChainSupport(device, &swapChainSupport);
		swapChainAdequate = swapChainSupport.formatCount != 0 && swapChainSupport.presentModeCount != 0;
	}

  VkPhysicalDeviceFeatures supportedFeatures = {};
  vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

	return indices.has_value_g && indices.has_value_p && extensionSupported && swapChainAdequate && supportedFeatures.samplerAnisotropy;
}

struct QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device)
{
	struct QueueFamilyIndices indices = {false, false, 0, 0};
	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

	std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

	for (int i = 0; i < queueFamilies.size(); i++)
	{
		if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
		{
			indices.graphicsFamily = i;
			indices.has_value_g = true;
			break;
		}
	}

	for (uint32_t i = 0; i < queueFamilyCount; i++)
	{
		VkBool32 presentSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
		if (presentSupport)
		{
			indices.presentFamily = i;
			indices.has_value_p = true;
			break;
		}
	}

	return indices;
}

bool checkDeviceExtensionSupport(VkPhysicalDevice device)
{
	uint32_t extensionCount;
	vkEnumerateDeviceExtensionProperties(device, NULL, &extensionCount, NULL);

	std::vector<VkExtensionProperties> availableExtensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(device, NULL, &extensionCount, availableExtensions.data());

	const char* requiredExtensions[DEVICE_EXTENSION_COUNT];
	for (int i = 0; i < DEVICE_EXTENSION_COUNT; i++)
	{
		requiredExtensions[i] = deviceExtensions[i];
	}

	for (int i = 0; i < DEVICE_EXTENSION_COUNT; i++)
	{
		for (uint32_t i2 = 0; i2 < extensionCount; i2++)
		{
			if (strcmp(requiredExtensions[i], availableExtensions[i2].extensionName) == 0)
			{
				requiredExtensions[i] = "";
			}
		}
	}

	for (int i = 0; i < DEVICE_EXTENSION_COUNT; i++)
	{
		if (requiredExtensions[i] != "")
		{
			return false;
		}
	}

	return true;
}

// createLogicalDevice

void createLogicalDevice()
{
	struct QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

	unsigned char queueFamiliesCount;

	if (indices.graphicsFamily == indices.presentFamily)
	{
		queueFamiliesCount = 1;
	}
	else
	{
		queueFamiliesCount = 2;
	}

	std::vector<uint32_t> uniqueQueueFamilies(queueFamiliesCount);

	if (queueFamiliesCount == 1)
	{
		uniqueQueueFamilies[0] = indices.graphicsFamily;
	}
	else
	{
		uniqueQueueFamilies[0] = indices.graphicsFamily;
		uniqueQueueFamilies[1] = indices.presentFamily;
	}

	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos(queueFamiliesCount);

	float queuePriority = 1.0f;
	for (int i = 0; i < queueFamiliesCount; i++)
	{
		VkDeviceQueueCreateInfo queueCreateInfo = {};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = uniqueQueueFamilies[i];
		queueCreateInfo.queueCount = 1;
		queueCreateInfo.pQueuePriorities = &queuePriority;
		queueCreateInfos[i] = queueCreateInfo;
	}

	VkDeviceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	createInfo.queueCreateInfoCount = queueFamiliesCount;
	createInfo.pQueueCreateInfos = queueCreateInfos.data();

	VkPhysicalDeviceFeatures deviceFeatures = {}; // VK FEATURES!!!
  deviceFeatures.samplerAnisotropy = VK_TRUE;
  deviceFeatures.sampleRateShading = VK_TRUE;
	
	createInfo.pEnabledFeatures = &deviceFeatures;

	createInfo.enabledExtensionCount = DEVICE_EXTENSION_COUNT;
	createInfo.ppEnabledExtensionNames = deviceExtensions;
	if (enableValidationLayers)
	{
		createInfo.enabledLayerCount = validationLayerCount;
		createInfo.ppEnabledLayerNames = validationLayers;
	}
	else
	{
		createInfo.enabledLayerCount = 0;
	}

	VkResult result = vkCreateDevice(physicalDevice, &createInfo, NULL, &device);
	if (result != VK_SUCCESS)
	{
		printf("\033[31mERR:\033[0m Failed to create logical device\n");
		exit(-1);
	}

	vkGetDeviceQueue(device, indices.graphicsFamily, 0, &graphicsQueue);
	vkGetDeviceQueue(device, indices.presentFamily, 0, &presentQueue);
}

// createSwapChain

void createSwapChain()
{
	querySwapChainSupport(physicalDevice, &swapChainSupport);

	VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats, swapChainSupport.formatCount);
	VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes, swapChainSupport.presentModeCount);
	VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

	swapChainImageFormat = surfaceFormat.format;
	swapChainExtent = extent;

	uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
	if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount)
	{
		imageCount = swapChainSupport.capabilities.maxImageCount;
	}

	VkSwapchainCreateInfoKHR createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface = surface;
	createInfo.minImageCount = imageCount;
	createInfo.imageFormat = surfaceFormat.format;
	createInfo.imageColorSpace = surfaceFormat.colorSpace;
	createInfo.imageExtent = extent;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	struct QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
	uint32_t queueFamilyIndices[2] = {
		indices.graphicsFamily, indices.presentFamily
	};

	if (indices.graphicsFamily != indices.presentFamily)
	{
		createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		createInfo.queueFamilyIndexCount = 2;
		createInfo.pQueueFamilyIndices = queueFamilyIndices;
	}
	else
	{
		createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		createInfo.queueFamilyIndexCount = 0;
		createInfo.pQueueFamilyIndices = NULL;
	}

	createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createInfo.presentMode = presentMode;
	createInfo.clipped = VK_TRUE;
	createInfo.oldSwapchain = VK_NULL_HANDLE; // Resizing window
		
	VkResult result = vkCreateSwapchainKHR(device, &createInfo, NULL, &swapChain);
	if (result != VK_SUCCESS)
	{
		printf("\033[31mERR:\033[0m Failed to create swap chain\n");
		exit(-1);
	}

	vkGetSwapchainImagesKHR(device, swapChain, &imageCount, NULL);
	swapChainImageCount = imageCount;
	swapChainImages = (VkImage*)calloc(imageCount, sizeof(VkImage));
	vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages);
}

// createImageViews

VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels);

void createImageViews()
{

  // swap chain stuff
  free(swapChainImageViews);
	swapChainImageViewCount = swapChainImageCount;
	swapChainImageViews = (VkImageView*)calloc(swapChainImageViewCount, sizeof(VkImageView));
	
	for (size_t i = 0; i < swapChainImageViewCount; i++)
	{
    swapChainImageViews[i] = createImageView(swapChainImages[i], swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1);
	}
}

// createRenderPass

VkFormat findDepthFormat();

void createRenderPass()
{
	VkAttachmentDescription colorAttachment = {};
	colorAttachment.format = swapChainImageFormat;
	colorAttachment.samples = msaaSamples;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference colorAttachmentRef = {};
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkAttachmentDescription colorAttachmentResolve = {};
  colorAttachmentResolve.format = swapChainImageFormat;
  colorAttachmentResolve.samples = VK_SAMPLE_COUNT_1_BIT;
  colorAttachmentResolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  colorAttachmentResolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  colorAttachmentResolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  colorAttachmentResolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  colorAttachmentResolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  colorAttachmentResolve.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  VkAttachmentReference colorAttachmentResolveRef = {};
  colorAttachmentResolveRef.attachment = 2;
  colorAttachmentResolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkAttachmentDescription depthAttachment = {};
  depthAttachment.flags = {};
  depthAttachment.format = findDepthFormat();
  depthAttachment.samples = msaaSamples;
  depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

  VkAttachmentReference depthAttachmentRef = {};
  depthAttachmentRef.attachment = 1;
  depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass = {};
  subpass.flags = {};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.inputAttachmentCount = 0;
  subpass.pInputAttachments = {};
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &colorAttachmentRef;
  subpass.pResolveAttachments = &colorAttachmentResolveRef;
  subpass.pDepthStencilAttachment = &depthAttachmentRef;

	VkSubpassDependency dependency = {};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	dependency.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	dependency.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  dependency.dependencyFlags = {};

  std::array<VkAttachmentDescription, 3> attachments = {
    colorAttachment,
    depthAttachment,
    colorAttachmentResolve
  };

	VkRenderPassCreateInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = attachments.size();
	renderPassInfo.pAttachments = attachments.data();
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	renderPassInfo.dependencyCount = 1;
	renderPassInfo.pDependencies = &dependency;

	VkResult result = vkCreateRenderPass(device, &renderPassInfo, NULL, &renderPass);
	if (result != VK_SUCCESS)
	{
		printf("\033[31mERR:\033[0m Failed to create render pass\n");
		exit(-1);
	}
}

// createShadowMapRenderPass

void createShadowMapRenderPass()
{
	VkAttachmentDescription colorAttachment = {}; // TODO: make in VK_ATTACHMENT_UNUSED
  colorAttachment.flags = VK_ATTACHMENT_UNUSED;
	colorAttachment.format = swapChainImageFormat;
	colorAttachment.samples = msaaSamples;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference colorAttachmentRef = {};
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkAttachmentDescription depthAttachment = {};
  depthAttachment.flags = {};
  depthAttachment.format = findDepthFormat();
  depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
  depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

  VkAttachmentReference depthAttachmentRef = {};
  depthAttachmentRef.attachment = 0; // was 1
  depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass = {};
  subpass.flags = {};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.inputAttachmentCount = 0;
  subpass.pInputAttachments = {};
  subpass.colorAttachmentCount = 0;
  subpass.pColorAttachments = {};
  subpass.pResolveAttachments = {};
  subpass.pDepthStencilAttachment = &depthAttachmentRef;

	VkSubpassDependency dependency = {};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	dependency.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
	dependency.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependency.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
  dependency.dependencyFlags = {};

  std::array<VkAttachmentDescription, 1> attachments = {
    depthAttachment
  };

	VkRenderPassCreateInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = attachments.size();
	renderPassInfo.pAttachments = attachments.data();
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	renderPassInfo.dependencyCount = 1;
	renderPassInfo.pDependencies = &dependency;

	VkResult result = vkCreateRenderPass(device, &renderPassInfo, NULL, &shadowMapRenderPass);
	if (result != VK_SUCCESS)
	{
		printf("\033[31mERR:\033[0m Failed to create render pass\n");
		exit(-1);
	}
}

// createShadowMapDescriptorSetLayout

void createShadowMapDescriptorSetLayout()
{
	VkDescriptorSetLayoutBinding uboLayoutBinding = {};
	uboLayoutBinding.binding = 0;
	uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uboLayoutBinding.descriptorCount = 1;
	uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	uboLayoutBinding.pImmutableSamplers = NULL;

  uint32_t bindingCount = 1;
  VkDescriptorSetLayoutBinding bindings[] = {uboLayoutBinding};

	VkDescriptorSetLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = bindingCount;
	layoutInfo.pBindings = bindings;
	
	VkResult result = vkCreateDescriptorSetLayout(device, &layoutInfo, NULL, &shadowMapDescriptorSetLayout);
	if (result != VK_SUCCESS)
	{
		printf("\033[31mERR:\033[0m Failed to create descriptor set layout\n");
		exit(-1);
	}
}

// createDescriptorSetLayout

void createDescriptorSetLayout()
{
	VkDescriptorSetLayoutBinding uboLayoutBinding = {};
	uboLayoutBinding.binding = 0;
	uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uboLayoutBinding.descriptorCount = 1;
	uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	uboLayoutBinding.pImmutableSamplers = NULL;

  VkDescriptorSetLayoutBinding samplerLayoutBinding = {};
  samplerLayoutBinding.binding = 1;
  samplerLayoutBinding.descriptorCount = 1;
  samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  samplerLayoutBinding.pImmutableSamplers = NULL;
  samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

  VkDescriptorSetLayoutBinding shadowMapSamplerLayoutBinding = {};
  shadowMapSamplerLayoutBinding.binding = 2;
  shadowMapSamplerLayoutBinding.descriptorCount = 1;
  shadowMapSamplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  shadowMapSamplerLayoutBinding.pImmutableSamplers = NULL;
  shadowMapSamplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

  uint32_t bindingCount = 3;
  VkDescriptorSetLayoutBinding bindings[] = {uboLayoutBinding, samplerLayoutBinding, shadowMapSamplerLayoutBinding};

	VkDescriptorSetLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = bindingCount;
	layoutInfo.pBindings = bindings;
	
	VkResult result = vkCreateDescriptorSetLayout(device, &layoutInfo, NULL, &descriptorSetLayout);
	if (result != VK_SUCCESS)
	{
		printf("\033[31mERR:\033[0m Failed to create descriptor set layout\n");
		exit(-1);
	}
}

// createObjectGraphicsPipeline

std::vector<char> readFile(char* filePath);
VkShaderModule createShaderModule(std::string code);
#include "embededFiles.hxx"

void createObjectGraphicsPipeline()
{
  char vertSrc[] = "shaders/shader.vert.spv";
  char fragSrc[] = "shaders/shader.frag.spv";

  std::string objectVertShaderCode = base64_decode(objectVertShaderCodeBase64, false);
  std::string objectFragShaderCode = base64_decode(objectFragShaderCodeBase64, false);

	VkShaderModule objectVertShaderModule = createShaderModule(objectVertShaderCode);
	VkShaderModule objectFragShaderModule = createShaderModule(objectFragShaderCode);

	VkPipelineShaderStageCreateInfo vertShaderStageInfo = {};
	vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertShaderStageInfo.module = objectVertShaderModule;
	vertShaderStageInfo.pName = "main";

	VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
	fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragShaderStageInfo.module = objectFragShaderModule;
	fragShaderStageInfo.pName = "main";

  std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages = {fragShaderStageInfo, vertShaderStageInfo};
	
	uint32_t dynamicStateCount = 2;
	VkDynamicState dynamicStates[2] = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR
	};

	VkPipelineDynamicStateCreateInfo dynamicState = {};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = dynamicStateCount;
	dynamicState.pDynamicStates = dynamicStates;

	VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

	VkVertexInputBindingDescription bindingDescription = getBindingDescription();
	VkVertexInputAttributeDescription* attributeDescriptions = getAttributeDescriptions();

	vertexInputInfo.vertexBindingDescriptionCount = 1;
	vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
	vertexInputInfo.vertexAttributeDescriptionCount = attributeDescriptionCount;
	vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions;

	VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	VkViewport viewport = {};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)swapChainExtent.width;
	viewport.height = (float)swapChainExtent.width;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor = {};
	scissor.offset = { 0, 0 };
	scissor.extent = swapChainExtent;

	VkPipelineViewportStateCreateInfo viewportState = {};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.pViewports = &viewport;
	viewportState.scissorCount = 1;
	viewportState.pScissors = &scissor;

	VkPipelineRasterizationStateCreateInfo rasterizer = {};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; // y-flip compensation
	rasterizer.depthBiasEnable = VK_FALSE;
	rasterizer.depthBiasConstantFactor = 0.0f;
	rasterizer.depthBiasClamp = 0.0f;
	rasterizer.depthBiasSlopeFactor = 0.0f;

	VkPipelineMultisampleStateCreateInfo multisampling = {};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_TRUE;
	multisampling.rasterizationSamples = msaaSamples;
	multisampling.minSampleShading = 0.2f;
	multisampling.pSampleMask = NULL;
	multisampling.alphaToCoverageEnable = VK_FALSE;
	multisampling.alphaToOneEnable = VK_FALSE;

	VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE;
	colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

	VkPipelineColorBlendStateCreateInfo colorBlending = {};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.logicOp = VK_LOGIC_OP_COPY;
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;
	colorBlending.blendConstants[0] = 0.0f;
	colorBlending.blendConstants[1] = 0.0f;
	colorBlending.blendConstants[2] = 0.0f;
	colorBlending.blendConstants[3] = 0.0f;

	VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 1;
	pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout; 
	pipelineLayoutInfo.pushConstantRangeCount = 0; 
	pipelineLayoutInfo.pPushConstantRanges = NULL; 

	VkResult result = vkCreatePipelineLayout(device, &pipelineLayoutInfo, NULL, &objectPipelineLayout);
	if (result != VK_SUCCESS)
	{
		printf("\033[31mERR:\033[0m Failed to create pipeline layout\n");
		exit(-1);
	}

  VkPipelineDepthStencilStateCreateInfo depthStencil = {};
  depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO; 
  depthStencil.depthTestEnable = VK_TRUE; 
  depthStencil.depthWriteEnable = VK_TRUE;
  depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;

	VkGraphicsPipelineCreateInfo pipelineInfo = {};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = shaderStages.size();
	pipelineInfo.pStages = shaderStages.data();

	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pDepthStencilState = &depthStencil;
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDynamicState = &dynamicState;

	pipelineInfo.layout = objectPipelineLayout;
	
	pipelineInfo.renderPass = renderPass;
	pipelineInfo.subpass = 0;

	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
	pipelineInfo.basePipelineIndex = -1;

	VkResult result2 = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &objectGraphicsPipeline);
	if (result2 != VK_SUCCESS)
	{
		printf("\033[31mERR:\033[0m Failed to create graphics pipeline\n");
		exit(-1);
	}

	vkDestroyShaderModule(device, objectVertShaderModule, nullptr);
	vkDestroyShaderModule(device, objectFragShaderModule, nullptr);
}

// createShadowMapGraphicsPipeline

std::vector<char> readFile(char* filePath);
VkShaderModule createShaderModule(std::string code);

void createShadowMapGraphicsPipeline()
{
  std::string shadowMapVertShaderCode = base64_decode(shadowMapVertShaderCodeBase64, false);
  std::string shadowMapFragShaderCode = base64_decode(shadowMapFragShaderCodeBase64, false);

	VkShaderModule shadowMapVertShaderModule = createShaderModule(shadowMapVertShaderCode);
	VkShaderModule shadowMapFragShaderModule = createShaderModule(shadowMapFragShaderCode);

	VkPipelineShaderStageCreateInfo vertShaderStageInfo = {};
	vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertShaderStageInfo.module = shadowMapVertShaderModule;
	vertShaderStageInfo.pName = "main";

	VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
	fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragShaderStageInfo.module = shadowMapFragShaderModule;
	fragShaderStageInfo.pName = "main";

  std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages = {fragShaderStageInfo, vertShaderStageInfo};
	
	uint32_t dynamicStateCount = 2;
	VkDynamicState dynamicStates[2] = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR
	};

	VkPipelineDynamicStateCreateInfo dynamicState = {};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = dynamicStateCount;
	dynamicState.pDynamicStates = dynamicStates;

	VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

	VkVertexInputBindingDescription bindingDescription = getBindingDescription();
	VkVertexInputAttributeDescription* attributeDescriptions = getAttributeDescriptions();

	vertexInputInfo.vertexBindingDescriptionCount = 1;
	vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
	vertexInputInfo.vertexAttributeDescriptionCount = attributeDescriptionCount;
	vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions;

	VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	VkViewport viewport = {};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)shadowMapExtent.width;
	viewport.height = (float)shadowMapExtent.width;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor = {};
	scissor.offset = { 0, 0 };
	scissor.extent = shadowMapExtent;

	VkPipelineViewportStateCreateInfo viewportState = {};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.pViewports = &viewport;
	viewportState.scissorCount = 1;
	viewportState.pScissors = &scissor;

	VkPipelineRasterizationStateCreateInfo rasterizer = {};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; // y-flip compensation
	rasterizer.depthBiasEnable = VK_FALSE;
	rasterizer.depthBiasConstantFactor = 0.0f;
	rasterizer.depthBiasClamp = 0.0f;
	rasterizer.depthBiasSlopeFactor = 0.0f;

	VkPipelineMultisampleStateCreateInfo multisampling = {};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampling.minSampleShading = 0.0f;
	multisampling.pSampleMask = NULL;
	multisampling.alphaToCoverageEnable = VK_FALSE;
	multisampling.alphaToOneEnable = VK_FALSE;

	VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE;
	colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

	VkPipelineColorBlendStateCreateInfo colorBlending = {};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.logicOp = VK_LOGIC_OP_COPY;
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;
	colorBlending.blendConstants[0] = 0.0f;
	colorBlending.blendConstants[1] = 0.0f;
	colorBlending.blendConstants[2] = 0.0f;
	colorBlending.blendConstants[3] = 0.0f;

	VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 1;
	pipelineLayoutInfo.pSetLayouts = &shadowMapDescriptorSetLayout; 
	pipelineLayoutInfo.pushConstantRangeCount = 0; 
	pipelineLayoutInfo.pPushConstantRanges = NULL; 

	VkResult result = vkCreatePipelineLayout(device, &pipelineLayoutInfo, NULL, &shadowMapPipelineLayout);
	if (result != VK_SUCCESS)
	{
		printf("\033[31mERR:\033[0m Failed to create pipeline layout\n");
		exit(-1);
	}

  VkPipelineDepthStencilStateCreateInfo depthStencil = {};
  depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO; 
  depthStencil.depthTestEnable = VK_TRUE; 
  depthStencil.depthWriteEnable = VK_TRUE;
  depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;

	VkGraphicsPipelineCreateInfo pipelineInfo = {};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = shaderStages.size();
	pipelineInfo.pStages = shaderStages.data();

	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pDepthStencilState = &depthStencil;
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDynamicState = &dynamicState;

	pipelineInfo.layout = shadowMapPipelineLayout;
	
	pipelineInfo.renderPass = shadowMapRenderPass;
	pipelineInfo.subpass = 0;

	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
	pipelineInfo.basePipelineIndex = -1;

	VkResult result2 = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &shadowMapGraphicsPipeline);
	if (result2 != VK_SUCCESS)
	{
		printf("\033[31mERR:\033[0m Failed to create graphics pipeline for shadow maps\n");
		exit(-1);
	}

	vkDestroyShaderModule(device, shadowMapVertShaderModule, nullptr);
	vkDestroyShaderModule(device, shadowMapFragShaderModule, nullptr);
}

#include <cstdio>
#include <fstream>

std::vector<char> readFile(char* filePath)
{
  std::ifstream file;
  file.open(filePath, std::ios::binary);
  if (!file)
  {
		printf("\033[31mERR:\033[0m Failed to open file \"%s\"\n", filePath);
		exit(-1);
  }
  file.seekg(0, std::ios::end);
  size_t fileSizeInByte = file.tellg();
  std::vector<char> buffer;
  buffer.resize(fileSizeInByte);
  file.seekg(0, std::ios::beg);
  file.read(&buffer[0], fileSizeInByte);
	return buffer;
}

VkShaderModule createShaderModule(std::string code)
{
	VkShaderModuleCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = code.size();
	createInfo.pCode = (const uint32_t*)code.data();
	VkShaderModule shaderModule;
	VkResult result = vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule);
	if (result != VK_SUCCESS)
	{
		printf("\033[31mERR:\033[0m Failed to create shader module");
		exit(-1);
	}
	return shaderModule;
}

// createFramebuffers

void createFramebuffers()
{
	swapChainFramebufferCount = swapChainImageViewCount;
	swapChainFramebuffers = (VkFramebuffer*)calloc(swapChainFramebufferCount, sizeof(VkFramebuffer));
	for (size_t i = 0; i < swapChainImageViewCount; i++)
	{
    std::array<VkImageView, 3> attachments = {
      colorImageView,
      depthImageView,
			swapChainImageViews[i]
		};
		VkFramebufferCreateInfo framebufferInfo = {};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = renderPass;
		framebufferInfo.attachmentCount = attachments.size();
		framebufferInfo.pAttachments = attachments.data();
		framebufferInfo.width = swapChainExtent.width;
		framebufferInfo.height = swapChainExtent.height;
		framebufferInfo.layers = 1;

		VkResult result = vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapChainFramebuffers[i]);
		if (result != VK_SUCCESS)
		{
			printf("\033[31mERR:\033[0m Failed to create framebuffer\n");
			exit(-1);
		}
	}
}

// createCommandPool

void createCommandPool()
{
	struct QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice);

	VkCommandPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily;

	VkResult result = vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool);
	if (result != VK_SUCCESS)
	{
		printf("\033[31mERR:\033[0m Failed to create command pool!\n");
		exit(-1);
	}
}

// createColorResources

void createImage(uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits numSamples, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage* image, VkDeviceMemory* imageMemory);
VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels);

void createColorResources()
{
  VkFormat colorFormat = swapChainImageFormat;

  createImage(swapChainExtent.width, swapChainExtent.height, 1, msaaSamples, colorFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &colorImage, &colorImageMemory);
  colorImageView = createImageView(colorImage, colorFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1);
}

// createShadowMapResources

VkFormat findDepthFormat();
void createImage(uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits numSamples, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage* image, VkDeviceMemory* imageMemory);
void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels);

void createShadowMapResources()
{
  VkFormat depthFormat = findDepthFormat();
  
  createImage(
      shadowMapExtent.width,
      shadowMapExtent.height,
      1,
      VK_SAMPLE_COUNT_1_BIT,
      depthFormat,
      VK_IMAGE_TILING_OPTIMAL,
      VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
      &shadowMapImage, &shadowMapImageMemory);

  shadowMapImageView = createImageView(shadowMapImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, 1);
  transitionImageLayout(shadowMapImage, depthFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 1);
}

// createShadowMapSampler

void createShadowMapSampler()
{
  VkSamplerCreateInfo samplerInfo{};
  samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  samplerInfo.magFilter = VK_FILTER_LINEAR;
  samplerInfo.minFilter = VK_FILTER_LINEAR;
  samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
  samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
  samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
  samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
  samplerInfo.compareEnable = VK_TRUE;  // Enable hardware shadow comparison
  samplerInfo.compareOp = VK_COMPARE_OP_LESS;  // Common for depth shadows
  samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

  vkCreateSampler(device, &samplerInfo, nullptr, &shadowMapSampler);
}

// createFramebufferForShadowMap

void createFramebufferForShadowMap()
{
  std::array<VkImageView, 1> attachments = {
    shadowMapImageView
  };
  VkFramebufferCreateInfo framebufferInfo = {};
  framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  framebufferInfo.renderPass = shadowMapRenderPass;
  framebufferInfo.attachmentCount = attachments.size();
  framebufferInfo.pAttachments = attachments.data();
  framebufferInfo.width = shadowMapExtent.width;
  framebufferInfo.height = shadowMapExtent.height;
  framebufferInfo.layers = 1;

  VkResult result = vkCreateFramebuffer(device, &framebufferInfo, nullptr, &shadowMapFramebuffer);
  if (result != VK_SUCCESS)
  {
    printf("\033[31mERR:\033[0m Failed to create framebuffer for shadow map\n");
    exit(-1);
  }
}

// createDepthResources

VkFormat findDepthFormat();
void createImage(uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits numSamples, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage* image, VkDeviceMemory* imageMemory);
void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels);

void createDepthResources()
{
  VkFormat depthFormat = findDepthFormat();
  
  createImage(swapChainExtent.width, swapChainExtent.height, 1, msaaSamples, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &depthImage, &depthImageMemory);
  depthImageView = createImageView(depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, 1);
  transitionImageLayout(depthImage, depthFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 1);
}

// findDepthFormat()

VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);

VkFormat findDepthFormat()
{
  return findSupportedFormat(
      {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
      VK_IMAGE_TILING_OPTIMAL,
      VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
      );
}

bool hasStencilComponent(VkFormat format)
{
  return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features)
{
  for (const auto format : candidates)
  {
    VkFormatProperties props;
    vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);
    if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features)
      return format;
    if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features)
      return format;
  }

  throw std::runtime_error("Failed to find supported format!");
}

void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer* buffer, VkDeviceMemory * bufferMemory);
uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

void createImage(uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits numSamples, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage* image, VkDeviceMemory* imageMemory);

void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels);
void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);

// createTextureImage

void generateMipmaps(VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels);

void createTextureImage() {
  int texWidth, texHeight, texChannels;
  stbi_uc* pixels = stbi_load("static/textures/statue.jpg", &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
  VkDeviceSize imageSize = texWidth * texHeight * 4;

  mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1;

  if (!pixels) {
		printf("\033[31mERR:\033[0m Failed to load texture image!\n");
		exit(-1);
  }

  VkBuffer stagingBuffer;
  VkDeviceMemory stagingBufferMemory;
  createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &stagingBuffer, &stagingBufferMemory);

  void* data;
  vkMapMemory(device, stagingBufferMemory, 0, imageSize, 0, &data);
  memcpy(data, pixels, (size_t)imageSize);
  vkUnmapMemory(device, stagingBufferMemory);

  stbi_image_free(pixels);

  createImage(texWidth, texHeight, mipLevels, VK_SAMPLE_COUNT_1_BIT, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &textureImage, &textureImageMemory);

  transitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, mipLevels);
  copyBufferToImage(stagingBuffer, textureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));

  generateMipmaps(textureImage, VK_FORMAT_R8G8B8A8_SRGB, texWidth, texHeight, mipLevels);

  vkDestroyBuffer(device, stagingBuffer, nullptr);
  vkFreeMemory(device, stagingBufferMemory, nullptr);
}

VkCommandBuffer beginSingleTimeCommands();
void endSingleTimeCommands(VkCommandBuffer commandBuffer);

void generateMipmaps(VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels)
{
  VkFormatProperties formatProperties;
  vkGetPhysicalDeviceFormatProperties(physicalDevice, imageFormat, &formatProperties);

  if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT))
  {
		printf("\033[31mERR:\033[0m Texture image format does not support linear blitting!\n");
    exit(-1);
  }

  VkCommandBuffer commandBuffer = beginSingleTimeCommands();

  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.image = image;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;
  barrier.subresourceRange.levelCount = 1;

  int32_t mipWidth = texWidth;
  int32_t mipHeight = texHeight;

  for (uint32_t i = 1; i < mipLevels; i++)
  {
    barrier.subresourceRange.baseMipLevel = i - 1;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
        0, nullptr,
        0, nullptr,
        1, &barrier
      );

    VkImageBlit blit{};
    blit.srcOffsets[0] = { 0, 0, 0 };
    blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
    blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.srcSubresource.mipLevel = i - 1;
    blit.srcSubresource.baseArrayLayer = 0;
    blit.srcSubresource.layerCount = 1;
    blit.dstOffsets[0] = { 0, 0, 0 };
    blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
    blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.dstSubresource.mipLevel = i;
    blit.dstSubresource.baseArrayLayer = 0;
    blit.dstSubresource.layerCount = 1;

    vkCmdBlitImage(
        commandBuffer,
        image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1, &blit,
        VK_FILTER_LINEAR
      );

    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
        0, nullptr,
        0, nullptr,
        1, &barrier
      );

    if (mipWidth > 1) mipWidth /= 2;
    if (mipHeight > 1) mipHeight /= 2;
  }

  barrier.subresourceRange.baseMipLevel = mipLevels - 1;
  barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

  vkCmdPipelineBarrier(
      commandBuffer,
      VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
      0, nullptr,
      0, nullptr,
      1, &barrier
    );

  endSingleTimeCommands(commandBuffer);
}

void createImage(uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits numSamples, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage* image, VkDeviceMemory* imageMemory)
{
  VkImageCreateInfo imageInfo = {};
  imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imageInfo.imageType = VK_IMAGE_TYPE_2D;
  imageInfo.extent.width = width;
  imageInfo.extent.height = height;
  imageInfo.extent.depth = 1;
  imageInfo.mipLevels = mipLevels;
  imageInfo.arrayLayers = 1;
  imageInfo.format = format;
  imageInfo.tiling = tiling;
  imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  imageInfo.usage = usage;
  imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  imageInfo.samples = numSamples;
  imageInfo.flags = 0;
  
  VkResult res = vkCreateImage(device, &imageInfo, nullptr, image);
  if (res != VK_SUCCESS) {
		printf("\033[31mERR:\033[0m Failed to create image!\n");
		exit(-1);
  }

  VkMemoryRequirements memRequirements;
  vkGetImageMemoryRequirements(device, *image, &memRequirements);

  VkMemoryAllocateInfo allocInfo = {};
  allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocInfo.allocationSize = memRequirements.size;
  allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

  VkResult res2 = vkAllocateMemory(device, &allocInfo, nullptr, imageMemory);
  if (res2 != VK_SUCCESS) {
		printf("\033[31mERR:\033[0m Failed to allocate image memory!\n");
		exit(-1);
  }

  vkBindImageMemory(device, *image, *imageMemory, 0);
}

VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels);

void createTextureImageView()
{
  textureImageView = createImageView(textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT, mipLevels);
}

VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels)
{
  VkImageViewCreateInfo viewInfo = {};
  viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  viewInfo.image = image;
  viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
  viewInfo.format = format;
  viewInfo.subresourceRange.aspectMask = aspectFlags;
  viewInfo.subresourceRange.baseMipLevel = 0;
  viewInfo.subresourceRange.levelCount = mipLevels;
  viewInfo.subresourceRange.baseArrayLayer = 0;
  viewInfo.subresourceRange.layerCount = 1;

  VkImageView imageView;

  VkResult res = vkCreateImageView(device, &viewInfo, nullptr, &imageView);
  if (res != VK_SUCCESS)
  {
		printf("\033[31mERR:\033[0m Failed to create texture image view!\n");
		exit(-1);
  }

  return imageView;
}

void createTextureSampler()
{
  VkSamplerCreateInfo samplerInfo = {};
  samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  samplerInfo.magFilter = VK_FILTER_LINEAR; // TODO
  samplerInfo.minFilter = VK_FILTER_LINEAR;
  samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  samplerInfo.anisotropyEnable = VK_TRUE;
  
  VkPhysicalDeviceProperties properties = {};
  vkGetPhysicalDeviceProperties(physicalDevice, &properties);

  samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
  samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
  samplerInfo.unnormalizedCoordinates = VK_FALSE;
  samplerInfo.compareEnable = VK_FALSE;
  samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
  samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  samplerInfo.minLod = 0.0f;
  samplerInfo.maxLod = VK_LOD_CLAMP_NONE;
  samplerInfo.mipLodBias = 0.0f;

  VkResult res = vkCreateSampler(device, &samplerInfo, nullptr, &textureSampler);
  if (res != VK_SUCCESS)
  {
		printf("\033[31mERR:\033[0m Failed to create texture sampler!\n");
		exit(-1);
  }
}

// createVertexBuffers, createIndexBuffer, createUniformBuffers

void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

void createVertexBuffers()
{
  for (auto& model : Models)
  {
    VkDeviceSize bufferSize = sizeof(model.second.vertices[0]) * model.second.vertices.size();

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;

    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &stagingBuffer, &stagingBufferMemory);

    void* data;
    vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, model.second.vertices.data(), (size_t)bufferSize);
    vkUnmapMemory(device, stagingBufferMemory);

    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &(model.second.vertexBuffer), &(model.second.vertexBufferMemory));

    copyBuffer(stagingBuffer, model.second.vertexBuffer, bufferSize);

    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
  }
}

void createIndexBuffer()
{
  for (auto& model : Models)
  {
    VkDeviceSize bufferSize = sizeof(model.second.indices[0]) * model.second.indices.size();

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &stagingBuffer, &stagingBufferMemory);

    void* data;
    vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, model.second.indices.data(), (size_t)bufferSize);
    vkUnmapMemory(device, stagingBufferMemory);

    createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &(model.second.indexBuffer), &(model.second.indexBufferMemory));

    copyBuffer(stagingBuffer, model.second.indexBuffer, bufferSize);

    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
  }
}

// createShadowMapUniformBuffers

void createShadowMapUniformBuffers()
{
  for (auto& gameObject : gameObjects)
  {
    VkDeviceSize bufferSize = sizeof(struct ShadowUBO);
    
    gameObject.shadowMapUniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    gameObject.shadowMapUniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
    gameObject.shadowMapUniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
      createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &gameObject.shadowMapUniformBuffers[i], &gameObject.shadowMapUniformBuffersMemory[i]);

      vkMapMemory(device, gameObject.shadowMapUniformBuffersMemory[i], 0, bufferSize, 0, &gameObject.shadowMapUniformBuffersMapped[i]);
    }
  }
}

// createUniformBuffers

void createUniformBuffers()
{
  for (auto& gameObject : gameObjects)
  {
    VkDeviceSize bufferSize = sizeof(struct SceneUBO);
    
    gameObject.uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    gameObject.uniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
    gameObject.uniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
      createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &gameObject.uniformBuffers[i], &gameObject.uniformBuffersMemory[i]); //attention

      vkMapMemory(device, gameObject.uniformBuffersMemory[i], 0, bufferSize, 0, &gameObject.uniformBuffersMapped[i]);
    }
  }
}

void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer* buffer, VkDeviceMemory * bufferMemory)
{
	VkBufferCreateInfo bufferInfo = {};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = size;
	bufferInfo.usage = usage;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VkResult result = vkCreateBuffer(device, &bufferInfo, NULL, buffer);
	if (result != VK_SUCCESS)
	{
		printf("\033[31mERR:\033[0m Failed to create buffer\n");
		exit(-1);
	}

	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(device, *buffer, &memRequirements);

	VkMemoryAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

	VkResult result2 = vkAllocateMemory(device, &allocInfo, NULL, bufferMemory);
	if (result2 != VK_SUCCESS)
	{
		printf("\033[31mERR:\033[0m Failed to allocate buffer memory\n");
		exit(-1);
	}

	vkBindBufferMemory(device, *buffer, *bufferMemory, 0);
}

uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
	{
		if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
		{
			return i;
		}
	}

	printf("\033[31mERR:\033[0m Failed to find suitable memory type\n");
	exit(-1);
}

VkCommandBuffer beginSingleTimeCommands();
void endSingleTimeCommands(VkCommandBuffer commandBuffer);

void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
{
	VkCommandBuffer commandBuffer = beginSingleTimeCommands();

	VkBufferCopy copyRegion = {};
	copyRegion.size = size;
	vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

  endSingleTimeCommands(commandBuffer);
}

void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels)
{
  VkCommandBuffer commandBuffer = beginSingleTimeCommands();

  VkImageMemoryBarrier barrier = {};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.oldLayout = oldLayout;
  barrier.newLayout = newLayout;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = image;
  if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
  {
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

    if (hasStencilComponent(format))
    {
      barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }
  }
  else
  {
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  }
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = mipLevels;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;
  
  VkPipelineStageFlags sourceStage;
  VkPipelineStageFlags destinationStage;

  if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
  {
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
  }
  else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
  {
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; 
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  }
  else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
  {
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  }
  else 
  {
    printf("\033[31mERR:\033[0m Unsupported layout transition\n");
    exit(-1);
  }

  vkCmdPipelineBarrier(
      commandBuffer,
      sourceStage, destinationStage,
      0,
      0, NULL,
      0, NULL,
      1, &barrier
      );

  endSingleTimeCommands(commandBuffer);
}

void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height)
{
  VkCommandBuffer commandBuffer = beginSingleTimeCommands();

  VkBufferImageCopy region = {};
  region.bufferOffset = 0;
  region.bufferRowLength = 0;
  region.bufferImageHeight = 0;

  region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  region.imageSubresource.mipLevel = 0;
  region.imageSubresource.baseArrayLayer = 0;
  region.imageSubresource.layerCount = 1;

  region.imageOffset = {0, 0, 0};
  region.imageExtent = {
    width,
    height,
    1
    };

  vkCmdCopyBufferToImage(
    commandBuffer,
    buffer,
    image,
    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    1,
    &region
    );

  endSingleTimeCommands(commandBuffer);
}

VkCommandBuffer beginSingleTimeCommands()
{
  VkCommandBufferAllocateInfo allocInfo = {};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandPool = commandPool;
  allocInfo.commandBufferCount = 1;

  VkCommandBuffer commandBuffer;
  vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

  VkCommandBufferBeginInfo beginInfo = {};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  vkBeginCommandBuffer(commandBuffer, &beginInfo);

  return commandBuffer;
}

void endSingleTimeCommands(VkCommandBuffer commandBuffer)
{
  vkEndCommandBuffer(commandBuffer);

  VkSubmitInfo submitInfo = {};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffer;

  vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
  vkQueueWaitIdle(graphicsQueue);

  vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

// createShadowMapDescriptorPool

void createShadowMapDescriptorPool()
{
  std::vector<VkDescriptorPoolSize> poolSizes(1);
  poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  poolSizes[0].descriptorCount = gameObjects.size() * (uint32_t)MAX_FRAMES_IN_FLIGHT;

	VkDescriptorPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = poolSizes.size();
	poolInfo.pPoolSizes = poolSizes.data();
	poolInfo.maxSets = gameObjects.size() * (uint32_t)MAX_FRAMES_IN_FLIGHT;

	VkResult result = vkCreateDescriptorPool(device, &poolInfo, NULL, &shadowMapDescriptorPool);
	if (result != VK_SUCCESS)
	{
		printf("\033[31mERR:\033[0m Failed to create descriptor pool\n");
		exit(-1);
	}
}

// createDescriptorPool

void createDescriptorPool()
{
  std::vector<VkDescriptorPoolSize> poolSizes(3);
  poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  poolSizes[0].descriptorCount = gameObjects.size() * (uint32_t)MAX_FRAMES_IN_FLIGHT;
  poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  poolSizes[1].descriptorCount = gameObjects.size() * (uint32_t)MAX_FRAMES_IN_FLIGHT;
  poolSizes[2].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  poolSizes[2].descriptorCount = gameObjects.size() * (uint32_t)MAX_FRAMES_IN_FLIGHT;


	VkDescriptorPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = poolSizes.size();
	poolInfo.pPoolSizes = poolSizes.data();
	poolInfo.maxSets = gameObjects.size() * (uint32_t)MAX_FRAMES_IN_FLIGHT;

	VkResult result = vkCreateDescriptorPool(device, &poolInfo, NULL, &descriptorPool);
	if (result != VK_SUCCESS)
	{
		printf("\033[31mERR:\033[0m Failed to create descriptor pool\n");
		exit(-1);
	}
}

// createShadowMapDescriptorSets

void createShadowMapDescriptorSets()
{
  for (auto& gameObject : gameObjects)
  {
    std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, shadowMapDescriptorSetLayout);
    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = shadowMapDescriptorPool;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(layouts.size());
    allocInfo.pSetLayouts = layouts.data();

    gameObject.shadowMapDescriptorSets.clear();
    gameObject.shadowMapDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);

    VkResult result = vkAllocateDescriptorSets(device, &allocInfo, gameObject.shadowMapDescriptorSets.data());
    if (result != VK_SUCCESS)
    {
      printf("\033[31mERR:\033[0m Failed to allocate descriptor sets for shadow map\n");
      exit(-1);
    }
    
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
      VkDescriptorBufferInfo bufferInfo = {};
      bufferInfo.buffer = gameObject.shadowMapUniformBuffers[i];
      bufferInfo.offset = 0;
      bufferInfo.range = sizeof(struct ShadowUBO);

      uint32_t descriptorWriteCount = 1;
      std::vector<VkWriteDescriptorSet> descriptorWrites(descriptorWriteCount);

      descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      descriptorWrites[0].dstSet = gameObject.shadowMapDescriptorSets[i];
      descriptorWrites[0].dstBinding = 0;
      descriptorWrites[0].dstArrayElement = 0;
      descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
      descriptorWrites[0].descriptorCount = 1;
      descriptorWrites[0].pBufferInfo = &bufferInfo;

      vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
    }
  }
}

// createDescriptorSets

void createDescriptorSets()
{
  for (auto& gameObject : gameObjects)
  {
    std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, descriptorSetLayout);
    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(layouts.size());
    allocInfo.pSetLayouts = layouts.data();

    gameObject.descriptorSets.clear();
    gameObject.descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);

    VkResult result = vkAllocateDescriptorSets(device, &allocInfo, gameObject.descriptorSets.data());
    if (result != VK_SUCCESS)
    {
      printf("\033[31mERR:\033[0m Failed to allocate descriptor sets\n");
      exit(-1);
    }
    
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
      VkDescriptorBufferInfo bufferInfo = {};
      bufferInfo.buffer = gameObject.uniformBuffers[i];
      bufferInfo.offset = 0;
      bufferInfo.range = sizeof(struct SceneUBO);

      VkDescriptorImageInfo imageInfo = {};
      imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      imageInfo.imageView = textureImageView;
      imageInfo.sampler = textureSampler;

      VkDescriptorImageInfo shadowMapInfo{};
      shadowMapInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
      shadowMapInfo.imageView = shadowMapImageView;
      shadowMapInfo.sampler = shadowMapSampler;

      uint32_t descriptorWriteCount = 3;
      std::vector<VkWriteDescriptorSet> descriptorWrites(descriptorWriteCount);

      descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      descriptorWrites[0].dstSet = gameObject.descriptorSets[i];
      descriptorWrites[0].dstBinding = 0;
      descriptorWrites[0].dstArrayElement = 0;
      descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
      descriptorWrites[0].descriptorCount = 1;
      descriptorWrites[0].pBufferInfo = &bufferInfo;

      descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      descriptorWrites[1].dstSet = gameObject.descriptorSets[i];
      descriptorWrites[1].dstBinding = 1;
      descriptorWrites[1].dstArrayElement = 0;
      descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      descriptorWrites[1].descriptorCount = 1;
      descriptorWrites[1].pImageInfo = &imageInfo;

      descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      descriptorWrites[2].dstSet = gameObject.descriptorSets[i];
      descriptorWrites[2].dstBinding = 2;
      descriptorWrites[2].dstArrayElement = 0;
      descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      descriptorWrites[2].descriptorCount = 1;
      descriptorWrites[2].pImageInfo = &shadowMapInfo;

      vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
    }
  }
}

// createCommandBuffers

void createCommandBuffers()
{
	VkCommandBufferAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = commandPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = commandBufferCount;

	VkResult result = vkAllocateCommandBuffers(device, &allocInfo, commandBuffers);
	if (result != VK_SUCCESS)
	{
		printf("\033[31mERR:\033[0m Failed to allocate command buffers\n");
		exit(-1);
	}
}

// createSyncObjects

void createSyncObjects()
{
	VkSemaphoreCreateInfo semaphoreInfo = {};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fenceInfo = {};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		VkResult result1 = vkCreateSemaphore(device, &semaphoreInfo, NULL, &imageAvailableSemaphores[i]);
		VkResult result2 = vkCreateSemaphore(device, &semaphoreInfo, NULL, &renderFinishedSemaphores[i]);
		VkResult result3 = vkCreateFence(device, &fenceInfo, NULL, &inFlightFences[i]);
		if (result1 != VK_SUCCESS ||
				result2 != VK_SUCCESS ||
				result3 != VK_SUCCESS)
		{
			printf("\033[31mERR:\033[0m Failed to create semaphores\n");
			exit(-1);
		}
	}
}

// updateShadowUniformBuffer

void updateShadowUniformBuffer(uint32_t currentImage)
{
	struct ShadowUBO ubo = {};

  float r = 100.0f;
  glm::vec3 shift = glm::vec3(20.0f, 20.0f, 0.0f);
  lightPos = glm::vec3(r*0.7f, -r*1.0f, r) + shift;
  glm::vec3 lookTo = glm::vec3(0.0f, 0.0f, 0.0f) + shift;
  lightDirection = -glm::normalize(lookTo - lightPos);
  glm::mat4 view = glm::lookAt(lightPos, lookTo, glm::vec3(0.0f, 0.0f, 1.0f));

  //glm::mat4 proj = glm::perspective(static_cast<float>(glm::radians(80.0)), 1.0f, 0.1f, 10.0f);
  
  float farPlane = 1000.0f;
  biasFactor = 10.0f / farPlane / (static_cast<float>(SHADOW_MAP_RESOLUTION) / 8192.0f);
  float d = 100.0f;
  glm::mat4 proj = glm::ortho(-d, d, -d, d, 0.1f, farPlane);
	proj[1][1] *= -1;

  sharedLightProjViewMatrix = proj * view;

  for (auto& gameObject : gameObjects)
  {
    glm::mat4 model = gameObject.getModelMatrix();
    ubo.lightSpaceMatrix = sharedLightProjViewMatrix * model;

    memcpy(gameObject.shadowMapUniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
  }
}

// updateSceneUniformBuffer

void updateSceneUniformBuffer(uint32_t currentImage)
{
	struct SceneUBO ubo = {};

  float aspectRatio = static_cast<float>(swapChainExtent.width) / static_cast<float>(swapChainExtent.height);

  float d = 20.0;
  glm::vec3 camPos = glm::vec3(sin(glm::radians(camRotation)) * d, -cos(glm::radians(camRotation)) * d, 0.0f);
  glm::vec3 shift = glm::vec3(4.0f + 3.0f * aspectRatio, 0.0f, 0.0f);
  camRotation = -20.0f + (35.0f / aspectRatio);
  camPos += glm::vec3(0.0f, 0.0f, 13.0f) + shift;
  //glm::vec3 camPos = glm::vec3(0.0f, -3.0f, 0.0f);
  glm::vec3 lookTo = glm::vec3(0.0f, 0.0f, 11.0f) + shift;
  ubo.view = glm::lookAt(camPos, lookTo, glm::vec3(0.0f, 0.0f, 1.0f));

  ubo.proj = glm::perspective(static_cast<float>(glm::radians(60.0)), aspectRatio, 0.1f, 500.0f);
	ubo.proj[1][1] *= -1;

  ubo.lightDir = lightDirection;
  ubo.viewPos = camPos;
  //ubo.shadowMapResolution = glm::vec3(static_cast<float>(SHADOW_MAP_RESOLUTION));
  ubo.shadowMapResolution = glm::vec3(static_cast<float>(SHADOW_MAP_RESOLUTION));
  ubo.biasFactor = glm::vec3(biasFactor);

  for (auto& gameObject : gameObjects)
  {
    ubo.model = gameObject.getModelMatrix();
    ubo.normalMatrix = glm::transpose(glm::inverse(ubo.model));
    ubo.normalViewMatrix = glm::transpose(glm::inverse(ubo.view * ubo.model));
    ubo.lightSpaceMatrix = sharedLightProjViewMatrix * ubo.model;
    ubo.materialSpecular = glm::vec3(0.3f);

    memcpy(gameObject.uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
  }
}

void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
void cursorPositionCallback(GLFWwindow* window, double xpos, double ypos);
void keyboardCallback(GLFWwindow* window, int key, int scancode, int action, int mods);

void setupInput()
{
  glfwSetMouseButtonCallback(window, mouseButtonCallback);
  glfwSetCursorPosCallback(window, cursorPositionCallback);
  glfwSetKeyCallback(window, keyboardCallback);
}

void jump();

bool isDragging = false;

void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
  if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
    jump();
  if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS)
  {
    isDragging = true;
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
  }
  if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_RELEASE)
  {
    isDragging = false;
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
  }
}

double lastXpos;
double xspeed;

void cursorPositionCallback(GLFWwindow* window, double xpos, double ypos)
{
  xspeed = xpos - lastXpos;
  if (isDragging)
  {
    //camRotation -= xspeed * 0.25;
  }
  lastXpos = xpos;
}

void jump();

void keyboardCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	if (( key == GLFW_KEY_UP ||
        key == GLFW_KEY_SPACE
      ) && action == GLFW_PRESS)
		jump();
}

// mainLoop

void drawFrame();

double deltaTime = 0.0f;
double fps = 0;
std::chrono::duration<double> previousTime;

void mainLoop()
{
  auto previousTime = std::chrono::steady_clock::now();

  using Framerate = std::chrono::duration<std::chrono::steady_clock::rep, std::ratio<1, MAX_FPS>>;
  auto next = std::chrono::steady_clock::now() + Framerate{1};

	while (!glfwWindowShouldClose(window))
	{
    auto currentTime = std::chrono::steady_clock::now();
    auto elapsed = currentTime - previousTime;

    deltaTime = static_cast<double>(elapsed.count()) * 0.000000001;
    fps = 1.0 / deltaTime;
    //std::cout << fps << "\n";

		glfwPollEvents();
		drawFrame();

    previousTime = currentTime;

    std::this_thread::sleep_until(next);
    next += Framerate{1};

    //std::cout << "delta: " << deltaTime << "\n";
	}
	vkDeviceWaitIdle(device);
}

void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
void recreateSwapChain();

void drawFrame()
{
	vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

	uint32_t imageIndex;

	VkResult result1 = vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

	if (result1 == VK_ERROR_OUT_OF_DATE_KHR)
	{
		recreateSwapChain();
		return;
	}
	else if (result1 != VK_SUCCESS && result1 != VK_SUBOPTIMAL_KHR)
	{
		printf("\033[31mERR:\033[0m Failed to acquire swap chain image\n");
		exit(-1);
	}


	vkResetFences(device, 1, &inFlightFences[currentFrame]);

	vkResetCommandBuffer(commandBuffers[currentFrame], 0);
	recordCommandBuffer(commandBuffers[currentFrame], imageIndex);

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	VkSemaphore waitSemaphores[] = {imageAvailableSemaphores[currentFrame]};
	VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;

	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffers[currentFrame];

	VkSemaphore signalSemaphores[] = {renderFinishedSemaphores[currentFrame]};
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphores;

	VkResult result2 = vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]);
	if (result2 != VK_SUCCESS)
	{
		printf("\033[31mERR:\033[0m Failed to submit draw command buffer\n");
		exit(-1);
	}
  vkQueueWaitIdle(graphicsQueue);

	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = signalSemaphores;

  VkSwapchainKHR swapChains[] = {swapChain};
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapChains;

	presentInfo.pImageIndices = &imageIndex;

	VkResult result3 = vkQueuePresentKHR(presentQueue, &presentInfo);

	if (result3 == VK_ERROR_OUT_OF_DATE_KHR || result3 == VK_SUBOPTIMAL_KHR || framebufferResized)
	{
		framebufferResized = false;
		recreateSwapChain();
	}
	else if (result3 != VK_SUCCESS)
	{
		printf("\033[31mERR:\033[0m Failed to present swap chain image\n");
		exit(-1);
	}

	currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

// recordCommandBuffer

void recordShadowMapCommands(VkCommandBuffer commandBuffer);
void transitionShadowMapToRead(VkCommandBuffer commandBuffer);
void recordMainRenderCommands(VkCommandBuffer commandBuffer, uint32_t imageIndex);

void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex)
{
	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = 0;
	beginInfo.pInheritanceInfo = NULL;


	VkResult result = vkBeginCommandBuffer(commandBuffer, &beginInfo);
	if (result != VK_SUCCESS)
	{
		printf("\033[31mERR:\033[0m Failed to begin recording command buffer\n");
		exit(-1);
	}
  
	updateShadowUniformBuffer(currentFrame);
	updateSceneUniformBuffer(currentFrame);
  recordShadowMapCommands(commandBuffer);
  recordMainRenderCommands(commandBuffer, imageIndex);

	VkResult result4 = vkEndCommandBuffer(commandBuffer);
	if (result4 != VK_SUCCESS)
	{
		printf("\033[31mERR:\033[0m Failed to record command buffer\n");
		exit(-1);
	}
}

void recordShadowMapCommands(VkCommandBuffer commandBuffer)
{
  // rendering shadow map
	VkRenderPassBeginInfo shadowMapRenderPassInfo = {};
	shadowMapRenderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	shadowMapRenderPassInfo.renderPass = shadowMapRenderPass;
	shadowMapRenderPassInfo.framebuffer = shadowMapFramebuffer;

	shadowMapRenderPassInfo.renderArea.offset = {0, 0};
	shadowMapRenderPassInfo.renderArea.extent = shadowMapExtent;

	VkClearValue shadowMapClearColor[1] = {};
  shadowMapClearColor[0].depthStencil = {1.0f, 0};

	shadowMapRenderPassInfo.clearValueCount = 1;
	shadowMapRenderPassInfo.pClearValues = shadowMapClearColor;

	vkCmdBeginRenderPass(commandBuffer, &shadowMapRenderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

	VkViewport shadowMapViewport = {};
	shadowMapViewport.x = 0.0f;
	shadowMapViewport.y = 0.0f;
	shadowMapViewport.width = (float)(shadowMapExtent.width);
	shadowMapViewport.height = (float)(shadowMapExtent.height);
	shadowMapViewport.minDepth = 0.0f;
	shadowMapViewport.maxDepth = 1.0f;
	vkCmdSetViewport(commandBuffer, 0, 1, &shadowMapViewport);

	VkRect2D shadowMapScissor = {};
	shadowMapScissor.offset = {0, 0};
	shadowMapScissor.extent = shadowMapExtent;
	vkCmdSetScissor(commandBuffer, 0, 1, &shadowMapScissor);

  for (auto& gameObject : gameObjects)
  {
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shadowMapGraphicsPipeline);

    std::vector<VkBuffer> vertexBuffers = {Models[gameObject.modelName].vertexBuffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, vertexBuffers.size(), vertexBuffers.data(), offsets);
    vkCmdBindIndexBuffer(commandBuffer, Models[gameObject.modelName].indexBuffer, 0, VK_INDEX_TYPE_UINT32);

    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shadowMapPipelineLayout, 0, 1, &gameObject.shadowMapDescriptorSets[currentFrame], 0, NULL);
    vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(Models[gameObject.modelName].indices.size()), 1, 0, 0, 0);
  }

	vkCmdEndRenderPass(commandBuffer);
}

void transitionShadowMapToRead(VkCommandBuffer commandBuffer)
{
  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
  barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
  barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
  barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  barrier.image = shadowMapImage;
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = 1;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;

  vkCmdPipelineBarrier(commandBuffer,
      VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
      VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
      0, 0, nullptr, 0, nullptr,
      1, &barrier);
}

void recordMainRenderCommands(VkCommandBuffer commandBuffer, uint32_t imageIndex)
{
  // actual render
	VkRenderPassBeginInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = renderPass;
	renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex];

	renderPassInfo.renderArea.offset = {0, 0};
	renderPassInfo.renderArea.extent = swapChainExtent;

	VkClearValue clearColor[2] = {};
  clearColor[0].color = {0.63f, 0.76f, 1.0f, 1.0f};
  clearColor[1].depthStencil = {1.0f, 0};

	renderPassInfo.clearValueCount = 2;
	renderPassInfo.pClearValues = clearColor;

	vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

	VkViewport viewport = {};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)(swapChainExtent.width);
	viewport.height = (float)(swapChainExtent.height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

	VkRect2D scissor = {};
	scissor.offset = {0, 0};
	scissor.extent = swapChainExtent;
	vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

  for (auto& gameObject : gameObjects)
  {
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, objectGraphicsPipeline);

    std::vector<VkBuffer> vertexBuffers = {Models[gameObject.modelName].vertexBuffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, vertexBuffers.size(), vertexBuffers.data(), offsets);
    vkCmdBindIndexBuffer(commandBuffer, Models[gameObject.modelName].indexBuffer, 0, VK_INDEX_TYPE_UINT32);

    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, objectPipelineLayout, 0, 1, &gameObject.descriptorSets[currentFrame], 0, NULL);
    //vkCmdDraw(commandBuffer, vertexCount, 1, 0, 0);
    vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(Models[gameObject.modelName].indices.size()), 1, 0, 0, 0);
  }

	vkCmdEndRenderPass(commandBuffer);
}

void cleanupSwapChain();

void recreateSwapChain()
{
	int width = 0, height = 0;
	glfwGetFramebufferSize(window, &width, &height);

	while (width == 0 || height == 0)
	{
		glfwGetFramebufferSize(window, &width, &height);
		glfwWaitEvents();
	}

	vkDeviceWaitIdle(device);

	cleanupSwapChain();
	createSwapChain();

	createImageViews();

  cleanResources();
  createColorResources();
  //createShadowMapResources();
  createDepthResources();

  createFramebufferForShadowMap();
	createFramebuffers();
}

// Cleanup

void cleanupSwapChain();
void DestroySwapChainDetails(struct SwapChainSupportDetails *details);

void Cleanup()
{
	cleanupSwapChain();
  // textureImage
  vkDestroyImageView(device, textureImageView, NULL);
  vkDestroyImage(device, textureImage, NULL);
  vkFreeMemory(device, textureImageMemory, NULL);
  vkDestroySampler(device, textureSampler, NULL);
  // color and depth resources + shadow map
  cleanResources();
  // shadowMap
  vkDestroyImageView(device, shadowMapImageView, NULL);
  vkDestroyImage(device, shadowMapImage, NULL);
  vkFreeMemory(device, shadowMapImageMemory, NULL);
  vkDestroySampler(device, shadowMapSampler, NULL);
  // uniform buffers
  for (auto& gameObject : gameObjects)
  {
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
      vkDestroyBuffer(device, gameObject.shadowMapUniformBuffers[i], NULL);
      vkFreeMemory(device, gameObject.shadowMapUniformBuffersMemory[i], NULL);
      vkDestroyBuffer(device, gameObject.uniformBuffers[i], NULL);
      vkFreeMemory(device, gameObject.uniformBuffersMemory[i], NULL);
    }
  }
	vkDestroyDescriptorPool(device, shadowMapDescriptorPool, NULL);
	vkDestroyDescriptorPool(device, descriptorPool, NULL);
	vkDestroyDescriptorSetLayout(device, shadowMapDescriptorSetLayout, NULL);
	vkDestroyDescriptorSetLayout(device, descriptorSetLayout, NULL);
  for (auto& model : Models)
  {
    vkDestroyBuffer(device, model.second.vertexBuffer, NULL);
    vkDestroyBuffer(device, model.second.indexBuffer, NULL);
    vkFreeMemory(device, model.second.vertexBufferMemory, NULL);
    vkFreeMemory(device, model.second.indexBufferMemory, NULL);
  }
	vkDestroyPipeline(device, objectGraphicsPipeline, NULL);
	vkDestroyPipeline(device, shadowMapGraphicsPipeline, NULL);
	vkDestroyPipelineLayout(device, objectPipelineLayout, NULL);
	vkDestroyPipelineLayout(device, shadowMapPipelineLayout, NULL);
	vkDestroyRenderPass(device, renderPass, NULL);
	vkDestroyRenderPass(device, shadowMapRenderPass, NULL);
	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		vkDestroySemaphore(device, imageAvailableSemaphores[i], NULL);
		vkDestroySemaphore(device, renderFinishedSemaphores[i], NULL);
		vkDestroyFence(device, inFlightFences[i], NULL);
	}	
	vkDestroyCommandPool(device, commandPool, NULL);
	vkDestroyDevice(device, NULL);
	vkDestroySurfaceKHR(instance, surface, NULL);
	vkDestroyInstance(instance, NULL);
	DestroySwapChainDetails(&swapChainSupport);
	glfwDestroyWindow(window);
	glfwTerminate();
}

void cleanupSwapChain()
{
  cleanSwapChainImages();
	for (size_t i = 0; i < swapChainFramebufferCount; i++)
	{
		vkDestroyFramebuffer(device, swapChainFramebuffers[i], NULL);
	}

	for (size_t i = 0; i < swapChainImageViewCount; i++)
	{
		vkDestroyImageView(device, swapChainImageViews[i], NULL);
	}

  vkDestroyFramebuffer(device, shadowMapFramebuffer, NULL);

	vkDestroySwapchainKHR(device, swapChain, NULL);
}

void DestroySwapChainDetails(struct SwapChainSupportDetails *details)
{
	free(details->formats);
	free(details->presentModes);
}

static void framebufferResizeCallback()
{
	framebufferResized = true;
}

void initWindow()
{
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	//glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	//window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", glfwGetPrimaryMonitor(), NULL);
	window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", NULL, NULL);
	glfwSetFramebufferSizeCallback(window, (GLFWframebuffersizefun)framebufferResizeCallback);
  int width, height;
  glfwGetFramebufferSize(window, &width, &height);
  std::cout << "width: " << width << "\nheight: " << height << "\n";
}
