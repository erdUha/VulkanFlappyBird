int validationLayerCount = 1;
const char* validationLayers[1] = {
	"VK_LAYER_KHRONOS_validation"
};

#ifdef NODEBUG
	const bool enableValidationLayers = false;
#else
	const bool enableValidationLayers = true;
#endif

bool checkValidationLayerSupport()
{
	uint32_t layerCount;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

	std::vector<VkLayerProperties> availableLayers(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

	for (int i = 0; i < validationLayerCount; i++)
	{
		bool layerFound = false;

		for (uint32_t i2 = 0; i2 < layerCount; i2++)
		{
			if (strcmp(validationLayers[i], availableLayers[i2].layerName) == 0)
			{
				layerFound = true;
				break;
			}
		}

		if (!layerFound)
		{
			return false;
		}
	}

	return true;
}
