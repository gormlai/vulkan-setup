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

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/rotate_vector.hpp>

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

    
    enum class ShaderType
    {
        Vertex,
        Fragment
    };

	enum class BufferType
	{
		Index,
		Vertex,
        Uniform,
	};
    
    struct UniformBufferObject
    {
        glm::mat4 _model;
        glm::mat4 _view;
        glm::mat4 _projection;
        glm::vec4 _lights[16];
        int _numLights;
    };
    
    struct Shader
    {
        std::string _filename;
        std::vector<char> _byteCode;
        ShaderType _type;
        
    public:
        Shader(const std::string & filename, ShaderType type);
    };


    struct VkGraphicsPipelineCreateInfoDescriptor 
    {
        VkGraphicsPipelineCreateInfoDescriptor(
            VkGraphicsPipelineCreateInfo& createInfo,
            std::vector<VkPipelineShaderStageCreateInfo>& pipelineShaderStage,
            VkPipelineVertexInputStateCreateInfo& vertexInputInfo,
            VkPipelineInputAssemblyStateCreateInfo& inputAssemblyInfo,
            VkViewport& viewport,
            VkRect2D& scissor,
            VkPipelineViewportStateCreateInfo& viewportStateCreateInfo,
            VkPipelineRasterizationStateCreateInfo& rasterizerCreateInfo,
            VkPipelineMultisampleStateCreateInfo& multisamplingCreateInfo,
            VkPipelineDepthStencilStateCreateInfo& depthStencilCreateInfo,
            VkPipelineColorBlendAttachmentState& colorBlendAttachmentCreateInfo,
            VkPipelineColorBlendStateCreateInfo& colorBlendingCreateInfo,
            VkPipelineLayoutCreateInfo& pipelineLayoutCreateInfo,
            VkPipelineDynamicStateCreateInfo& dynamicStateCreateInfo,
            std::vector<VkVertexInputBindingDescription>& vertexInputBindingDescriptions,
            std::vector<VkVertexInputAttributeDescription>& vertexInputAttributeDescriptions)
            :  _createInfo(createInfo)
            , _pipelineShaderStage(pipelineShaderStage)
            , _vertexInputInfo(vertexInputInfo)
            , _inputAssemblyInfo(inputAssemblyInfo)
            , _viewport(viewport)
            , _scissor(scissor)
            , _viewportStateCreateInfo(viewportStateCreateInfo)
            , _rasterizerCreateInfo(rasterizerCreateInfo)
            , _multisamplingCreateInfo(multisamplingCreateInfo)
            , _depthStencilCreateInfo(depthStencilCreateInfo)
            , _colorBlendAttachmentCreateInfo(colorBlendAttachmentCreateInfo)
            , _colorBlendingCreateInfo(colorBlendingCreateInfo)
            , _pipelineLayoutCreateInfo(pipelineLayoutCreateInfo)
            , _dynamicStateCreateInfo(dynamicStateCreateInfo)
            , _vertexInputBindingDescriptions(vertexInputBindingDescriptions)
            , _vertexInputAttributeDescriptions(vertexInputAttributeDescriptions)
        {}

        VkGraphicsPipelineCreateInfo& _createInfo;
           
        // helper objects
        std::vector<VkPipelineShaderStageCreateInfo> & _pipelineShaderStage;
        VkPipelineVertexInputStateCreateInfo & _vertexInputInfo;
        VkPipelineInputAssemblyStateCreateInfo & _inputAssemblyInfo;

        VkViewport & _viewport;
        VkRect2D & _scissor;
        VkPipelineViewportStateCreateInfo & _viewportStateCreateInfo;

        VkPipelineRasterizationStateCreateInfo & _rasterizerCreateInfo;
        VkPipelineMultisampleStateCreateInfo & _multisamplingCreateInfo;
        VkPipelineDepthStencilStateCreateInfo & _depthStencilCreateInfo;

        VkPipelineColorBlendAttachmentState & _colorBlendAttachmentCreateInfo;
        VkPipelineColorBlendStateCreateInfo & _colorBlendingCreateInfo;
        VkPipelineLayoutCreateInfo & _pipelineLayoutCreateInfo;
        VkPipelineDynamicStateCreateInfo & _dynamicStateCreateInfo;

        // setting up vertex arrays
        std::vector<VkVertexInputBindingDescription> & _vertexInputBindingDescriptions;
        std::vector<VkVertexInputAttributeDescription> & _vertexInputAttributeDescriptions;

    };
        
    struct AppDescriptor
    {
        std::string _appName;
        uint32_t _requiredVulkanVersion;
        SDL_Window * _window;
        std::vector<VkPhysicalDevice> _physicalDevices;
        unsigned int _chosenPhysicalDevice;
        std::vector<VkExtensionProperties> _deviceExtensions;
        std::vector<VkSurfaceFormatKHR> _surfaceFormats;
        
        std::vector<Shader> _shaders;

        unsigned int _numGraphicsPipelines = 0;
        std::function<void (VkGraphicsPipelineCreateInfoDescriptor & desc, unsigned int index)> _graphicsPipelineCreationCallback = [](VkGraphicsPipelineCreateInfoDescriptor&, unsigned int) { return; };

		std::function<glm::vec4 (void)> _backgroundClearColor = []() { return glm::vec4{ 0,0,0,1 }; };

		std::function <VkVertexInputBindingDescription()> _getBindingDescription = []() { return VkVertexInputBindingDescription(); };
		std::function < std::array<VkVertexInputAttributeDescription, 2>()> _getAttributes = []() {return std::array<VkVertexInputAttributeDescription, 2>(); };
        
        typedef std::function <bool(float, float)> UpdateFunction; // returns true, if the code should continue. Return false, to request termination
        UpdateFunction _updateFunction = [](float, float) { return true; };

		typedef std::function<void(glm::vec3 &, glm::vec3 &, glm::vec3 &)> CameraUpdateFunction;
		CameraUpdateFunction _cameraUpdateFunction = [](glm::vec3 & pos, glm::vec3 & lookat, glm::vec3 & up) { pos = glm::vec3{ 0,0,0}; lookat = glm::vec3{ 0,0,-1 }; up = glm::vec3{ 0,1,0 }; };
        
        typedef std::function<glm::mat4 (const void*, float, float)> UpdateModelMatrix;
        UpdateModelMatrix _updateModelMatrix = [](const void*, float, float) { return glm::mat4(); };
        
        AppDescriptor();
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
        std::vector<BufferDescriptor> _uniformBuffers;
        VkDescriptorPool _descriptorPool;
        std::vector<VkDescriptorSet> _descriptorSets;

		unsigned int _numIndices;
        UniformBufferObject _transformation;
        void * _userData;
        
		VulkanMesh()
            :_numIndices(0)
            ,_userData(nullptr)
		{
		}
	};
    
	struct VulkanCamera
	{
		VulkanCamera() { }
		VulkanCamera(const glm::vec3 & pos, const glm::vec3 & lookat, const glm::vec3 & up)
			:_position(pos)
			, _lookat(lookat)
			, _up(up) {}

		glm::vec3 _position;
		glm::vec3 _lookat;
		glm::vec3 _up;
	};

    struct Context
    {
        VkInstance _instance;
        VkPhysicalDevice _physicalDevice;
        VkDevice _device;
        VkPhysicalDeviceProperties _deviceProperties;
        VkPhysicalDeviceFeatures _physicalDeviceFeatures;
        
        VkQueue _presentQueue;
        VkQueue _graphicsQueue;
        
        VkSurfaceKHR _surface;
        VkSurfaceCapabilitiesKHR _surfaceCapabilities;
        VkSurfaceFormatKHR _surfaceFormat;
        
        VkExtent2D _swapChainSize;
        VkSwapchainKHR _swapChain;
        
		// for colour buffer
        std::vector<VkImage> _rawImages;
        std::vector<VkImageView> _colorBuffers;

		// for depth buffer
		std::vector<VkImage> _depthImages;
		std::vector<VkImageView> _depthImageViews;
		std::vector<VkDeviceMemory> _depthMemory;

        std::vector<VkFramebuffer> _frameBuffers;
		std::vector<VkShaderModule> _shaderModules;
        
        VkRenderPass _renderPass;
		VkRenderPass _adhocRenderPass;
		VkPipeline _pipeline;
        VkPipelineLayout _pipelineLayout;
		VkPipelineCache _pipelineCache;
        
        VkDescriptorSetLayout _descriptorSetLayout;
        
        VkDebugUtilsMessengerEXT _debugUtilsCallback;
		VkDebugReportCallbackEXT _debugReportCallback;
		VkAllocationCallbacks * _allocator;
        
        VkCommandPool _commandPool;
        std::vector<VkCommandBuffer> _commandBuffers;

		VkCommandPool _adhocCommandPool;
		std::vector<VkCommandBuffer> _adhocCommandBuffers;

        std::vector<VkSemaphore> _renderFinishedSemaphores;
        std::vector<VkSemaphore> _imageAvailableSemaphores;
        std::vector<VkFence> _fences;
        
		std::vector<VulkanMesh> _vulkanMeshes;
        
        unsigned int _currentFrame;
        
        Context();

		const VulkanCamera & getCamera() { return _camera;  }

	private:
		friend bool update(AppDescriptor& appDesc, Context & context, uint32_t currentImage);
		VulkanCamera _camera;

    };

	bool resetCommandBuffers(Context& context, std::vector<VkCommandBuffer>& commandBuffers);
	void clearMeshes(AppDescriptor& appDesc, Context & context);
	bool addMesh(AppDescriptor& appDesc, Context & context, std::vector<unsigned char> & vertexData, std::vector<unsigned char> & indexData, void * userData);
	void destroyMesh(Context & context, VulkanMesh & mesh);
	void destroyBufferDescriptor(Context & context, BufferDescriptor & descriptor);

    bool setupDebugCallback(Context & context);
    bool areValidationLayersAvailable(const std::vector<const char*> & validationLayers);
    bool loadVulkanLibrary();
    bool loadVulkanFunctions();
    bool createInstanceAndLoadExtensions(const AppDescriptor& appDesc, Context & context);
    bool createVulkanSurface(SDL_Window * window, Context & context);
    bool enumeratePhysicalDevices(AppDescriptor& appDesc, Context & context);
    bool choosePhysicalDevice(AppDescriptor&appDesc, Context & vulkanContext);
    bool lookupDeviceExtensions(AppDescriptor&appDesc);
    bool createDevice(AppDescriptor& appDesc, Context & context);
    bool createQueue(AppDescriptor& appDesc, Context & context);
    bool createSwapChain(AppDescriptor& appDesc, Context & context);
    bool createColorBuffers(Context & context);
	bool createDepthBuffers(AppDescriptor& appDesc, Context & context);
    bool createRenderPass(Context & vulkanContext, VkRenderPass * result, bool clearColorBuffer);
    bool createDescriptorSetLayout(Context & vulkanContext);
    bool createFrameBuffers(Context & vulkanContext);
    std::vector<VkShaderModule> createShaderModules(AppDescriptor & appDesc, Context & context);
	bool createPipelineCache(AppDescriptor & appDesc, Context & context);
	bool createGraphicsPipeline(AppDescriptor & appDesc, Context & context);
    bool createCommandPool(AppDescriptor & appDesc, Context & context, VkCommandPool * result);
	bool recordStandardCommandBuffers(AppDescriptor & appDesc, Context & context);
	std::vector<VkFence> createFences(Context & context);
    std::vector<VkSemaphore> createSemaphores(Context & context);
    bool createSemaphores(AppDescriptor & appDesc, Context & context);
    bool createBuffer(Context & context, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, BufferDescriptor & bufDesc);
    bool createBuffer(Context & context, const void * srcData, VkDeviceSize bufferSize, BufferDescriptor & result, BufferType type);
    bool createIndexAndVertexBuffer(AppDescriptor & appDesc, Context & context, std::vector<unsigned char> & vertexData, std::vector<unsigned char> & indexData, void *userData, unsigned int meshIndex);
    bool createUniformBuffer(AppDescriptor & appDesc, Context & context, unsigned int meshIndex);
    bool createDescriptorPool(Context & context, unsigned int bufferIndex);
    bool createDescriptorSet(AppDescriptor & appDesc, Context & context, unsigned int bufferIndex);
    bool handleVulkanSetup(AppDescriptor & appDesc, Context & context);
    
    bool update(AppDescriptor & appDesc, Context & context, uint32_t currentImage);
    void updateUniforms(AppDescriptor & appDesc, Context & context, unsigned int bufferIndex, uint32_t meshIndex);
	bool createSwapChainDependents(AppDescriptor & appDesc, Context & context);
	bool recreateSwapChain(AppDescriptor & appDesc, Context & context);
	bool cleanupSwapChain(AppDescriptor & appDesc, Context & context);


}

#endif
