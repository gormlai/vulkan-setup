#ifndef _VULKAN_SETUP_H_
#define _VULKAN_SETUP_H_

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <string>
#include <vector>
#include <algorithm>
#include <array>
#include <functional>

/*
 LICENSE - this file is public domain
 
 This is free and unencumbered software released into the public domain.
 
 Anyone is free to copy, modify, publish, use, compile, sell, or
 distribute this software, either in source code form or as a compiled
 binary, for any purpose, commercial or non-commercial, and by any
 means.
 
 In jurisdictions that recognize copyright laws, the author or authors
 of this software dedicate any and all copyright interest in the
 software to the public domain. We make this dedication for the benefit
 of the public at large and to the detriment of our heirs and
 successors. We intend this dedication to be an overt act of
 relinquishment in perpetuity of all present and future rights to this
 software under copyright law.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 OTHER DEALINGS IN THE SOFTWARE.
 
 For more information, please refer to <http://unlicense.org/>
 
 */

namespace Vulkan
{
    extern bool validationLayersEnabled;
    
	template<typename PosType, typename ColorType>
    struct Vertex
    {
        PosType _position;
        ColorType _color;
        
		static VkVertexInputBindingDescription getBindingDescription()
		{
			VkVertexInputBindingDescription bindingDescription;
			memset(&bindingDescription, 0, sizeof(VkVertexInputBindingDescription));

			bindingDescription.binding = 0;
			bindingDescription.stride = sizeof(Vertex);
			bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

			return bindingDescription;
		}

		static std::array<VkVertexInputAttributeDescription, 2> getAttributes()
		{
			std::array<VkVertexInputAttributeDescription, 2> attributes = {};

			attributes[0].binding = 0;
			attributes[0].location = 0;
			attributes[0].format = VK_FORMAT_R32G32_SFLOAT;
			attributes[0].offset = offsetof(Vertex, _position);

			attributes[1].binding = 0;
			attributes[1].location = 1;
			attributes[1].format = VK_FORMAT_R32G32B32_SFLOAT;
			attributes[1].offset = offsetof(Vertex, _color);
			return attributes;
		}

    };
    
    enum class ShaderType
    {
        Vertex,
        Fragment
    };

	enum class BufferType
	{
		Index,
		Vertex,
	};
    
    struct Shader
    {
        std::string _filename;
        std::vector<char> _byteCode;
        ShaderType _type;
        
    public:
        Shader(const std::string & filename, ShaderType type);
    };
    
    struct AppDescriptor
    {
        std::string _appName;
        uint32_t _requiredVulkanVersion;
        
        AppDescriptor()
        : _requiredVulkanVersion(0)
        {
        }
    };
    
    struct AppInformation
    {
        AppDescriptor _descriptor;
        SDL_Window * _window;
        std::vector<VkPhysicalDevice> _physicalDevices;
        unsigned int _chosenPhysicalDevice;
        std::vector<VkExtensionProperties> _deviceExtensions;
        std::vector<VkSurfaceFormatKHR> _surfaceFormats;
        
        std::vector<Shader> _shaders;

		std::function<int (BufferType)> _numBuffers = [](BufferType) { return 0;  };
		std::function<void(BufferType, unsigned int, std::vector<unsigned char> &)> _createBuffer = [](BufferType, unsigned int, std::vector<unsigned char> &) {};


		std::function <VkVertexInputBindingDescription()> _getBindingDescription = []() { return VkVertexInputBindingDescription(); };
		std::function < std::array<VkVertexInputAttributeDescription, 2>()> _getAttributes = []() {return std::array<VkVertexInputAttributeDescription, 2>(); };
        
        AppInformation();
    };
    
    struct BufferDescriptor
    {
        VkBuffer _buffer;
        VkDeviceMemory _memory;
        
        bool fill(VkDevice device, const void * srcData, VkDeviceSize amount);
        bool copyFrom(VkDevice device, VkCommandPool commandPool, VkQueue queue, BufferDescriptor & src, VkDeviceSize amount);
    };

	struct VulkanMesh
	{
		BufferDescriptor _vertexBuffer;
		BufferDescriptor _indexBuffer;
		unsigned int _numIndices;

		VulkanMesh()
		{
			memset(this, 0, sizeof(VulkanMesh));
		}
	};
    
    struct VulkanContext
    {
        VkInstance _instance;
        VkPhysicalDevice _physicalDevice;
        VkDevice _device;
        VkPhysicalDeviceProperties _deviceProperties;
        VkPhysicalDeviceFeatures _deviceFeatures;
        
        VkQueue _presentQueue;
        VkQueue _graphicsQueue;
        
        VkSurfaceKHR _surface;
        VkSurfaceCapabilitiesKHR _surfaceCapabilities;
        VkSurfaceFormatKHR _surfaceFormat;
        
        VkExtent2D _swapChainSize;
        VkSwapchainKHR _swapChain;
        
        std::vector<VkImage> _rawImages;
        std::vector<VkImageView> _colorBuffers;
        
        std::vector<VkFramebuffer> _frameBuffers;
        
        VkRenderPass _renderPass;
        VkPipeline _pipeline;
        VkPipelineLayout _pipelineLayout;
        
        VkDebugUtilsMessengerEXT _debugUtilsCallback;
		VkDebugReportCallbackEXT _debugReportCallback;
        
        VkCommandPool _commandPool;
        std::vector<VkCommandBuffer> _commandBuffers;
        
        std::vector<VkSemaphore> _renderFinishedSemaphore;
        std::vector<VkSemaphore> _imageAvailableSemaphore;
        std::vector<VkFence> _fences;
        
		std::vector<VulkanMesh> _vulkanMeshes;
        
        unsigned int _currentFrame;
        
        VulkanContext();
    };
    
    bool setupDebugCallback(VulkanContext & context);
    bool areValidationLayersAvailable(const std::vector<const char*> & validationLayers);
    bool loadVulkanLibrary();
    bool loadVulkanFunctions();
    bool createInstanceAndLoadExtensions(const AppInformation & appInfo, VulkanContext & context);
    bool createVulkanSurface(SDL_Window * window, VulkanContext & context);
    bool enumeratePhysicalDevices(AppInformation & appInfo, VulkanContext & context);
    bool choosePhysicalDevice(AppInformation &appInfo, VulkanContext & vulkanContext);
    bool lookupDeviceExtensions(AppInformation &appInfo);
    bool createDevice(AppInformation & appInfo, VulkanContext & context);
    bool createQueue(AppInformation & appInfo, VulkanContext & context);
    bool createSwapChain(AppInformation & appInfo, VulkanContext & context);
    bool createColorBuffers(VulkanContext & context);
    bool createRenderPass(VulkanContext & vulkanContext);
    bool createFrameBuffers(VulkanContext & vulkanContext);
    std::vector<VkShaderModule> createShaderModules(AppInformation & appInfo, VulkanContext & context);
    bool createFixedState(AppInformation & appInfo, VulkanContext & context);
    bool createGraphicsPipeline(AppInformation & appInfo, VulkanContext & context, const std::vector<VkShaderModule> & shaderModules);
    bool createCommandPool(AppInformation & appInfo, VulkanContext & context);
    bool createCommandBuffers(AppInformation & appInfo, VulkanContext & context);
    std::vector<VkFence> createFences(VulkanContext & context);
    std::vector<VkSemaphore> createSemaphores(VulkanContext & context);
    bool createSemaphores(AppInformation & appInfo, VulkanContext & context);
    int findMemoryType(VulkanContext & context, uint32_t typeFilter, VkMemoryPropertyFlags properties);
    bool createBuffer(VulkanContext & context, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, BufferDescriptor & bufDesc);
    bool createVertexOrIndexBuffer(VulkanContext & context, const void * srcData, VkDeviceSize bufferSize, BufferDescriptor & result, BufferType type);
    bool createIndexBuffer(AppInformation & appInfo, VulkanContext & context, unsigned int bufferIndex);
    bool createVertexBuffer(AppInformation & appInfo, VulkanContext & context, unsigned int bufferIndex);
    bool handleVulkanSetup(AppInformation & appInfo, VulkanContext & context);
}

#endif
