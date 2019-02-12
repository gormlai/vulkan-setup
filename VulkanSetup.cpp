#include "VulkanSetup.h"

///////////////////////////////////// Vulkan Variable ///////////////////////////////////////////////////////////////////

bool Vulkan::validationLayersEnabled = true;

///////////////////////////////////// Vulkan Context ///////////////////////////////////////////////////////////////////

Vulkan::VulkanContext::VulkanContext()
:_currentFrame(0)
,_swapChain(nullptr)
{
    
}

///////////////////////////////////// Vulkan Shader ///////////////////////////////////////////////////////////////////

Vulkan::Shader::Shader(const std::string & filename, ShaderType type)
:_filename(filename)
,_type(type)
{
    
}


///////////////////////////////////// Vulkan Vertex ///////////////////////////////////////////////////////////////////


///////////////////////////////////// Vulkan AppInformation ///////////////////////////////////////////////////////////////////

Vulkan::AppInformation::AppInformation()
:_window(nullptr)
, _chosenPhysicalDevice(0)
{
}

///////////////////////////////////// Vulkan BufferDescriptor ///////////////////////////////////////////////////////////////////


bool Vulkan::BufferDescriptor::fill(VkDevice device, const void * srcData, VkDeviceSize amount)
{
    void * data = nullptr;
    if(vkMapMemory(device, _memory, 0, amount, 0, &data) != VK_SUCCESS)
    {
        SDL_LogError(0, "Failed to map vertex buffer memory\n");
        return false;
    }
    
    memcpy(data, srcData, amount);
    vkUnmapMemory(device, _memory);
    return true;
}

bool Vulkan::BufferDescriptor::copyFrom(VkDevice device, VkCommandPool commandPool, VkQueue queue, BufferDescriptor & src, VkDeviceSize amount)
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
    
    VkBufferCopy copyRegion = {};
    copyRegion.srcOffset = 0; // Optional
    copyRegion.dstOffset = 0; // Optional
    copyRegion.size = amount;
    vkCmdCopyBuffer(commandBuffer, src._buffer, _buffer, 1, &copyRegion);
    
    vkEndCommandBuffer(commandBuffer);
    
    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    
    vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);
    
    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
    return true;
}

///////////////////////////////////// Vulkan Methods ///////////////////////////////////////////////////////////////////

SDL_Window * Vulkan::initSDL(const std::string & appName)
{
    SDL_Window * window = nullptr;
    
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS) != 0)
        return window;
    
    window = SDL_CreateWindow(appName.c_str(),
                              SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED,
                              640,
                              480,
                              SDL_WINDOW_SHOWN | SDL_WINDOW_VULKAN);
    if (window == nullptr)
    {
        const char * error = SDL_GetError();
        SDL_LogError(0, "Error = %s\n", error);
        
    }
    
    return window;
    
}

namespace
{
	VKAPI_ATTR VkBool32 VKAPI_CALL VulkanDebugUtilsCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
		VkDebugUtilsMessageTypeFlagsEXT type,
		const VkDebugUtilsMessengerCallbackDataEXT * callbackData,
		void * userData)
	{

		switch (severity)
		{
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
			SDL_LogInfo(0, "%s\n", callbackData->pMessage);
			break;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
			SDL_LogWarn(0, "%s\n", callbackData->pMessage);
			break;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
			SDL_Log(0, "%s\n", callbackData->pMessage);
			break;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
			SDL_LogError(0, "%s\n", callbackData->pMessage);
			break;
		default:
			SDL_Log(0, "%s\n", callbackData->pMessage);
			break;
		}

		return VK_TRUE;
	}

	VKAPI_ATTR VkBool32 VKAPI_CALL VulkanDebugReportCallback(
		VkDebugReportFlagsEXT       flags,
		VkDebugReportObjectTypeEXT  objectType,
		uint64_t                    object,
		size_t                      location,
		int32_t                     messageCode,
		const char*                 pLayerPrefix,
		const char*                 pMessage,
		void*                       pUserData) 
	{
		int k = 0;
		k = 1;
		return VK_TRUE;
	}
}


bool Vulkan::setupDebugCallback(Vulkan::VulkanContext & context)
{
    if (!validationLayersEnabled)
        return true; // everything is true about the empty set

	// debug utils
	{
		const char * debugFunctionCreatorName = "vkCreateDebugUtilsMessengerEXT";
		auto debugUtilsMessengerCreator = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(context._instance, debugFunctionCreatorName);
		if (debugUtilsMessengerCreator == nullptr)
		{
#if defined(__APPLE__) // function doesn't seeem to exist with moltenvk
			return true;
#else
			SDL_LogError(0, "Could not create function: %s\n", debugFunctionCreatorName);
			return false;
#endif
		}


		VkDebugUtilsMessengerCreateInfoEXT createInfo;
		memset(&createInfo, 0, sizeof(createInfo));

		createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		createInfo.messageSeverity =
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
		createInfo.messageType =
			VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		createInfo.pNext = nullptr;
		createInfo.flags = 0;
		createInfo.pUserData = nullptr;
		createInfo.pfnUserCallback = VulkanDebugUtilsCallback;


		VkResult debugUtilsCreationResult = debugUtilsMessengerCreator(context._instance, &createInfo, nullptr, &context._debugUtilsCallback);
		if (debugUtilsCreationResult != VK_SUCCESS)
		{
			SDL_LogError(0, "Failed to create callback for method %s\n", debugFunctionCreatorName);
			// TODO: should probably destroy the callbackCreator here
			return false;
		}
	}
    
	// debug report
	{
		const char * debugFunctionCreatorName = "vkCreateDebugReportCallbackEXT";
		auto debugReportMessengerCreator = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(context._instance, debugFunctionCreatorName);
		if (debugReportMessengerCreator == nullptr)
		{
#if defined(__APPLE__) // function doesn't seeem to exist with moltenvk
			return true;
#else
			SDL_LogError(0, "Could not create function: %s\n", debugFunctionCreatorName);
			return false;
#endif
		}


		VkDebugReportCallbackCreateInfoEXT createInfo;
		memset(&createInfo, 0, sizeof(createInfo));

		createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT;
		createInfo.pNext = nullptr;
		createInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | //VK_DEBUG_REPORT_INFORMATION_BIT_EXT |
			VK_DEBUG_REPORT_WARNING_BIT_EXT |
			VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
		createInfo.pUserData = nullptr;
		createInfo.pfnCallback = &VulkanDebugReportCallback;


		VkResult debugReportCreationResult = debugReportMessengerCreator(context._instance, &createInfo, nullptr, &context._debugReportCallback);
		if (debugReportCreationResult != VK_SUCCESS)
		{
			SDL_LogError(0, "Failed to create callback for method %s\n", debugFunctionCreatorName);
			// TODO: should probably destroy the callbackCreator here
			return false;
		}
	}


    
    return true;
}

bool Vulkan::areValidationLayersAvailable(const std::vector<const char*> & validationLayers)
{
    std::vector<VkLayerProperties> layers;
    
    unsigned int layerCount = 0;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    if (layerCount > 0)
    {
        
        layers.resize(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, &layers[0]);
        
        for (const std::string & neededLayer : validationLayers)
        {
            bool found = false;
            for (const auto & layer : layers)
            {
                if (strcmp(neededLayer.c_str(), layer.layerName) == 0)
                {
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                SDL_LogError(0, "Could not find needed validation layer %s\n", neededLayer.c_str());
                layers.clear();
                return false;
            }
        }
        return true;
        
    }
    
    return false;
}

bool Vulkan::loadVulkanLibrary() {
    // load vulkan library (probably SDL already did it)
    if (SDL_Vulkan_LoadLibrary(NULL) != 0) {
        // TODO - add error message
        return false;
    }
    return true;
}

bool Vulkan::loadVulkanFunctions() {
    // vulkan library should already have been loaded by SDL at this point, so we don't need to do it again
    void * procAddr = SDL_Vulkan_GetVkGetInstanceProcAddr();
    return procAddr != nullptr;
}

bool Vulkan::createInstanceAndLoadExtensions(const Vulkan::AppInformation & appInfo, Vulkan::VulkanContext & context)
{
    
    // first count the number of instance extensions
    unsigned int instanceExtensionCount = 0;
    VkResult instanceEnumerateResult = vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtensionCount, nullptr);
    if (instanceEnumerateResult != VK_SUCCESS)
        return false;
    
    std::vector<VkExtensionProperties> instanceExtensions;
    instanceExtensions.resize(instanceExtensionCount);
    instanceEnumerateResult = vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtensionCount, &instanceExtensions[0]);
    if (instanceEnumerateResult != VK_SUCCESS)
        return false;
    
    std::vector<const char *> instanceExtensionNames;
    for (VkExtensionProperties & property : instanceExtensions)
        instanceExtensionNames.push_back((const char *)property.extensionName);
    
    // log all the extensions
    SDL_LogInfo(0, "Vulkan Instance Extensions. Count = %d", instanceExtensionCount);
    for (unsigned int i = 0; i < (unsigned int)instanceExtensionNames.size(); i++)
        SDL_LogInfo(0, "\t%d: %s", i, instanceExtensionNames[i]);
    
    
    unsigned int requiredInstanceExtensionCount = 0;
    if (!SDL_Vulkan_GetInstanceExtensions(appInfo._window, &requiredInstanceExtensionCount, nullptr))
    {
        SDL_LogError(0, "Failed to get number of extensions");
        return false;
    }
    
    std::vector<const char*> requiredInstanceExtensions;
    requiredInstanceExtensions.resize(requiredInstanceExtensionCount);
    if (!SDL_Vulkan_GetInstanceExtensions(appInfo._window, &requiredInstanceExtensionCount, &requiredInstanceExtensions[0]))
    {
        SDL_LogError(0, "Failed to acquire possible extensions error");
        return false;
    }
    
    if (validationLayersEnabled)
    {
#if !defined(__APPLE__)
        requiredInstanceExtensionCount++;
        requiredInstanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif
    }
    
    
    // log all the required extensions
    SDL_LogInfo(0, "Required Vulkan Instance Extensions. Count = %d", requiredInstanceExtensionCount);
    for (unsigned int i = 0; i < requiredInstanceExtensionCount; i++)
        SDL_LogInfo(0, "\t%d: %s", i, requiredInstanceExtensions[i]);
    
    // check if required extensions are available
    {
        unsigned int requiredIndex = 0;
        for (requiredIndex = 0; requiredIndex < requiredInstanceExtensionCount; requiredIndex++)
        {
            const char * requiredExtension = requiredInstanceExtensions[requiredIndex];
            unsigned int instanceIndex = 0;
            for (unsigned int instanceIndex = 0; instanceIndex < instanceExtensionCount; instanceIndex++)
            {
                const char * extension = instanceExtensionNames[instanceIndex];
                if (strcmp(extension, requiredExtension) == 0)
                    break;
            }
            
            if (instanceIndex == instanceExtensionCount)
            {
                SDL_LogError(0, "required vulkan extension %s not found", requiredExtension);
                return false;
            }
        }
    }
    
    VkApplicationInfo vkAppInfo;
    memset(&vkAppInfo, 0, sizeof(vkAppInfo));
    vkAppInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    vkAppInfo.apiVersion = appInfo._descriptor._requiredVulkanVersion;
    vkAppInfo.applicationVersion = 1;
    vkAppInfo.engineVersion = 1;
    vkAppInfo.pApplicationName = appInfo._descriptor._appName.c_str();
    
    VkInstanceCreateInfo instanceCreateInfo;
    memset(&instanceCreateInfo, 0, sizeof(instanceCreateInfo));
    
    
    instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceCreateInfo.pApplicationInfo = &vkAppInfo;
#if !defined(__APPLE__)
	requiredInstanceExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
#endif
    instanceCreateInfo.enabledExtensionCount = requiredInstanceExtensions.size();
    instanceCreateInfo.ppEnabledExtensionNames = &requiredInstanceExtensions[0];
    
    if (validationLayersEnabled)
    {
        static const std::vector<const char*> validationLayers = {
#if defined(__APPLE__)
            "MoltenVK"
#else
            "VK_LAYER_LUNARG_standard_validation"
#endif
        };
        if (areValidationLayersAvailable(validationLayers))
        {
            instanceCreateInfo.enabledLayerCount = (uint32_t)validationLayers.size();
            instanceCreateInfo.ppEnabledLayerNames = &validationLayers[0];
        }
    }
    
    VkResult result = vkCreateInstance(&instanceCreateInfo, nullptr, &context._instance);
    if (result != VK_SUCCESS)
        return false;
    
    return true;
    
}

bool Vulkan::createVulkanSurface(SDL_Window * window, Vulkan::VulkanContext & context) {
    
    context._surface = VK_NULL_HANDLE;
    if (SDL_Vulkan_CreateSurface(window, context._instance, &context._surface) == 0)
        return false;
    
    return true;
}

bool Vulkan::enumeratePhysicalDevices(Vulkan::AppInformation & appInfo, Vulkan::VulkanContext & context)
{
    uint32_t deviceCount = 0;
    // first count the number of physical devices by passing in NULL as the last argument
    VkResult countResult = vkEnumeratePhysicalDevices(context._instance, &deviceCount, NULL);
    if (countResult != VK_SUCCESS) {
        SDL_LogError(0, "vkEnumeratePhysicalDevices returned error code %d", countResult);
        return false;
    }
    
    if (deviceCount == 0) {
        SDL_LogError(0, "vkEnumeratePhysicalDevices returned 0 devices");
        return false;
    }
    
    std::vector<VkPhysicalDevice> devices(deviceCount);
    VkResult enumerateResult = vkEnumeratePhysicalDevices(context._instance, &deviceCount, &devices[0]);
    if (enumerateResult != VK_SUCCESS) {
        SDL_LogError(0, "vkEnumeratePhysicalDevices returned error code %d", countResult);
        return false;
    }
    
    unsigned int deviceIndex = 0;
    for (; deviceIndex < deviceCount; deviceIndex++) {
        VkPhysicalDevice device = devices[deviceIndex];
        vkGetPhysicalDeviceProperties(device, &context._deviceProperties);
        
        if (context._deviceProperties.apiVersion < appInfo._descriptor._requiredVulkanVersion)
            continue;
        
        break;
    }
    
    if (deviceIndex == deviceCount)
        return false;
    
    appInfo._physicalDevices = devices;
    return true;
}

// prioritise discrete over integrated
bool Vulkan::choosePhysicalDevice(AppInformation &appInfo, VulkanContext & vulkanContext) {
    bool foundDevice = false;
    unsigned int currentPhysicalDevice = 0;
    VkPhysicalDeviceProperties currentPhysicalProperties;
    vkGetPhysicalDeviceProperties(appInfo._physicalDevices[currentPhysicalDevice], &currentPhysicalProperties);
    for (unsigned int i = 0; i < (unsigned int)appInfo._physicalDevices.size(); i++)
    {
        VkPhysicalDeviceProperties physicalProperties;
        vkGetPhysicalDeviceProperties(appInfo._physicalDevices[i], &physicalProperties);
        if (!foundDevice)
        {
            currentPhysicalDevice = i;
            currentPhysicalProperties = physicalProperties;
            foundDevice = true;
        }
        else if (foundDevice && currentPhysicalProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU && physicalProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
        {
            currentPhysicalDevice = i;
            currentPhysicalProperties = physicalProperties;
        }
    }
    
    if (!foundDevice)
        return false;
    
    appInfo._chosenPhysicalDevice = currentPhysicalDevice;
    vulkanContext._physicalDevice = appInfo._physicalDevices[currentPhysicalDevice];
    SDL_LogInfo(0, "Chosen Vulkan Physical Device = %s. Driver version = %d\n", currentPhysicalProperties.deviceName, currentPhysicalProperties.driverVersion);
    
    return true;
}

bool Vulkan::lookupDeviceExtensions(AppInformation &appInfo) {
    uint32_t deviceExtensionsCount = 0;
    VkResult enumerateDeviceExtensionsResult = vkEnumerateDeviceExtensionProperties(appInfo._physicalDevices[appInfo._chosenPhysicalDevice],
                                                                                    nullptr,
                                                                                    &deviceExtensionsCount,
                                                                                    nullptr);
    if (enumerateDeviceExtensionsResult != VK_SUCCESS)
    {
        return false;
    }
    
    if (deviceExtensionsCount > 0)
    {
        std::vector<VkExtensionProperties> deviceExtensions(deviceExtensionsCount);
        vkEnumerateDeviceExtensionProperties(appInfo._physicalDevices[appInfo._chosenPhysicalDevice],
                                             nullptr,
                                             &deviceExtensionsCount,
                                             &deviceExtensions[0]);
        
        appInfo._deviceExtensions = deviceExtensions;
    }
    return true;
}

bool Vulkan::createDevice(AppInformation & appInfo, VulkanContext & context)
{
    VkDeviceQueueCreateInfo deviceQueueCreateInfo;
    VkDeviceCreateInfo deviceCreateInfo;
    memset(&deviceQueueCreateInfo, 0, sizeof(deviceQueueCreateInfo));
    memset(&deviceCreateInfo, 0, sizeof(deviceCreateInfo));
    
    deviceQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    deviceQueueCreateInfo.queueFamilyIndex = appInfo._chosenPhysicalDevice;
    deviceQueueCreateInfo.queueCount = 1;
    static constexpr float fQueuePriority = 1.0f;
    deviceQueueCreateInfo.pQueuePriorities = &fQueuePriority;
    
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.queueCreateInfoCount = 1;
    deviceCreateInfo.pQueueCreateInfos = &deviceQueueCreateInfo;
    deviceCreateInfo.pEnabledFeatures = nullptr;
    
    static const char * deviceExtensionNames[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };
    static const unsigned int numExtensionNames = sizeof(deviceExtensionNames) / sizeof(const char*);
    deviceCreateInfo.ppEnabledExtensionNames = &deviceExtensionNames[0];
    deviceCreateInfo.enabledExtensionCount = numExtensionNames;
    
    VkResult creationResult = vkCreateDevice(appInfo._physicalDevices[appInfo._chosenPhysicalDevice], &deviceCreateInfo, nullptr /* no allocation callbacks at this time */, &context._device);
    return creationResult == VK_SUCCESS;
}

bool Vulkan::createQueue(AppInformation & appInfo, VulkanContext & context)
{
    vkGetDeviceQueue(context._device, appInfo._chosenPhysicalDevice, 0, &context._graphicsQueue);
    vkGetDeviceQueue(context._device, appInfo._chosenPhysicalDevice, 0, &context._presentQueue);
    return true;
}

bool Vulkan::createSwapChain(AppInformation & appInfo, VulkanContext & context)
{
    // get surface capabilities
    VkResult surfaceCapabilitiesResult = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(context._physicalDevice, context._surface, &context._surfaceCapabilities);
    if (surfaceCapabilitiesResult != VK_SUCCESS)
        return false;
    
    unsigned int surfaceFormatsCount = 0;
    VkResult surfaceFormatsCountResult = vkGetPhysicalDeviceSurfaceFormatsKHR(context._physicalDevice, context._surface, &surfaceFormatsCount, nullptr);
    if (surfaceFormatsCountResult != VK_SUCCESS || surfaceFormatsCount == 0)
        return false;
    
    appInfo._surfaceFormats.resize(surfaceFormatsCount);
    VkResult surfaceFormatsResult = vkGetPhysicalDeviceSurfaceFormatsKHR(context._physicalDevice, context._surface, &surfaceFormatsCount, &appInfo._surfaceFormats[0]);
    if (surfaceFormatsResult != VK_SUCCESS || surfaceFormatsCount == 0)
        return false;
    
    VkSwapchainCreateInfoKHR swapChainCreateInfo;
    memset(&swapChainCreateInfo, 0, sizeof(swapChainCreateInfo));
    int width = 0, height = 0;
    
    const unsigned int imageCount = std::max<uint32_t>(std::min<uint32_t>(3, context._surfaceCapabilities.maxImageCount), context._surfaceCapabilities.minImageCount);
    if (imageCount == 0)
        return false;
    
#if defined(__APPLE__)
    SDL_GetWindowSize(appInfo._window, &width, &height);
#else
    SDL_Vulkan_GetDrawableSize(appInfo._window, &width, &height);
#endif
    if (width <= 0 || height <= 0)
        return false;
    
    context._surfaceFormat = appInfo._surfaceFormats[0];
    context._swapChainSize.width = width;
    context._swapChainSize.height = height;
    
    swapChainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapChainCreateInfo.surface = context._surface;
    swapChainCreateInfo.minImageCount = imageCount;
    swapChainCreateInfo.imageFormat = context._surfaceFormat.format;
    swapChainCreateInfo.imageColorSpace = context._surfaceFormat.colorSpace;
    swapChainCreateInfo.imageExtent = context._swapChainSize;
    swapChainCreateInfo.imageArrayLayers = 1;
    swapChainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapChainCreateInfo.preTransform = context._surfaceCapabilities.currentTransform;
    swapChainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapChainCreateInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    swapChainCreateInfo.clipped = VK_TRUE;
    swapChainCreateInfo.oldSwapchain = context._swapChain;
    
    VkResult swapChainCreationResult = vkCreateSwapchainKHR(context._device, &swapChainCreateInfo, nullptr, &context._swapChain);
    if (swapChainCreationResult != VK_SUCCESS)
        return false;
    
    // if success check how many images will actually be in the swap chain
    unsigned int vkImageCount = 0;
    VkResult getVkImageCountFromSwapChainResult = vkGetSwapchainImagesKHR(context._device, context._swapChain, &vkImageCount, nullptr);
    if (getVkImageCountFromSwapChainResult != VK_SUCCESS || vkImageCount == 0)
        return false;
    
    context._rawImages.resize(vkImageCount);
    vkGetSwapchainImagesKHR(context._device, context._swapChain, &vkImageCount, &context._rawImages[0]);
    
    return true;
}

bool Vulkan::createColorBuffers(VulkanContext & context)
{
    for (VkImage & image : context._rawImages)
    {
        VkImageViewCreateInfo createInfo;
        memset(&createInfo, 0, sizeof(createInfo));
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = image;
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = context._surfaceFormat.format;
        
        // do not remap colors
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;
        
        VkImageView imageView;
        VkResult imageViewCreationResult = vkCreateImageView(context._device, &createInfo, nullptr, &imageView);
        if (imageViewCreationResult != VK_SUCCESS)
            return false;
        
        context._colorBuffers.push_back(imageView);
    }
    
    return true;
}

bool Vulkan::createRenderPass(VulkanContext & vulkanContext)
{
    VkAttachmentReference colorAttachmentReference;
    memset(&colorAttachmentReference, 0, sizeof(colorAttachmentReference));
    colorAttachmentReference.attachment = 0;
    colorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    
    VkSubpassDescription subpassDescription;
    memset(&subpassDescription, 0, sizeof(subpassDescription));
    subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDescription.colorAttachmentCount = 1;
    subpassDescription.pColorAttachments = &colorAttachmentReference;
    
    VkAttachmentDescription colorAttachment;
    memset(&colorAttachmentReference, 0, sizeof(colorAttachmentReference));
    colorAttachment.format = vulkanContext._surfaceFormat.format;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    
    
    VkRenderPassCreateInfo createInfo;
    memset(&createInfo, 0, sizeof(createInfo));
    createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    createInfo.attachmentCount = 1;
    createInfo.pAttachments = &colorAttachment;
    createInfo.subpassCount = 1;
    createInfo.pSubpasses = &subpassDescription;
    
    VkResult createRenderPassResult = vkCreateRenderPass(vulkanContext._device, &createInfo, nullptr, &vulkanContext._renderPass);
    if (createRenderPassResult != VK_SUCCESS)
        return false;
    
    return true;
}

bool Vulkan::createFrameBuffers(VulkanContext & vulkanContext)
{
    for (VkImageView & imageview : vulkanContext._colorBuffers)
    {
        VkFramebufferCreateInfo createInfo;
        memset(&createInfo, 0, sizeof(createInfo));
        
        createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        createInfo.renderPass = vulkanContext._renderPass;
        createInfo.attachmentCount = 1;
        createInfo.pAttachments = &imageview;
        createInfo.width = vulkanContext._swapChainSize.width;
        createInfo.height = vulkanContext._swapChainSize.height;
        
        VkFramebuffer frameBuffer;
        VkResult frameBufferCreationResult = vkCreateFramebuffer(vulkanContext._device, &createInfo, nullptr, &frameBuffer);
        if (frameBufferCreationResult != VK_SUCCESS)
            return false;
        
        vulkanContext._frameBuffers.push_back(frameBuffer);
    }
    
    return true;
}

std::vector<VkShaderModule> Vulkan::createShaderModules(AppInformation & appInfo, VulkanContext & context)
{
    std::vector<VkShaderModule> shaderModules;
    for (Shader & shader : appInfo._shaders)
    {
        VkShaderModuleCreateInfo createInfo;
        memset(&createInfo, 0, sizeof(createInfo));
        
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = shader._byteCode.size();
        createInfo.pCode = reinterpret_cast<uint32_t*>(&shader._byteCode[0]);
        
        VkShaderModule module;
        VkResult result;
        result = vkCreateShaderModule(context._device, &createInfo, nullptr, &module);
        if (result != VK_SUCCESS)
        {
            SDL_LogError(0, "Failed to create shader module for file %s with error %d\n", shader._filename.c_str(), (int)result);
            return std::vector<VkShaderModule>();
        }
        
        shaderModules.push_back(module);
    }
    
    // now that all modules have been created, create the pipeline
    return shaderModules;
}

bool Vulkan::createFixedState(AppInformation & appInfo, VulkanContext & context)
{
    return true;
}

bool Vulkan::createGraphicsPipeline(AppInformation & appInfo, VulkanContext & context, const std::vector<VkShaderModule> & shaderModules)
{
    const std::vector<Shader> & shaders = appInfo._shaders;
    std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
    for (unsigned int i = 0; i < (unsigned int)shaders.size() ; i++)
    {
        VkPipelineShaderStageCreateInfo createInfo;
        memset(&createInfo, 0, sizeof(VkPipelineShaderStageCreateInfo));
        
        createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        switch (shaders[i]._type)
        {
            case ShaderType::Vertex:
                createInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
                break;
            case ShaderType::Fragment:
                createInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
                break;
        }
        createInfo.module = shaderModules[i];
        createInfo.pName = "main";
        
        shaderStages.push_back(createInfo);
        
    }
    
    VkGraphicsPipelineCreateInfo createInfo;
    memset(&createInfo, 0, sizeof(VkGraphicsPipelineCreateInfo));
    
    createInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    createInfo.stageCount = (uint32_t)shaderStages.size();
	createInfo.pStages = (createInfo.stageCount == 0) ? nullptr : &shaderStages[0];

    // Pipeline Vertex Input State
    VkPipelineVertexInputStateCreateInfo vertexInputInfo;
	VkVertexInputBindingDescription vertexBindingDescription = appInfo._getBindingDescription();
	std::array<VkVertexInputAttributeDescription, 2> vertexAttributeDescription = appInfo._getAttributes();
    memset(&vertexInputInfo, 0, sizeof(VkPipelineVertexInputStateCreateInfo));
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &vertexBindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount = (uint32_t)vertexAttributeDescription.size();
    vertexInputInfo.pVertexAttributeDescriptions = &vertexAttributeDescription[0];
    createInfo.pVertexInputState = &vertexInputInfo;
    
    // Pipeline Input Assembly State
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo;
    memset(&inputAssemblyInfo, 0, sizeof(VkPipelineInputAssemblyStateCreateInfo));
    inputAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssemblyInfo.primitiveRestartEnable = VK_FALSE;
    createInfo.pInputAssemblyState = &inputAssemblyInfo;
    
    // viewport
    VkViewport viewport;
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = (float)context._swapChainSize.width;
    viewport.height = (float)context._swapChainSize.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    
    // scissor
    VkRect2D scissor;
    scissor.offset = { 0,0 };
    scissor.extent = context._swapChainSize;
    
    // viewport
    VkPipelineViewportStateCreateInfo viewportStateCreateInfo;
    memset(&viewportStateCreateInfo, 0, sizeof(VkPipelineViewportStateCreateInfo));
    viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportStateCreateInfo.viewportCount = 1;
    viewportStateCreateInfo.pViewports = &viewport;
    viewportStateCreateInfo.scissorCount = 1;
    viewportStateCreateInfo.pScissors = &scissor;
    createInfo.pViewportState = &viewportStateCreateInfo;
    
    // basic rasterization parameters
    VkPipelineRasterizationStateCreateInfo rasterizerCreateInfo;
    memset(&rasterizerCreateInfo, 0, sizeof(VkPipelineRasterizationStateCreateInfo));
    rasterizerCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizerCreateInfo.depthClampEnable = VK_FALSE;
    rasterizerCreateInfo.rasterizerDiscardEnable = VK_FALSE;
    rasterizerCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizerCreateInfo.lineWidth = 1.0f;
    rasterizerCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizerCreateInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizerCreateInfo.depthBiasEnable = VK_FALSE;
    rasterizerCreateInfo.depthBiasConstantFactor = 0.0f;
    rasterizerCreateInfo.depthBiasClamp = 0.0f;
    rasterizerCreateInfo.depthBiasSlopeFactor = 0.0f;
    createInfo.pRasterizationState = &rasterizerCreateInfo;
    
    // multisampling
    VkPipelineMultisampleStateCreateInfo multisamplingCreateInfo;
    memset(&multisamplingCreateInfo, 0, sizeof(VkPipelineMultisampleStateCreateInfo));
    multisamplingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisamplingCreateInfo.sampleShadingEnable = VK_FALSE;
    multisamplingCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisamplingCreateInfo.minSampleShading = 1.0f;
    multisamplingCreateInfo.pSampleMask = nullptr;
    multisamplingCreateInfo.alphaToCoverageEnable = VK_FALSE;
    multisamplingCreateInfo.alphaToOneEnable = VK_FALSE;
    createInfo.pMultisampleState = &multisamplingCreateInfo;
    
    createInfo.pDepthStencilState = nullptr;
    
    VkPipelineColorBlendAttachmentState colorBlendAttachmentCreateInfo;
    memset(&colorBlendAttachmentCreateInfo, 0, sizeof(VkPipelineColorBlendAttachmentState));
    colorBlendAttachmentCreateInfo.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachmentCreateInfo.blendEnable = VK_FALSE;
    colorBlendAttachmentCreateInfo.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachmentCreateInfo.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachmentCreateInfo.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachmentCreateInfo.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachmentCreateInfo.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachmentCreateInfo.alphaBlendOp = VK_BLEND_OP_ADD;
    
    VkPipelineColorBlendStateCreateInfo colorBlendingCreateInfo;
    memset(&colorBlendingCreateInfo, 0, sizeof(VkPipelineColorBlendStateCreateInfo));
    colorBlendingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlendingCreateInfo.logicOpEnable = VK_FALSE;
    colorBlendingCreateInfo.logicOp = VK_LOGIC_OP_COPY;
    colorBlendingCreateInfo.attachmentCount = 1;
    colorBlendingCreateInfo.pAttachments = &colorBlendAttachmentCreateInfo;
    colorBlendingCreateInfo.blendConstants[0] = 0.0f;
    colorBlendingCreateInfo.blendConstants[1] = 0.0f;
    colorBlendingCreateInfo.blendConstants[2] = 0.0f;
    colorBlendingCreateInfo.blendConstants[3] = 0.0f;
    createInfo.pColorBlendState = &colorBlendingCreateInfo;
    createInfo.pDynamicState = nullptr;
    
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo;
    memset(&pipelineLayoutCreateInfo, 0, sizeof(VkPipelineLayoutCreateInfo));
    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.setLayoutCount = 0;
    pipelineLayoutCreateInfo.pSetLayouts = nullptr;
    pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
    pipelineLayoutCreateInfo.pPushConstantRanges = nullptr;
    
    
    VkPipelineLayout pipelineLayout;
    if (vkCreatePipelineLayout(context._device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
    {
        SDL_LogError(0, "Failed to create graphics pipeline layout\n");
        return false;
    }
    createInfo.layout = pipelineLayout;
    createInfo.renderPass = context._renderPass;
    createInfo.subpass = 0;
    createInfo.basePipelineHandle = VK_NULL_HANDLE;
    createInfo.basePipelineIndex = -1;
    
	VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo;
	memset(&dynamicStateCreateInfo, 0, sizeof(VkPipelineDynamicStateCreateInfo));
	dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	VkDynamicState dynamicState[] = {
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR
	};
	dynamicStateCreateInfo.dynamicStateCount = 2;
	dynamicStateCreateInfo.pDynamicStates = &dynamicState[0];
	createInfo.pDynamicState = &dynamicStateCreateInfo;

	VkPipelineDepthStencilStateCreateInfo depthStencilState;
	memset(&depthStencilState, 0, sizeof(VkPipelineDepthStencilStateCreateInfo));
	depthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencilState.depthTestEnable = VK_TRUE;
	depthStencilState.depthWriteEnable = VK_TRUE;
	depthStencilState.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
	createInfo.pDepthStencilState = &depthStencilState;


    // TODO: missing some stuff here
    VkPipeline graphicsPipeline;
    if (vkCreateGraphicsPipelines(context._device, VK_NULL_HANDLE, 1, &createInfo, nullptr, &graphicsPipeline) != VK_SUCCESS)
    {
        SDL_LogError(0, "Failed to create vulkan graphics pipeline\n");
        return false;
    }
    context._pipeline = graphicsPipeline;
    
    
    return true;
}

bool Vulkan::createCommandPool(AppInformation & appInfo, VulkanContext & context)
{
    VkCommandPoolCreateInfo createInfo;
    memset(&createInfo, 0, sizeof(VkCommandPoolCreateInfo));
    createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    createInfo.queueFamilyIndex = appInfo._chosenPhysicalDevice;
    createInfo.flags = 0;
    VkCommandPool commandPool;
    if(vkCreateCommandPool(context._device, &createInfo, nullptr, &commandPool) != VK_SUCCESS)
    {
        SDL_LogError(0, "Failed to create command pool\n");
        return false;
    }
    context._commandPool = commandPool;
    return true;
}

bool Vulkan::createCommandBuffers(AppInformation & appInfo, VulkanContext & context)
{
    std::vector<VkCommandBuffer> commandBuffers(context._frameBuffers.size());
    
    VkCommandBufferAllocateInfo commandBufferAllocateInfo;
    memset(&commandBufferAllocateInfo, 0, sizeof(VkCommandBufferAllocateInfo));
    commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocateInfo.commandPool = context._commandPool;
    commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferAllocateInfo.commandBufferCount = (unsigned int)commandBuffers.size();
    if(vkAllocateCommandBuffers(context._device, &commandBufferAllocateInfo, &commandBuffers[0]) != VK_SUCCESS)
    {
        SDL_LogError(0, "Failed to create VkCommandbufferAllocateInfo\n");
        return false;
    }
    
    // do a basic recording of the command buffers
    for(unsigned int i=0 ; i < (unsigned int)commandBuffers.size() ; i++)
    {
        VkCommandBufferBeginInfo beginInfo;
        beginInfo.sType = VK_STRUCTURE_TYPE_DEVICE_GROUP_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
        if(vkBeginCommandBuffer(commandBuffers[i], &beginInfo) != VK_SUCCESS)
        {
            SDL_LogError(0, "call to vkBeginCommandBuffer failed\n");
            return false;
        }
        
        VkRenderPassBeginInfo renderPassBeginInfo;
        memset(&renderPassBeginInfo, 0, sizeof(VkRenderPassBeginInfo));
        renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassBeginInfo.renderPass = context._renderPass;
        renderPassBeginInfo.framebuffer = context._frameBuffers[i];
        renderPassBeginInfo.renderArea.offset = {0,0};
        renderPassBeginInfo.renderArea.extent = context._swapChainSize;
        
        VkClearValue clearColorValue{0.0f, 0.0f, 0.0f, 1.0f};
        renderPassBeginInfo.clearValueCount = 1;
        renderPassBeginInfo.pClearValues = &clearColorValue;
        
        vkCmdBeginRenderPass(commandBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, context._pipeline);
        
        VkBuffer vertexBuffer[] = {context._vertexBuffer._buffer};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(commandBuffers[i], 0, 1, vertexBuffer, offsets);
        vkCmdBindIndexBuffer(commandBuffers[i], context._indexBuffer._buffer, 0, VK_INDEX_TYPE_UINT16);
        
        vkCmdDrawIndexed(commandBuffers[i], 6 , 1, 0, 0, 0);
        vkCmdEndRenderPass(commandBuffers[i]);
        
        if(vkEndCommandBuffer(commandBuffers[i]) != VK_SUCCESS)
        {
            SDL_LogError(0,"Call to vkEndCommandBuffer failed (i=%d)\n", i);
            return false;
        }
    }
    
    context._commandBuffers = commandBuffers;
    
    return true;
}

std::vector<VkFence> Vulkan::createFences(VulkanContext & context)
{
    std::vector<VkFence> fences(context._frameBuffers.size());
    for(unsigned int i=0 ; i < (unsigned int)fences.size() ; i++)
    {
        VkFenceCreateInfo createInfo;
        memset(&createInfo, 0, sizeof(VkFenceCreateInfo));
        createInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        createInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        
        if(vkCreateFence(context._device, &createInfo, nullptr, &fences[i]) != VK_SUCCESS)
        {
            SDL_LogError(0, "Failed to create fences (%d)\n", i);
            return std::vector<VkFence>();
        }
    }
    return fences;
    
}

std::vector<VkSemaphore> Vulkan::createSemaphores(VulkanContext & context)
{
    std::vector<VkSemaphore> semaphores(context._frameBuffers.size());
    for(unsigned int i=0 ; i < (unsigned int)semaphores.size() ; i++)
    {
        VkSemaphoreCreateInfo createInfo;
        memset(&createInfo, 0, sizeof(VkSemaphoreCreateInfo));
        createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        
        if(vkCreateSemaphore(context._device, &createInfo, nullptr, &semaphores[i]) != VK_SUCCESS)
        {
            SDL_LogError(0, "Failed to create semaphore (%d)\n", i);
            return std::vector<VkSemaphore>();
        }
    }
    return semaphores;
}

bool Vulkan::createSemaphores(AppInformation & appInfo, VulkanContext & context)
{
    context._imageAvailableSemaphore = createSemaphores(context);
    context._renderFinishedSemaphore = createSemaphores(context);
    context._fences = createFences(context);
    
    return context._imageAvailableSemaphore.size() == context._renderFinishedSemaphore.size()
    && context._imageAvailableSemaphore.size() == context._fences.size()
    && !context._imageAvailableSemaphore.empty();
}

int Vulkan::findMemoryType(VulkanContext & context, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(context._physicalDevice, &memProperties);
    
    for(uint32_t i=0 ; i <  memProperties.memoryTypeCount ; i++ )
    {
        if((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties)==properties)
            return i;
    }
    
    return -1;
}


bool Vulkan::createBuffer(VulkanContext & context, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, BufferDescriptor & bufDesc)
{
    VkBufferCreateInfo createInfo;
    memset(&createInfo, 0, sizeof(VkBufferCreateInfo));
    createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    createInfo.size = size;
    createInfo.usage = usage;
    createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    VkBuffer vertexBuffer;
    if(vkCreateBuffer(context._device, &createInfo, nullptr, &vertexBuffer) != VK_SUCCESS)
    {
        SDL_LogError(0, "Failed to create vertex buffer\n");
        return false;
    }
    
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(context._device, vertexBuffer, &memRequirements);
    int memType = findMemoryType(context, memRequirements.memoryTypeBits, properties);
    if(memType<0)
    {
        SDL_LogError(0, "Required type of memory not available for this vertex buffer\n");
        return false;
    }
    
    VkMemoryAllocateInfo allocInfo;
    memset(&allocInfo, 0, sizeof(VkMemoryAllocateInfo));
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = (uint32_t)memType;
    
    VkDeviceMemory vertexBufferMemory;
    if(vkAllocateMemory(context._device, &allocInfo, nullptr, &vertexBufferMemory) != VK_SUCCESS)
    {
        SDL_LogError(0, "Failed to allocate memory necessary for the vertex buffer\n");
        return false;
    }
    
    if(vkBindBufferMemory(context._device, vertexBuffer, vertexBufferMemory, 0) != VK_SUCCESS)
    {
        SDL_LogError(0, "Failed to bind vertex buffer memory\n");
        return false;
    }
    
    bufDesc._buffer = vertexBuffer;
    bufDesc._memory = vertexBufferMemory;
    return true;
}

bool Vulkan::createVertexOrIndexBuffer(VulkanContext & context, const void * srcData, VkDeviceSize bufferSize, BufferDescriptor & result)
{
    
    BufferDescriptor stagingBufferDescriptor;
    if(!createBuffer(context, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBufferDescriptor))
    {
        SDL_LogError(0, "Failed to crete buffer of size %d bytes\n", (int)bufferSize);
        return false;
    }
    
    BufferDescriptor vertexBufferDescriptor;
    if(!createBuffer(context, bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBufferDescriptor))
    {
        SDL_LogError(0, "Failed to crete buffer of size %d bytes\n", (int)bufferSize);
        return false;
    }
    
    stagingBufferDescriptor.fill(context._device, srcData, bufferSize);
    vertexBufferDescriptor.copyFrom(context._device, context._commandPool, context._graphicsQueue, stagingBufferDescriptor, bufferSize);
    
    result = vertexBufferDescriptor;
    return true;
    
}

bool Vulkan::createIndexBuffer(AppInformation & appInfo, VulkanContext & context, unsigned int bufferIndex)
{
	std::vector<unsigned char> indexData;
	appInfo._createBuffer(BufferType::Index, bufferIndex, indexData);
	const void * data = indexData.data();
	const VkDeviceSize bufferSize =  indexData.size();
    createVertexOrIndexBuffer(context, data, bufferSize, context._indexBuffer);
    return true;
}

bool Vulkan::createVertexBuffer(AppInformation & appInfo, VulkanContext & context, unsigned int bufferIndex )
{
	std::vector<unsigned char> vertexData;
	appInfo._createBuffer(BufferType::Index, bufferIndex, vertexData);
	const void * data = vertexData.data();
	const VkDeviceSize bufferSize = vertexData.size();
    createVertexOrIndexBuffer(context, data, bufferSize, context._vertexBuffer);
    return true;
}

bool Vulkan::handleVulkanSetup(AppInformation & appInfo, VulkanContext & context)
{
    if (!loadVulkanLibrary()) {
        SDL_LogError(0, "Failed to load vulkan library");
        return false;
    }
    
    if (!loadVulkanFunctions()) {
        SDL_LogError(0, "Failed to load vulkan functions");
        return false;
    }
    
    if (!createInstanceAndLoadExtensions(appInfo, context)) {
        SDL_LogError(0, "Failed to create instance and load extensions");
        return false;
    }
    
    if (validationLayersEnabled && !setupDebugCallback(context))
    {
        SDL_LogError(0, "Failed to setup requested debug callback\n");
        return false;
    }
    
    if (!createVulkanSurface(appInfo._window, context)) {
        SDL_LogError(0, "Failed to create vulkan surface");
        return false;
    }
    
    if (!enumeratePhysicalDevices(appInfo, context)) {
        SDL_LogError(0, "Failed to enumerate and choose device");
        return false;
    }
    
    // choose a gpu
    if (!choosePhysicalDevice(appInfo, context)) {
        SDL_LogError(0, "Failed to choose appropriate physical device");
        return false;
    }
    
    // load extensions
    if (!lookupDeviceExtensions(appInfo))
    {
        SDL_LogError(0, "Failed to enumerate device extensions!");
        return false;
    }
    
    if (!createDevice(appInfo, context))
    {
        SDL_LogError(0, "Failed to create device!");
        return false;
    }
    
    if (!createQueue(appInfo, context))
    {
        SDL_LogError(0, "Failed to create queue!");
        return false;
    }
    
    if (!createSwapChain(appInfo, context))
    {
        SDL_LogError(0, "Failed to create and setup swap chain!");
        return false;
    }
    
    if (!createColorBuffers(context))
    {
        SDL_LogError(0, "Failed to create color buffers\n");
        return false;
    }
    
    if (!createRenderPass(context))
    {
        SDL_LogError(0, "Failed to create render pass\n");
        return false;
    }
    
    std::vector<VkShaderModule> shaderModules = createShaderModules(appInfo, context);
    if (shaderModules.empty() || shaderModules.size() != appInfo._shaders.size())
    {
        SDL_LogError(0, "Failed to create shader modules\n");
        return false;
    }
    
    if (!createFixedState(appInfo, context))
    {
        SDL_LogError(0, "Failed to create fixed state!");
        return false;
    }
    
    
    if (!createGraphicsPipeline(appInfo, context, shaderModules))
    {
        SDL_LogError(0, "Failed to create graphics pipeline\n");
        return false;
    }
    
    if (!createFrameBuffers(context))
    {
        SDL_LogError(0, "Failed to create frame buffers\n");
        return false;
    }
    
    if(!createCommandPool(appInfo, context))
    {
        SDL_LogError(0, "Failed to create command pool\n");
        return false;
    }
    
	for (int i = 0; i < appInfo._numBuffers(BufferType::Vertex); i++)
	{
		if (!createVertexBuffer(appInfo, context, i))
		{
			SDL_LogError(0, "Failed to create vertex buffer\n");
			return false;
		}

	}
    
	for (int i = 0; i < appInfo._numBuffers(BufferType::Index); i++)
	{
		if (!createIndexBuffer(appInfo, context, i))
		{
			SDL_LogError(0, "Failed to create index buffer\n");
			return false;
		}
	}
    
    if(!createCommandBuffers(appInfo, context))
    {
        SDL_LogError(0, "Failed to create command buffers\n");
        return false;
    }
    
    if(!createSemaphores(appInfo, context))
    {
        SDL_LogError(0, "Failed to create semaphores\n");
        return false;
    }
    
    return true;
}
