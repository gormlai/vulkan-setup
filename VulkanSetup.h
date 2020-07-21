#ifndef _VULKAN_SETUP_H_
#define _VULKAN_SETUP_H_

#include <volk/volk.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <string>
#include <vector>
#include <algorithm>
#include <array>
#include <functional>
#include <memory>
#include <string.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include "vk_mem_alloc.h"

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

    class Logger
    {
    public:
        enum class Level
        {
            Verbose,
            Info,
            Warn,
            Debug,
            Error,
        };

        virtual void log(Vulkan::Logger::Level logLevel, const std::string entry) {}
        virtual ~Logger() {}
    };


	enum class BufferType
	{
		Index = 0,
		Vertex,
        Uniform,
	};

    enum class ShaderStage
    {
        Vertex = 0,
        Fragment,
        Compute,
        ShaderStageCount,
    };
    VkShaderStageFlagBits mapFromShaderStage(Vulkan::ShaderStage stage);

    struct Shader
    {
        std::string _filename;
        std::vector<char> _byteCode;
        VkShaderStageFlagBits  _type;
        VkShaderModule _shaderModule;
        
    public:
        Shader(const std::string & filename, VkShaderStageFlagBits type);
        Shader() {}
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

    struct VkComputePipelineCreateInfoDescriptor
    {
        VkComputePipelineCreateInfo _createInfo;
    };

    struct VkRenderPassCreateInfoDescriptor
    {
        VkAttachmentReference _colorAttachmentReference;
        VkAttachmentReference _depthAttachmentReference;
        VkAttachmentReference _colorAttachmentReferenceResolve;
        VkSubpassDescription _subpassDescription;
        VkAttachmentDescription _colorAttachment;
        VkAttachmentDescription _depthAttachment;
        VkAttachmentDescription _colorAttachmentResolve;
        std::array<VkSubpassDependency,10> _dependency;
        VkRenderPassCreateInfo _createInfo;
        std::array<VkAttachmentDescription,10> _attachmentDescriptions;

        VkRenderPassCreateInfoDescriptor()
        {
            memset(this, 0, sizeof(*this));
        }
    };

        
    typedef std::function<void(VkGraphicsPipelineCreateInfoDescriptor &)> GraphicsPipelineCustomizationCallback;
    typedef std::function<void(VkComputePipelineCreateInfoDescriptor&)> ComputePipelineCustomizationCallback;
    typedef std::function<void(VkRenderPassCreateInfoDescriptor&)> RenderPassCustomizationCallback;


    struct AppDescriptor
    {
        std::string _appName;
        uint32_t _requiredVulkanVersion;
        bool _enableVSync;
        uint32_t _requestedNumSamples;
        uint32_t _actualNumSamples;
        SDL_Window * _window;
        std::vector<VkPhysicalDevice> _physicalDevices;
        unsigned int _chosenPhysicalDevice;
        std::vector<VkExtensionProperties> _deviceExtensions;
        std::vector<VkSurfaceFormatKHR> _surfaceFormats;
        int _drawableSurfaceWidth;
        int _drawableSurfaceHeight;

        void addRequiredInstanceExtensions(std::vector<std::string>& extensions);
        void addRequiredDeviceExtensions(std::vector<std::string>& extensions);
        void addRequiredInstanceExtension(const std::string& extension);
        void addRequiredDeviceExtension(const std::string& extension);

        inline void setPreferredSurfaceFormat(VkFormat format) { _preferredSurfaceFormat = format;}
        std::vector<std::string> getRequiredDeviceExtensions() const { return _requiredDeviceExtensions; }
        std::vector<std::string> getRequiredInstanceExtensions() const { return _requiredInstanceExtensions; }
         
        bool hasExtension(const std::string& name)
        {
            for (auto ext : _deviceExtensions)
            {
                if (strcmp(ext.extensionName, name.c_str()) == 0)
                    return true;
            }
            return false;
        }
        
        AppDescriptor();

    private:
        std::vector<std::string> _requiredInstanceExtensions;
        std::vector<std::string> _requiredDeviceExtensions;
        VkFormat _preferredSurfaceFormat;
    };

    struct Buffer
    {
    public:
        virtual void destroy() {}
        virtual ~Buffer() {}

    protected:
    };
    typedef std::shared_ptr<Buffer> BufferPtr;

    struct Context;
    struct BufferDescriptor : public Buffer
    {
        VkBuffer _buffer;
        VmaAllocation _memory;
        bool _mappable;
        unsigned int _size;
  
        BufferDescriptor()
            :_buffer(VK_NULL_HANDLE)
            , _memory(VK_NULL_HANDLE)
            , _size(0)
            , _mappable(false)
        {
        }

        virtual ~BufferDescriptor() {
            destroy();
        }

        void destroy() override;

        virtual bool copyFrom(Vulkan::Context & context, VkCommandPool commandPool, VkQueue queue, const void * srcData, VkDeviceSize amount, VkDeviceSize dstOffset);

        bool copyFrom(Vulkan::Context& context,
            VkCommandPool commandPool,
            VkQueue queue,
            BufferDescriptor& src,
            VkDeviceSize amount,
            VkDeviceSize srcOffset,
            VkDeviceSize dstOffset);


        bool copyTo(VkDevice device,
                    VkCommandPool commandPool,
                    VkQueue queue,
                    VkImage image,
                    unsigned int width,
                    unsigned int height,
                    unsigned int depth);


    private:

    };
    typedef std::shared_ptr<BufferDescriptor> BufferDescriptorPtr;

    struct Context;
    struct PersistentBuffer : public Buffer
    {
        std::vector<unsigned int> _offsets;
        std::vector<VmaAllocationInfo> _allocInfos;
        std::vector<Vulkan::BufferDescriptor> _buffers;
        unsigned int _registeredSize;

        PersistentBuffer(unsigned int numBuffers)
            :_offsets(numBuffers)
            ,_allocInfos(numBuffers)
            ,_buffers(numBuffers)
            ,_registeredSize(0)
        {
            memset(&_offsets[0], 0, sizeof(unsigned int) * numBuffers);
        }

        virtual ~PersistentBuffer() {
            destroy();
        }


        static bool startFrame(unsigned int frameIndex);
        static bool submitFrame(Vulkan::Context& context, unsigned int frameIndex);
        bool flushData(Vulkan::Context & context, unsigned int frameIndex);

        inline Vulkan::BufferDescriptor & getBuffer(const unsigned int index)  { return _buffers[index % (unsigned int)_buffers.size()]; }
        bool copyFrom(unsigned int frameIndex, const void* srcData, VkDeviceSize amount, VkDeviceSize dstOffset = UINT64_MAX);
        bool copyFromAndFlush(Vulkan::Context& context, unsigned int frameIndex, const void* srcData, VkDeviceSize amount, VkDeviceSize dstOffset = UINT64_MAX);
        void destroy() override;

    };
    typedef std::shared_ptr<PersistentBuffer> PersistentBufferPtr;

    struct ImageDescriptor
    {
        VkImage _image;
        VmaAllocation _memory;
        unsigned int _size;
        void* _mappedData;

        ImageDescriptor()
            :_image(VK_NULL_HANDLE)
            , _size(0)
            , _mappedData(nullptr) {}

        void destroy();

        void* map();
        void unmap();
    };
    
    struct Mesh;
    struct Uniform;
    typedef std::function<unsigned int (const Vulkan::Uniform & uniform, std::vector<unsigned char> &)> UpdateUniformFunction;

	struct Mesh
	{
		unsigned int _numIndices;
        void * _userData;
        
        BufferPtr getVertexBuffer() {
            return _buffers[0];
        }

        BufferPtr getIndexBuffer() {
            return _buffers[1];
        }

        BufferPtr getInstanceBuffer() {
            return _instanceBuffer;
        }

        void setVertexBuffer(BufferPtr vertexBuffer) {
            _buffers[0] = vertexBuffer;
        }

        void setIndexBuffer(BufferPtr indexBuffer) {
            _buffers[1] = indexBuffer;
        }

        void setInstanceBuffer(BufferPtr instanceBuffer) {
            _instanceBuffer = instanceBuffer;
        }

        Mesh()
            :_numIndices(0)
            ,_userData(nullptr)
            ,_instanceBuffer(nullptr)
		{
		}

    private:
        BufferPtr _buffers[2];
        BufferPtr _instanceBuffer;

	};
    typedef std::shared_ptr<Mesh> MeshPtr;

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

    struct EffectDescriptor;
    struct Context;
    typedef std::function<bool (AppDescriptor &, Context &, EffectDescriptor &)> RecordCommandBuffersFunction;

    struct UniformAggregate
    {
        VkBufferView _bufferView;
        PersistentBufferPtr _buffer;
        VkSampler _sampler;
        VkImageView _imageView;

        UniformAggregate()
            :_bufferView(VK_NULL_HANDLE)
            , _sampler(VK_NULL_HANDLE)
            , _imageView(VK_NULL_HANDLE) {}
    };

    struct Uniform
    {
        VkDescriptorType _type;
        uint32_t _binding;
        uint32_t _set;
        uint32_t _size;
        uint32_t _offset;
        std::string _name;
        std::vector<UniformAggregate> _frames;
        std::vector<ShaderStage> _stages;

        Uniform()
            :_binding(UINT32_MAX)
            ,_size(0)
            , _set(UINT32_MAX) {}
    };

    struct EffectDescriptor
    {
        GraphicsPipelineCustomizationCallback _graphicsPipelineCreationCallback;
        ComputePipelineCustomizationCallback _computePipelineCreationCallback;
        RenderPassCustomizationCallback _renderPassCreationCallback;
        bool _createPipeline;
        std::vector<bool> _recordCommandsNeeded;

        VkPipeline _pipeline;
        VkPipelineLayout _pipelineLayout;
        std::vector<Shader> _shaderModules;
        std::vector<VkCommandBuffer> _commandBuffers;
        std::vector<VkDescriptorSet> _descriptorSets;
        VkDescriptorSetLayout _descriptorSetLayout;

        VkDescriptorPool _descriptorPool; // for image samplers

        VkRenderPass _renderPass; 

        UpdateUniformFunction _updateUniform = [](const Vulkan::Uniform& uniform, std::vector<unsigned char>&) { return 0; };
        std::vector<Uniform> _uniforms;

        RecordCommandBuffersFunction _recordCommandBuffers = [](AppDescriptor& appDesc, Context& context, EffectDescriptor& effectDescriptor) { return true; };
        std::string _name;

        EffectDescriptor()
            :_descriptorPool(VK_NULL_HANDLE)
            , _descriptorSetLayout(VK_NULL_HANDLE)
            , _pipeline(VK_NULL_HANDLE)
            , _pipelineLayout(VK_NULL_HANDLE)
            , _renderPass(VK_NULL_HANDLE)
            , _graphicsPipelineCreationCallback(nullptr)
            , _computePipelineCreationCallback(nullptr)
            , _renderPassCreationCallback(nullptr)
            , _createPipeline(true)
        {
        }

      inline void setRerecordNeeded()  { for (size_t i=0 ; i < _recordCommandsNeeded.size() ; i++) _recordCommandsNeeded[i] = true; }
        inline bool getRerecordNeeded(const unsigned int frame) { return _recordCommandsNeeded[frame]; }
        uint32_t totalSamplerCount() const;
        uint32_t totalTexelBufferCount() const;
        uint32_t totalImagesCount() const;
        uint32_t totalNumUniformBuffers() const;
        uint32_t totalNumUniforms() const;
        uint32_t totalTypeCount(VkDescriptorType type) const;
        uint32_t totalTypeCount(Vulkan::ShaderStage stage, VkDescriptorType type) const;
        uint32_t addUniformSamplerOrImage(Vulkan::Context& context, Vulkan::ShaderStage stage, const std::string& name, VkDescriptorType type, int binding = -1);
        uint32_t addUniformSampler(Vulkan::Context& context, Vulkan::ShaderStage stage, const std::string & name, int binding= -1 );
        uint32_t addUniformImage(Vulkan::Context& context, Vulkan::ShaderStage stage, const std::string& name, int binding = -1);
        uint32_t addUniformBuffer(Vulkan::Context& context, Vulkan::ShaderStage stage, const std::string& name, uint32_t size, int binding = -1);
        void collectDescriptorSetLayouts(std::vector<VkDescriptorSetLayout> & layouts);
        uint32_t collectUniformsOfType(VkDescriptorType type, Uniform** result);
        uint32_t collectUniformsOfType(VkDescriptorType type, Vulkan::ShaderStage stage, Uniform** result);
        Uniform* getUniformWithBinding(int binding);

        bool bindTexelBuffer(Vulkan::Context& context, Vulkan::ShaderStage shaderStage, uint32_t binding, VkBufferView bufferView, VkBuffer buffer, unsigned int offset, unsigned int range);
        bool bindSampler(Vulkan::Context& context, Vulkan::ShaderStage shaderStage, uint32_t binding, VkImageView imageView, VkImageLayout layout, VkSampler sampler);
        bool bindImage(Vulkan::Context& context, Vulkan::ShaderStage shaderStage, uint32_t binding, VkImageView imageView, VkImageLayout layout);

    };
    typedef std::shared_ptr<EffectDescriptor> EffectDescriptorPtr;

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
        std::vector<VkImage> _swapChainImages;
        std::vector<VkImageView> _swapChainImageViews;

        // for the msaa colour buffer
        std::vector<ImageDescriptor> _msaaColourImages;
        std::vector<VkImageView> _msaaColourImageViews;

		// for depth buffer
		std::vector<ImageDescriptor> _depthImages;
		std::vector<VkImageView> _depthImageViews;

        std::vector<VkFramebuffer> _frameBuffers;
        
        
        VkDebugUtilsMessengerEXT _debugUtilsCallback;
		VkDebugReportCallbackEXT _debugReportCallback;
		VkAllocationCallbacks * _allocator;
        
        VkCommandPool _commandPool;

        std::vector<VkSemaphore> _renderFinishedSemaphores;
        std::vector<VkSemaphore> _imageAvailableSemaphores;
        std::vector<VkFence> _fences;
        
        VkPipelineCache _pipelineCache;
        VkRenderPass _renderPass;

        unsigned int _numInflightFrames;
        unsigned int _currentFrame;
        std::vector<EffectDescriptorPtr> _potentialEffects;
        std::vector<EffectDescriptorPtr> _frameReadyEffects;

        Context();

		const VulkanCamera & getCamera() { return _camera;  }

	private:
		VulkanCamera _camera;

    };

    
    bool initializeIndexAndVertexBuffers(AppDescriptor& appDesc, 
        Context& context,
        std::vector<unsigned char>& vertexData, 
        std::vector<unsigned char>& indexData, 
        void* userData, 
        Vulkan::Mesh& result);
    BufferDescriptorPtr createIndexOrVertexBuffer(Context& context, const void* srcData, VkDeviceSize bufferSize, BufferType type);


    bool handleVulkanSetup(AppDescriptor& appDesc, Context& context);
    bool recreateSwapChain(AppDescriptor& appDesc, Context& context);
    void updateUniforms(AppDescriptor& appDesc, Context& context, uint32_t currentImage);

    bool resetCommandBuffer(Context& context, VkCommandBuffer & commandBuffers, unsigned int index);
    bool resetCommandBuffers(Context& context, std::vector<VkCommandBuffer>& commandBuffers);
    bool createShaderModules(AppDescriptor& appDesc, Context& context, std::vector<Shader>& shaders);
    bool initEffectDescriptor(AppDescriptor& appDesc, 
        Context& context, 
        const bool createGraphicsPipeline, 
        GraphicsPipelineCustomizationCallback graphicsPipelineCreationCallback, 
        RenderPassCustomizationCallback renderPassCreationCallback,
        Vulkan::EffectDescriptor& effect);
    bool initEffectDescriptor(AppDescriptor& appDesc, Context& context, ComputePipelineCustomizationCallback computePipelineCreationCallback, Vulkan::EffectDescriptor& effect);
    bool recreateEffectDescriptor(AppDescriptor& appDesc, Context& context, EffectDescriptorPtr effect);

    BufferDescriptorPtr createBuffer(Context& context, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);
    PersistentBufferPtr lookupPersistentBuffer(Context& context, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, const std::string tag, int numBuffers = -1);
    PersistentBufferPtr createPersistentBuffer(Context& context, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, const std::string tag, int numBuffers = -1);

    bool createBufferView(Context& context, VkBuffer buffer, VkFormat requiredFormat, VkDeviceSize size, VkDeviceSize offset, VkBufferView& result);

    bool allocateAndBindImageMemory(Vulkan::Context& context, VkImage& image, VkDeviceMemory& memory, VkMemoryPropertyFlags memoryProperties);

    inline unsigned int maxMipMapLevels(unsigned int width) { return static_cast<unsigned int>(floor(log2(width))) + 1; }
    inline unsigned int maxMipMapLevels(unsigned int width, unsigned int height) { return static_cast<unsigned int>(floor(log2(std::max(width, height)))) + 1; }
    inline unsigned int maxMipMapLevels(unsigned int width, unsigned int height, unsigned int depth) { return static_cast<unsigned int>(floor(log2(std::max(width, std::max(height, depth))))) + 1; }

    bool createImage(Vulkan::Context& context,
        unsigned int width,
        unsigned int height,
        unsigned int depth,
        unsigned int mipMapLevels,
        unsigned int samplesPrPixels,
        VkFormat requiredFormat,
        VkImageTiling requiredTiling,
        VkImageUsageFlags requiredUsage,
        VkMemoryPropertyFlags memoryProperties,
        Vulkan::ImageDescriptor& resultImage);


    bool createImage(Vulkan::Context& context, 
        const void* pixels, 
        const unsigned int pixelSize, 
        const unsigned int width, 
        const unsigned int height, 
        const unsigned int depth,
        const unsigned int samplesPrPixels,
        VkFormat format, 
        ImageDescriptor & result, 
        unsigned int mipLevels,
        VkImageLayout finalLayout);

    bool createImageView(Vulkan::Context& context,
        VkImage image,
        VkFormat requiredFormat,
        VkImageAspectFlags requiredAspectFlags,
        VkImageViewType imageViewType,
        VkImageView& result);

    bool transitionImageLayout(Vulkan::Context& context,
        VkImage image,
        VkImageLayout oldLayout,
        VkImageLayout newLayout,
        VkAccessFlags srcAccessMask,
        VkAccessFlags dstAccessMask,
        VkPipelineStageFlags srcStageFlags,
        VkPipelineStageFlags dstStageFlags,
        VkCommandBuffer commandBuffer);

    bool transitionImageLayout(Vulkan::Context& context,
        VkImage image,
        VkImageLayout oldLayout,
        VkImageLayout newLayout,
        VkCommandBuffer commandBuffer);

    bool transitionImageLayoutAndSubmit(Vulkan::Context & context,
                               VkImage image,
                               VkImageLayout oldLayout,
                               VkImageLayout newLayout);

    bool createUniformBuffer(AppDescriptor& appDesc, Context& context, VkDeviceSize bufferSize, BufferDescriptor& result);

    bool createSampler(Vulkan::Context& context, VkSampler& sampler, VkSamplerCreateInfo & samplerCreateInfo);
    bool createSampler(Vulkan::Context& context, VkSampler& sampler);

    std::vector<VkFence> createFences(Context& context, unsigned int count, VkFenceCreateFlags flags);
    bool createFence(Context& context, VkFenceCreateFlags flags, VkFence& result);

    inline unsigned int getNumInflightFrames(Context& context) {
        return context._numInflightFrames == 0 ? (unsigned int)context._swapChainImages.size() : context._numInflightFrames;
    }

    VkCommandBuffer createCommandBuffer(Vulkan::Context& context, VkCommandPool commandPool, bool beginCommandBuffer);
    bool createFrameBuffers(Context& Context, VkExtent2D frameBufferSize, VkRenderPass& renderPass, std::vector<VkImageView>& colorViews, std::vector<VkImageView>& msaaViews, std::vector<VkImageView>& depthsViews, std::vector<VkFramebuffer>& result);
    bool createDepthBuffer(AppDescriptor& appDesc, Context& context, ImageDescriptor & image, VkImageView& imageView);
    bool createDepthBuffers(AppDescriptor& appDesc, Context& context, std::vector<ImageDescriptor>& images, std::vector<VkImageView>& imageViews);

    void setLogger(Vulkan::Logger * logger);
    
    uint32_t maxAASamples(Context& context);
    uint32_t requestNumAASamples(Context& context, uint32_t count);

}

#endif
