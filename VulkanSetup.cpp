
#include "VulkanSetup.h"

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

////////////////////////////////////// Vulkan method declarations ///////////////////////////////////////////////////////

namespace Vulkan
{
    void clearMeshes(Context& context, EffectDescriptor& effect);
    void destroyMesh(Context& context, Mesh& mesh);
    void destroyBufferDescriptor(Context& context, BufferDescriptor& descriptor);

    bool setupDebugCallback(Context& context);
    bool areValidationLayersAvailable(const std::vector<const char*>& validationLayers);
    bool loadVulkanLibrary();
    bool loadVulkanFunctions();
    bool createInstanceAndLoadExtensions(const AppDescriptor& appDesc, Context& context);
    bool createVulkanSurface(SDL_Window* window, Context& context);
    bool enumeratePhysicalDevices(AppDescriptor& appDesc, Context& context);
    bool choosePhysicalDevice(AppDescriptor& appDesc, Context& Context);
    bool lookupDeviceExtensions(AppDescriptor& appDesc);
    bool createDevice(AppDescriptor& appDesc, Context& context);
    bool createQueue(AppDescriptor& appDesc, Context& context);
    bool createSwapChain(AppDescriptor& appDesc, Context& context);
    bool createColorBuffers(Context& context);
    bool createDepthBuffers(AppDescriptor& appDesc, Context& context);
    bool createRenderPass(Context& Context, VkRenderPass* result, bool clearColorBuffer);
    bool createDescriptorSetLayout(Context& Context, EffectDescriptor& effect);
    bool createFrameBuffers(Context& Context);
    bool createPipelineCache(AppDescriptor& appDesc, Context& context);
    bool createCommandPool(AppDescriptor& appDesc, Context& context, VkCommandPool* result);
    bool recordStandardCommandBuffers(AppDescriptor& appDesc, Context& context);
    std::vector<VkFence> createFences(Context& context);
    std::vector<VkSemaphore> createSemaphores(Context& context);
    bool createSemaphores(AppDescriptor& appDesc, Context& context);
    bool createVertexOrIndexBuffer(Context& context, const void* srcData, VkDeviceSize bufferSize, BufferDescriptor& result, BufferType type);
    bool createIndexAndVertexBuffer(AppDescriptor& appDesc, Context& context, std::vector<unsigned char>& vertexData, std::vector<unsigned char>& indexData, void* userData, Vulkan::Mesh& result);
    bool createDescriptorPool(Context& context, EffectDescriptor& effect);
    bool createDescriptorSet(AppDescriptor& appDesc, Context& context, EffectDescriptor& effect);

    void updateUniforms(AppDescriptor& appDesc, Context& context, unsigned int bufferIndex, uint32_t meshIndex);
    bool createSwapChainDependents(AppDescriptor& appDesc, Context& context);
    bool cleanupSwapChain(AppDescriptor& appDesc, Context& context);
    bool initEffectDescriptor(AppDescriptor& appDesc, Context& context, Vulkan::EffectDescriptor& effect);

    bool createGraphicsPipeline(AppDescriptor& appDesc, Context& context, GraphicsPipelineCustomizationCallback graphicsPipelineCreationCallback, Vulkan::EffectDescriptor& effect);
    bool createComputePipeline(AppDescriptor& appDesc, Context& context, ComputePipelineCustomizationCallback computePipelineCreationCallback, Vulkan::EffectDescriptor& effect);

}

///////////////////////////////////// Vulkan Variable ///////////////////////////////////////////////////////////////////

bool Vulkan::validationLayersEnabled = true;


///////////////////////////////////// Vulkan Helper Function ////////////////////////////////////////////////////////////

namespace
{
    VkShaderStageFlagBits mapFromShaderStage(Vulkan::ShaderStage stage)
    {
        switch (stage)
        {
            case Vulkan::ShaderStage::Vertex:
                return VK_SHADER_STAGE_VERTEX_BIT;
            case Vulkan::ShaderStage::Fragment:
                return VK_SHADER_STAGE_FRAGMENT_BIT;
            case Vulkan::ShaderStage::Compute:
                return VK_SHADER_STAGE_COMPUTE_BIT;
            default:
                return VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
        }
    }

    bool createCommandBuffers(Vulkan::Context& context, VkCommandPool commandPool, unsigned int numBuffers, std::vector<VkCommandBuffer>* result)
    {
        std::vector<VkCommandBuffer> commandBuffers(numBuffers);

        VkCommandBufferAllocateInfo commandBufferAllocateInfo;
        memset(&commandBufferAllocateInfo, 0, sizeof(VkCommandBufferAllocateInfo));
        commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        commandBufferAllocateInfo.commandPool = commandPool;
        commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        commandBufferAllocateInfo.commandBufferCount = (unsigned int)commandBuffers.size();
        const VkResult allocateCommandBuffersResult = vkAllocateCommandBuffers(context._device, &commandBufferAllocateInfo, &commandBuffers[0]);
        assert(allocateCommandBuffersResult == VK_SUCCESS);
        if (allocateCommandBuffersResult != VK_SUCCESS)
        {
            SDL_LogError(0, "Failed to create VkCommandbufferAllocateInfo\n");
            return false;
        }

        *result = commandBuffers;

        return true;
    }


    int findMemoryType(Vulkan::Context& context, uint32_t typeFilter, VkMemoryPropertyFlags properties) {

        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(context._physicalDevice, &memProperties);

        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
        {
            if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
                return i;
        }

        return -1;
    }

    bool findMatchingFormat(Vulkan::Context& context, const std::vector<VkFormat>& requiredFormats, VkImageTiling requiredTiling, VkFormatFeatureFlags requiredFeatures, VkFormat& result)
    {
        std::vector<VkFormat> matches;
        matches.reserve(requiredFormats.size());
        for (VkFormat format : requiredFormats)
        {
            VkFormatProperties props;
            vkGetPhysicalDeviceFormatProperties(context._physicalDevice, format, &props);
            switch (requiredTiling)
            {
            case VK_IMAGE_TILING_LINEAR:
                if (props.linearTilingFeatures & requiredFeatures)
                    matches.push_back(format);
                break;
            case VK_IMAGE_TILING_OPTIMAL:
                if (props.optimalTilingFeatures & requiredFeatures)
                    matches.push_back(format);
                break;
            default:
                break; // do nothing
            }

        }

        if (matches.empty())
            return false;

        // for now just take the first match
        result = matches[0];
        return true;
    }

    VkFormat findDepthFormat(Vulkan::Context& context, VkImageTiling requiredTiling)
    {
        VkFormat requiredFormat;
        findMatchingFormat(
            context,
            { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
            requiredTiling,
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT, requiredFormat);

        return requiredFormat;
    };

    bool allocateImageMemory(Vulkan::Context& context,
        VkMemoryPropertyFlags requiredProperties,
        VkImage image,
        VkDeviceMemory& result)
    {
        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(context._device, image, &memRequirements);

        VkMemoryAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(context, memRequirements.memoryTypeBits, requiredProperties);

        const VkResult allocationMemoryResult = vkAllocateMemory(context._device, &allocInfo, VK_NULL_HANDLE, &result);
        if (allocationMemoryResult != VK_SUCCESS)
            return false;

        vkBindImageMemory(context._device, image, result, 0);
        return true;
    }


}


///////////////////////////////////// Vulkan Context ///////////////////////////////////////////////////////////////////

Vulkan::Context::Context()
:_currentFrame(0)
,_swapChain(nullptr)
,_pipelineCache(VK_NULL_HANDLE)
,_allocator(nullptr)
,_debugReportCallback(VK_NULL_HANDLE)
,_debugUtilsCallback(VK_NULL_HANDLE)
{

}

///////////////////////////////////// Vulkan Effect Descriptor ///////////////////////////////////////////////////////////////////

namespace
{
    uint32_t numUniforms(const Vulkan::EffectDescriptor & effect)
    {
        uint32_t count = 0;
        for (int i = 0; i < static_cast<int>(Vulkan::ShaderStage::ShaderStageCount); i++)
            count += (uint32_t)effect._uniforms[i].size();
 
        return count;
    }

    Vulkan::Uniform* findUniform(Vulkan::EffectDescriptor& effect, Vulkan::ShaderStage stage, uint32_t binding)
    {
        uint32_t index = (uint32_t)stage;
        for (size_t i = 0; i < effect._uniforms[index].size(); i++)
        {
            Vulkan::Uniform* uniform = &effect._uniforms[index][i];
            if (uniform->_binding == binding)
                return uniform;
        }

        return nullptr;
    }
}

uint32_t Vulkan::EffectDescriptor::collectDescriptorSetsOfType(VkDescriptorType type, uint32_t frame, VkDescriptorSet* result)
{
    uint32_t count = 0;
    for (int i = 0; i < static_cast<int>(Vulkan::ShaderStage::ShaderStageCount); i++)
    {
        for (int j = 0; j < (int)_uniforms[i].size(); j++)
        {
            if (_uniforms[i][j]._type == type)
            {
                Vulkan::UniformAggregate& aggregate = _uniforms[i][j]._frames[frame];
                result[count++] = aggregate._descriptorSet;
            }
        }
    }

    return count;
}

uint32_t Vulkan::EffectDescriptor::collectDescriptorSets(uint32_t frame, VkDescriptorSet* result)
{
    uint32_t count = 0;
    for (int i = 0; i < static_cast<int>(Vulkan::ShaderStage::ShaderStageCount); i++)
    {
        for (int j = 0; j < (int)_uniforms[i].size(); j++)
        {
            Vulkan::UniformAggregate& aggregate = _uniforms[i][j]._frames[frame];
            result[count++] = aggregate._descriptorSet;
        }
    }

    return count;
}


void Vulkan::EffectDescriptor::collectDescriptorSetLayouts(std::vector<VkDescriptorSetLayout>& layouts)
{
    for (int i = 0; i < static_cast<int>(Vulkan::ShaderStage::ShaderStageCount); i++)
    {
        for (int j = 0; j < (int)_uniforms[i].size(); j++)
        {
            Uniform & uniform = _uniforms[i][j];
            layouts.push_back(uniform._descriptorSetLayout);
        }
    }
}

uint32_t Vulkan::EffectDescriptor::collectUniformsOfType(VkDescriptorType type, Vulkan::ShaderStage stage, Uniform** result)
{
    uint32_t count = 0;
    for (int j = 0; j < (int)_uniforms[(uint32_t)stage].size(); j++)
    {
        if (_uniforms[(uint32_t)stage][j]._type == type)
            result[count++] = &_uniforms[(uint32_t)stage][j];
    }

    return count;
}


uint32_t Vulkan::EffectDescriptor::collectUniformsOfType(VkDescriptorType type, Uniform ** result)
{
    uint32_t count = 0;
    for (int i = 0; i < static_cast<int>(Vulkan::ShaderStage::ShaderStageCount); i++)
    {
        for (int j = 0; j < (int)_uniforms[i].size(); j++)
        {
            if (_uniforms[i][j]._type == type)
                result[count++] = &_uniforms[i][j];
        }
    }

    return count;
}

uint32_t Vulkan::EffectDescriptor::totalTypeCount(VkDescriptorType type) const
{
    uint32_t count = 0;
    for (int i = 0; i < static_cast<int>(Vulkan::ShaderStage::ShaderStageCount); i++)
    {
        for (int j = 0; j < (int)_uniforms[i].size(); j++)
        {
            if (_uniforms[i][j]._type == type)
                count++;
        }
    }

    return count;
}

uint32_t Vulkan::EffectDescriptor::totalTypeCount(Vulkan::ShaderStage stage, VkDescriptorType type) const
{
    uint32_t count = 0;
    for (int j = 0; j < (int)_uniforms[(uint32_t)stage].size(); j++)
    {
        if (_uniforms[(uint32_t)stage][j]._type == type)
            count++;
    }

    return count;
}


uint32_t Vulkan::EffectDescriptor::totalImagesCount() const
{
    return totalTypeCount(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
}


uint32_t Vulkan::EffectDescriptor::totalSamplerCount() const
{
    return totalTypeCount(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
}

uint32_t Vulkan::EffectDescriptor::totalNumUniformBuffers() const
{
    return totalTypeCount(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
}

uint32_t Vulkan::EffectDescriptor::totalNumUniforms() const
{
    return numUniforms(*this);
}


uint32_t Vulkan::EffectDescriptor::addUniformSampler(Vulkan::Context& context, Vulkan::ShaderStage stage)
{
    Vulkan::Uniform newUniform{};
    newUniform._type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    newUniform._binding = numUniforms(*this);
    newUniform._frames.resize(context._rawImages.size());
    _uniforms[(int)stage].push_back(newUniform);
    return newUniform._binding;
}

uint32_t Vulkan::EffectDescriptor::addUniformImage(Vulkan::Context& context, Vulkan::ShaderStage stage)
{
    Vulkan::Uniform newUniform{};
    newUniform._type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    newUniform._binding = numUniforms(*this);
    newUniform._frames.resize(context._rawImages.size());
    _uniforms[(int)stage].push_back(newUniform);
    return newUniform._binding;
}

uint32_t Vulkan::EffectDescriptor::addUniformBuffer(Vulkan::Context& context, Vulkan::ShaderStage stage, uint32_t size)
{
    Vulkan::Uniform newUniform{};
    newUniform._type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    newUniform._binding = numUniforms(*this);
    for (uint32_t i = 0; i < (uint32_t)context._rawImages.size(); i++)
    {
        UniformAggregate aggregate;
        aggregate._buffer._size = size;
        newUniform._frames.push_back(aggregate);
    }
    _uniforms[(int)stage].push_back(newUniform);

    return newUniform._binding;
}

bool Vulkan::EffectDescriptor::bindImage(Vulkan::Context& context, Vulkan::ShaderStage shaderStage, uint32_t binding, VkImageView imageView)
{
    Uniform* uniform = findUniform(*this, shaderStage, binding);
    assert(uniform != nullptr);
    if (uniform == nullptr)
        return false;

    assert(uniform->_type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    if (uniform->_type != VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
        return false;

    for (UniformAggregate& aggregate : uniform->_frames)
    {
        aggregate._imageView = imageView;

        VkDescriptorImageInfo imageInfo;

        imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        imageInfo.imageView = imageView;
        imageInfo.sampler = VK_NULL_HANDLE;

        VkWriteDescriptorSet writeSet = { };
        writeSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeSet.descriptorCount = 1;
        writeSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        writeSet.dstArrayElement = 0;
        writeSet.dstBinding = binding;
        writeSet.dstSet = aggregate._descriptorSet;
        writeSet.pBufferInfo = VK_NULL_HANDLE;
        writeSet.pImageInfo = &imageInfo;
        writeSet.pNext = VK_NULL_HANDLE;
        writeSet.pTexelBufferView = VK_NULL_HANDLE;

        vkUpdateDescriptorSets(context._device, 1, &writeSet, 0, nullptr);
    }


    return true;
}

bool Vulkan::EffectDescriptor::bindSampler(Vulkan::Context & context, Vulkan::ShaderStage shaderStage, uint32_t binding, VkImageView imageView, VkSampler sampler)
{
    Uniform* uniform = findUniform(*this, shaderStage, binding);
    assert(uniform != nullptr);
    if (uniform == nullptr)
        return false;

    assert(uniform->_type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    if (uniform->_type != VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
        return false;

    for (UniformAggregate& aggregate : uniform->_frames)
    {
        aggregate._imageView = imageView;
        aggregate._sampler = sampler;

        VkDescriptorImageInfo imageInfo;

        imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        imageInfo.imageView = imageView;
        imageInfo.sampler = sampler;

        VkWriteDescriptorSet writeSet = { };
        writeSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeSet.descriptorCount = 1;
        writeSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writeSet.dstArrayElement = 0;
        writeSet.dstBinding = binding;
        writeSet.dstSet = aggregate._descriptorSet;
        writeSet.pBufferInfo = VK_NULL_HANDLE;
        writeSet.pImageInfo = &imageInfo;
        writeSet.pNext = VK_NULL_HANDLE;
        writeSet.pTexelBufferView = VK_NULL_HANDLE;

        vkUpdateDescriptorSets(context._device, 1, &writeSet, 0, nullptr);
    }


    return true;
}

///////////////////////////////////// Vulkan Shader ///////////////////////////////////////////////////////////////////


Vulkan::Shader::Shader(const std::string & filename, VkShaderStageFlagBits  type)
:_filename(filename)
,_type(type)
{
    
}


///////////////////////////////////// Vulkan Vertex ///////////////////////////////////////////////////////////////////


///////////////////////////////////// Vulkan AppDescriptor ///////////////////////////////////////////////////////////////////

Vulkan::AppDescriptor::AppDescriptor()
:_window(nullptr)
, _chosenPhysicalDevice(0)
{
}

///////////////////////////////////// Vulkan BufferDescriptor ///////////////////////////////////////////////////////////////////


bool Vulkan::BufferDescriptor::fill(VkDevice device, const void * srcData, VkDeviceSize amount)
{
    void * data = nullptr;
	const VkResult mapResult = vkMapMemory(device, _memory, 0, amount, 0, &data);
	assert(mapResult == VK_SUCCESS);
    if(mapResult != VK_SUCCESS)
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
    const VkResult allocationResult = vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);
	assert(allocationResult == VK_SUCCESS);
    
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    
    const VkResult beginCommandBufferResult = vkBeginCommandBuffer(commandBuffer, &beginInfo);
	assert(beginCommandBufferResult == VK_SUCCESS);

    VkBufferCopy copyRegion = {};
    copyRegion.srcOffset = 0; // Optional
    copyRegion.dstOffset = 0; // Optional
    copyRegion.size = amount;
    vkCmdCopyBuffer(commandBuffer, src._buffer, _buffer, 1, &copyRegion);
    
	const VkResult endCommandBufferResult = vkEndCommandBuffer(commandBuffer);
    assert(endCommandBufferResult == VK_SUCCESS);

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    
    const VkResult submitResult = vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
	assert(submitResult == VK_SUCCESS);
	const VkResult waitResult = vkQueueWaitIdle(queue);
	assert(waitResult == VK_SUCCESS);

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
    return true;
}

bool Vulkan::BufferDescriptor::copyTo(VkDevice device,
            VkCommandPool commandPool,
            VkQueue queue,
            VkImage image,
            unsigned int width,
            unsigned int height)
{
    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    const VkResult allocationResult = vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);
    assert(allocationResult == VK_SUCCESS);

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    const VkResult beginCommandBufferResult = vkBeginCommandBuffer(commandBuffer, &beginInfo);
    assert(beginCommandBufferResult == VK_SUCCESS);

    VkBufferImageCopy region = {};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = VkOffset3D{0, 0, 0};
    region.imageExtent = VkExtent3D{width, height, 1};

    vkCmdCopyBufferToImage(commandBuffer, _buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    const VkResult endCommandBufferResult = vkEndCommandBuffer(commandBuffer);
    assert(endCommandBufferResult == VK_SUCCESS);

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    const VkResult submitResult = vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
    assert(submitResult == VK_SUCCESS);
    const VkResult waitResult = vkQueueWaitIdle(queue);
    assert(waitResult == VK_SUCCESS);

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
    return true;
}


///////////////////////////////////// Vulkan Methods ///////////////////////////////////////////////////////////////////

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
		SDL_LogError(0, "VulkanDebugReportCallback : %s - %s\n", pLayerPrefix, pMessage);
		return VK_TRUE;
	}
}

// TODO - this method leaks device memory
bool Vulkan::createImage(Vulkan::Context & context,
                 unsigned int width,
                 unsigned int height,
                 unsigned int depth,
                 VkFormat requiredFormat,
                 VkImageTiling requiredTiling,
                 VkImageUsageFlags requiredUsage,
                 VkImage& resultImage)
{
    VkImageCreateInfo createInfo;
    memset(&createInfo, 0, sizeof(createInfo));
    createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    createInfo.pNext = NULL;
    createInfo.flags = 0;
    createInfo.imageType = (depth > 1) ? VK_IMAGE_TYPE_3D : VK_IMAGE_TYPE_2D;
    createInfo.format = requiredFormat;
    createInfo.extent.width = width;
    createInfo.extent.height = height;
    createInfo.extent.depth = depth;
    createInfo.mipLevels = 1;
    createInfo.arrayLayers = 1;
    createInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    createInfo.tiling = requiredTiling;
    createInfo.usage = requiredUsage;
    createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.queueFamilyIndexCount = 0;
    createInfo.pQueueFamilyIndices = NULL;
    createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    const VkResult result = vkCreateImage(context._device, &createInfo, VK_NULL_HANDLE, &resultImage);
    if (result != VK_SUCCESS)
        return false;

    VkMemoryRequirements imageMemoryRequirements;
    vkGetImageMemoryRequirements(context._device, resultImage, &imageMemoryRequirements);

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = imageMemoryRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(context, imageMemoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkDeviceMemory imageMemory;
    if (vkAllocateMemory(context._device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate image memory!");
    }

    vkBindImageMemory(context._device, resultImage, imageMemory, 0);

    return true;
}

bool Vulkan::createImageView(Vulkan::Context& context, VkImage image, VkFormat requiredFormat, VkImageAspectFlags requiredAspectFlags, VkImageView& result)
{
    VkImageViewCreateInfo createInfo;
    memset(&createInfo, 0, sizeof(createInfo));
    createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    createInfo.pNext = NULL;
    createInfo.flags = 0;
    createInfo.image = image;
    createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    createInfo.format = requiredFormat;
    createInfo.components = VkComponentMapping{ VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
    createInfo.subresourceRange.aspectMask = requiredAspectFlags;
    createInfo.subresourceRange.baseMipLevel = 0;
    createInfo.subresourceRange.levelCount = 1;
    createInfo.subresourceRange.baseArrayLayer = 0;
    createInfo.subresourceRange.layerCount = 1;

    const VkResult imageViewCreationResult = vkCreateImageView(context._device, &createInfo, VK_NULL_HANDLE, &result);
    if (imageViewCreationResult != VK_SUCCESS)
        return false;

    return true;
}




bool Vulkan::transitionImageLayout(Vulkan::Context & context, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout)
{
    std::vector<VkCommandBuffer> commandBuffers;
    if (!createCommandBuffers(context, context._commandPool, 1, &commandBuffers))
        return false;

    VkCommandBuffer & commandBuffer = commandBuffers[0];
    VkCommandBufferBeginInfo beginInfo;
    memset(&beginInfo, 0, sizeof(beginInfo));
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    beginInfo.pInheritanceInfo = NULL;
    beginInfo.pNext = NULL;

    const VkResult beginCommandResult = vkBeginCommandBuffer(commandBuffer, &beginInfo);
    if (beginCommandResult != VK_SUCCESS)
        return false;

    VkImageMemoryBarrier barrier;
    memset(&barrier, 0, sizeof(barrier));
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.pNext = NULL;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = 0;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
    ? VK_IMAGE_ASPECT_DEPTH_BIT
    : VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;

    switch (oldLayout)
    {
        case VK_IMAGE_LAYOUT_UNDEFINED:
        {
            switch (newLayout)
            {
            case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
                barrier.srcAccessMask = 0;
                barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
                destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
                break;
            case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
                barrier.srcAccessMask = 0;
                barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
                destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
                break;
            default:
                return false;
            }
        }
            break;
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
        {
            switch (newLayout)
            {
                case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
                    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                    sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
                    destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
                    break;
                case VK_IMAGE_LAYOUT_GENERAL:
                    barrier.srcAccessMask = 0;
                    barrier.dstAccessMask = 0;
                    sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
                    destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
                    break;
                default:
                    return false;
            }
        }
            break;
        default:
            return false;
    }

    vkCmdPipelineBarrier(
                         commandBuffer,
                         sourceStage,
                         destinationStage,
                         0,
                         0,
                         nullptr,
                         0,
                         nullptr,
                         1,
                         &barrier);

    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(context._graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(context._graphicsQueue);

    vkFreeCommandBuffers(context._device, context._commandPool, 1, &commandBuffers[0]);

    return true;
}


bool Vulkan::setupDebugCallback(Vulkan::Context & context)
{
    if (!validationLayersEnabled)
        return true; // everything is true about the empty set

	// debug utils
	{
		const char * debugFunctionCreatorName = "vkCreateDebugUtilsMessengerEXT";
		auto debugUtilsMessengerCreator = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(context._instance, debugFunctionCreatorName);
		if (debugUtilsMessengerCreator == nullptr)
		{
#if !defined(__APPLE__) // function doesn't seeem to exist with moltenvk
            SDL_LogError(0, "Could not create function: %s\n", debugFunctionCreatorName);
            return false;
#endif
		}


#if !defined(__APPLE__) // function doesn't seeem to exist with moltenvk
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
		assert(debugUtilsCreationResult == VK_SUCCESS);
		if (debugUtilsCreationResult != VK_SUCCESS)
		{
			SDL_LogError(0, "Failed to create callback for method %s\n", debugFunctionCreatorName);
			// TODO: should probably destroy the callbackCreator here
			return false;
		}
#endif
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
		createInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | /*VK_DEBUG_REPORT_INFORMATION_BIT_EXT  | */
			VK_DEBUG_REPORT_WARNING_BIT_EXT |
			VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
		createInfo.pUserData = nullptr;
		createInfo.pfnCallback = &VulkanDebugReportCallback;

		VkResult debugReportCreationResult = debugReportMessengerCreator(context._instance, &createInfo, nullptr, &context._debugReportCallback);
		assert(debugReportCreationResult == VK_SUCCESS);
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
    const VkResult enumerateResult = vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
	assert(enumerateResult == VK_SUCCESS);
    if (layerCount > 0)
    {
        
        layers.resize(layerCount);
		const VkResult enumerateInstanceLayerResult = vkEnumerateInstanceLayerProperties(&layerCount, &layers[0]);
		assert(enumerateInstanceLayerResult == VK_SUCCESS);
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

bool Vulkan::createInstanceAndLoadExtensions(const Vulkan::AppDescriptor & appDesc, Vulkan::Context & context)
{
    
    // first count the number of instance extensions
    unsigned int instanceExtensionCount = 0;
    VkResult instanceEnumerateResult = vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtensionCount, nullptr);
	assert(instanceEnumerateResult == VK_SUCCESS);
	if (instanceEnumerateResult != VK_SUCCESS)
        return false;
    
    std::vector<VkExtensionProperties> instanceExtensions;
    instanceExtensions.resize(instanceExtensionCount);
    instanceEnumerateResult = vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtensionCount, &instanceExtensions[0]);
	assert(instanceEnumerateResult == VK_SUCCESS);
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
    if (!SDL_Vulkan_GetInstanceExtensions(appDesc._window, &requiredInstanceExtensionCount, nullptr))
    {
        SDL_LogError(0, "Failed to get number of extensions");
        return false;
    }
    
    std::vector<const char*> requiredInstanceExtensions;
    requiredInstanceExtensions.resize(requiredInstanceExtensionCount);
    if (!SDL_Vulkan_GetInstanceExtensions(appDesc._window, &requiredInstanceExtensionCount, &requiredInstanceExtensions[0]))
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
    vkAppInfo.apiVersion = appDesc._requiredVulkanVersion;
	vkAppInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	vkAppInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    vkAppInfo.pApplicationName = appDesc._appName.c_str();
    
    VkInstanceCreateInfo instanceCreateInfo;
    memset(&instanceCreateInfo, 0, sizeof(instanceCreateInfo));
    
    
    instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceCreateInfo.pApplicationInfo = &vkAppInfo;
#if !defined(__APPLE__)
	requiredInstanceExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
#endif
    instanceCreateInfo.enabledExtensionCount = (uint32_t)requiredInstanceExtensions.size();
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

bool Vulkan::createVulkanSurface(SDL_Window * window, Vulkan::Context & context) {
    
    context._surface = VK_NULL_HANDLE;
    if (SDL_Vulkan_CreateSurface(window, context._instance, &context._surface) == 0)
        return false;
    
    return true;
}

bool Vulkan::enumeratePhysicalDevices(Vulkan::AppDescriptor & appDesc, Vulkan::Context & context)
{
    uint32_t deviceCount = 0;
    // first count the number of physical devices by passing in NULL as the last argument
    VkResult countResult = vkEnumeratePhysicalDevices(context._instance, &deviceCount, NULL);
	assert(countResult == VK_SUCCESS);
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
	assert(enumerateResult == VK_SUCCESS);
	if (enumerateResult != VK_SUCCESS) {
        SDL_LogError(0, "vkEnumeratePhysicalDevices returned error code %d", countResult);
        return false;
    }
    
    unsigned int deviceIndex = 0;
    for (; deviceIndex < deviceCount; deviceIndex++) {
        VkPhysicalDevice device = devices[deviceIndex];
        vkGetPhysicalDeviceProperties(device, &context._deviceProperties);
        
        if (context._deviceProperties.apiVersion < appDesc._requiredVulkanVersion)
            continue;
        
        break;
    }
    
    if (deviceIndex == deviceCount)
        return false;
    
    appDesc._physicalDevices = devices;
    return true;
}

// prioritise discrete over integrated
bool Vulkan::choosePhysicalDevice(AppDescriptor &appDesc, Context & Context) {
    bool foundDevice = false;
    unsigned int currentPhysicalDevice = 0;
    VkPhysicalDeviceProperties currentPhysicalProperties;
    vkGetPhysicalDeviceProperties(appDesc._physicalDevices[currentPhysicalDevice], &currentPhysicalProperties);
    for (unsigned int i = 0; i < (unsigned int)appDesc._physicalDevices.size(); i++)
    {
        VkPhysicalDeviceProperties physicalProperties;
        vkGetPhysicalDeviceProperties(appDesc._physicalDevices[i], &physicalProperties);
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
    
    appDesc._chosenPhysicalDevice = currentPhysicalDevice;
    Context._physicalDevice = appDesc._physicalDevices[currentPhysicalDevice];
    SDL_LogInfo(0, "Chosen Vulkan Physical Device = %s. Driver version = %d\n", currentPhysicalProperties.deviceName, currentPhysicalProperties.driverVersion);
    
	// get the device features, so we can check against them later on
	vkGetPhysicalDeviceFeatures(Context._physicalDevice, &Context._physicalDeviceFeatures);

    return true;
}

bool Vulkan::lookupDeviceExtensions(AppDescriptor &appDesc) {
    uint32_t deviceExtensionsCount = 0;
    VkResult enumerateDeviceExtensionsResult = vkEnumerateDeviceExtensionProperties(appDesc._physicalDevices[appDesc._chosenPhysicalDevice],
                                                                                    nullptr,
                                                                                    &deviceExtensionsCount,
                                                                                    nullptr);
	assert(enumerateDeviceExtensionsResult == VK_SUCCESS);
	if (enumerateDeviceExtensionsResult != VK_SUCCESS)
        return false;
    
    if (deviceExtensionsCount > 0)
    {
        std::vector<VkExtensionProperties> deviceExtensions(deviceExtensionsCount);
		enumerateDeviceExtensionsResult = vkEnumerateDeviceExtensionProperties(appDesc._physicalDevices[appDesc._chosenPhysicalDevice],
                                             nullptr,
                                             &deviceExtensionsCount,
                                             &deviceExtensions[0]);
		assert(enumerateDeviceExtensionsResult == VK_SUCCESS);

        appDesc._deviceExtensions = deviceExtensions;
    }
    return true;
}

bool Vulkan::createDevice(AppDescriptor & appDesc, Context & context)
{
  uint32_t pQueueFamilyCount = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(appDesc._physicalDevices[appDesc._chosenPhysicalDevice], &pQueueFamilyCount, NULL);
  assert(pQueueFamilyCount != 0);
  if(pQueueFamilyCount == 0)
    return false;

  std::vector<VkQueueFamilyProperties> queueProperties(pQueueFamilyCount);
  vkGetPhysicalDeviceQueueFamilyProperties(appDesc._physicalDevices[appDesc._chosenPhysicalDevice], &pQueueFamilyCount, &queueProperties[0]);
  
  
  
  VkDeviceQueueCreateInfo deviceQueueCreateInfo;
  VkDeviceCreateInfo deviceCreateInfo;
  memset(&deviceQueueCreateInfo, 0, sizeof(deviceQueueCreateInfo));
  memset(&deviceCreateInfo, 0, sizeof(deviceCreateInfo));
  
  deviceQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  deviceQueueCreateInfo.queueFamilyIndex = appDesc._chosenPhysicalDevice;
  deviceQueueCreateInfo.queueCount = 1;
  static constexpr float fQueuePriority = 1.0f;
  deviceQueueCreateInfo.pQueuePriorities = &fQueuePriority;
  
  deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  deviceCreateInfo.queueCreateInfoCount = 1;
  deviceCreateInfo.pQueueCreateInfos = &deviceQueueCreateInfo;
  deviceCreateInfo.pEnabledFeatures = &context._physicalDeviceFeatures;
    
  static const char * deviceExtensionNames[] = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
  };
  static const unsigned int numExtensionNames = sizeof(deviceExtensionNames) / sizeof(const char*);
  deviceCreateInfo.ppEnabledExtensionNames = &deviceExtensionNames[0];
  deviceCreateInfo.enabledExtensionCount = numExtensionNames;
  
  VkResult creationResult = vkCreateDevice(appDesc._physicalDevices[appDesc._chosenPhysicalDevice], &deviceCreateInfo, nullptr /* no allocation callbacks at this time */, &context._device);
  assert(creationResult == VK_SUCCESS);
  return creationResult == VK_SUCCESS;
}

bool Vulkan::createQueue(AppDescriptor & appDesc, Context & context)
{
    vkGetDeviceQueue(context._device, appDesc._chosenPhysicalDevice, 0, &context._graphicsQueue);
    vkGetDeviceQueue(context._device, appDesc._chosenPhysicalDevice, 0, &context._presentQueue);
    return true;
}

bool Vulkan::createSwapChain(AppDescriptor & appDesc, Context & context)
{
    // get surface capabilities
    VkResult surfaceCapabilitiesResult = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(context._physicalDevice, context._surface, &context._surfaceCapabilities);
	assert(surfaceCapabilitiesResult == VK_SUCCESS);
	if (surfaceCapabilitiesResult != VK_SUCCESS)
        return false;
    
    unsigned int surfaceFormatsCount = 0;
    VkResult surfaceFormatsCountResult = vkGetPhysicalDeviceSurfaceFormatsKHR(context._physicalDevice, context._surface, &surfaceFormatsCount, nullptr);
	assert(surfaceFormatsCountResult == VK_SUCCESS);
	if (surfaceFormatsCountResult != VK_SUCCESS || surfaceFormatsCount == 0)
        return false;
    
    appDesc._surfaceFormats.resize(surfaceFormatsCount);
    VkResult surfaceFormatsResult = vkGetPhysicalDeviceSurfaceFormatsKHR(context._physicalDevice, context._surface, &surfaceFormatsCount, &appDesc._surfaceFormats[0]);
	assert(surfaceFormatsResult == VK_SUCCESS);
	if (surfaceFormatsResult != VK_SUCCESS || surfaceFormatsCount == 0)
        return false;

	// check if present support for interface
	VkBool32 presentSupported = false;
	VkResult surfaceSupportedResult = vkGetPhysicalDeviceSurfaceSupportKHR(context._physicalDevice, 0, context._surface, &presentSupported);
	assert(surfaceSupportedResult == VK_SUCCESS);
	if(!presentSupported)
		return false;
    
    VkSwapchainCreateInfoKHR swapChainCreateInfo;
    memset(&swapChainCreateInfo, 0, sizeof(swapChainCreateInfo));
    int width = 0, height = 0;
    
    const unsigned int imageCount = std::max<uint32_t>(std::min<uint32_t>(3, context._surfaceCapabilities.maxImageCount), context._surfaceCapabilities.minImageCount);
    if (imageCount == 0)
        return false;
    
#if defined(__APPLE__)
    SDL_GL_GetDrawableSize(appDesc._window, &width, &height);
#else
    SDL_Vulkan_GetDrawableSize(appDesc._window, &width, &height);
#endif
    if (width <= 0 || height <= 0)
        return false;
    
    context._surfaceFormat = appDesc._surfaceFormats[0];
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
//    swapChainCreateInfo.oldSwapchain = context._swapChain;
    swapChainCreateInfo.oldSwapchain = VK_NULL_HANDLE;

    VkResult swapChainCreationResult = vkCreateSwapchainKHR(context._device, &swapChainCreateInfo, nullptr, &context._swapChain);
	assert(swapChainCreationResult == VK_SUCCESS);
	if (swapChainCreationResult != VK_SUCCESS)
        return false;
    
    // if success check how many images will actually be in the swap chain
    unsigned int vkImageCount = 0;
    VkResult getVkImageCountFromSwapChainResult = vkGetSwapchainImagesKHR(context._device, context._swapChain, &vkImageCount, nullptr);
	assert(getVkImageCountFromSwapChainResult == VK_SUCCESS);
	if (getVkImageCountFromSwapChainResult != VK_SUCCESS || vkImageCount == 0)
        return false;
    
    context._rawImages.resize(vkImageCount);
    VkResult getSwapChainImagesResult = vkGetSwapchainImagesKHR(context._device, context._swapChain, &vkImageCount, &context._rawImages[0]);
	assert(getSwapChainImagesResult == VK_SUCCESS);

    return true;
}

bool Vulkan::createDepthBuffers(AppDescriptor & appDesc, Context & context)
{
	constexpr VkImageTiling requiredTiling = VK_IMAGE_TILING_OPTIMAL;

	VkFormat depthFormat = findDepthFormat(context, requiredTiling);
	context._depthImageViews.resize(context._rawImages.size());
	context._depthImages.resize(context._rawImages.size());
	context._depthMemory.resize(context._rawImages.size());
	for (unsigned int i = 0; i < (unsigned int)context._depthImageViews.size(); i++)
	{
		VkImage depthImage;
		VkImageView depthImageView;
		VkDeviceMemory depthImageMemory;
		if(!createImage(context,
			context._swapChainSize.width,
			context._swapChainSize.height,
			1,
			depthFormat,
			requiredTiling,
			VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
			depthImage))
			continue;

		if (!allocateImageMemory(context, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, depthImage, depthImageMemory))
			continue;

		if(!createImageView(context, depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, depthImageView))
			continue;

		if (!transitionImageLayout(context, depthImage, depthFormat, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL))
			continue;

		context._depthImages[i] = depthImage;
		context._depthMemory[i] = depthImageMemory;
		context._depthImageViews[i] = depthImageView;

	}

	return true;
}

bool Vulkan::createColorBuffers(Context & context)
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
		assert(imageViewCreationResult == VK_SUCCESS);
        if (imageViewCreationResult != VK_SUCCESS)
            return false;
        
        context._colorBuffers.push_back(imageView);
    }
    
    return true;
}

bool Vulkan::createRenderPass(Context & Context, VkRenderPass * result, bool clearColorBuffer)
{
    VkAttachmentReference colorAttachmentReference;
    memset(&colorAttachmentReference, 0, sizeof(colorAttachmentReference));
    colorAttachmentReference.attachment = 0;
    colorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depthAttachmentReference;
	memset(&depthAttachmentReference, 0, sizeof(depthAttachmentReference));
	depthAttachmentReference.attachment = 1;
	depthAttachmentReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    
    VkSubpassDescription subpassDescription;
    memset(&subpassDescription, 0, sizeof(subpassDescription));
    subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDescription.colorAttachmentCount = 1;
    subpassDescription.pColorAttachments = &colorAttachmentReference;
//	subpassDescription.pDepthStencilAttachment = &depthAttachmentReference;

    VkAttachmentDescription colorAttachment;
    memset(&colorAttachment, 0, sizeof(colorAttachment));
    colorAttachment.format = Context._surfaceFormat.format;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = clearColorBuffer ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentDescription depthAttachment;
	memset(&depthAttachment, 0, sizeof(depthAttachment));
	depthAttachment.format = findDepthFormat(Context, VK_IMAGE_TILING_OPTIMAL);
	depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	depthAttachment.loadOp = clearColorBuffer ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;




	VkSubpassDependency dependency;
	memset(&dependency, 0, sizeof(dependency));
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    
    VkRenderPassCreateInfo createInfo;
    memset(&createInfo, 0, sizeof(createInfo));
//    VkAttachmentDescription attachmentDescriptions[] = { colorAttachment, depthAttachment };
    VkAttachmentDescription attachmentDescriptions[] = { colorAttachment};
    createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    createInfo.attachmentCount = sizeof(attachmentDescriptions) / sizeof(VkAttachmentDescription);
	createInfo.pAttachments = &attachmentDescriptions[0];
    createInfo.subpassCount = 1;
    createInfo.pSubpasses = &subpassDescription;
	createInfo.dependencyCount = 1;
	createInfo.pDependencies = &dependency;
    
	VkRenderPass renderPass;
    VkResult createRenderPassResult = vkCreateRenderPass(Context._device, &createInfo, nullptr, &renderPass);
	assert(createRenderPassResult == VK_SUCCESS);
	if (createRenderPassResult != VK_SUCCESS)
        return false;

	*result = renderPass;
    
    return true;
}

bool Vulkan::createFrameBuffers(Context & Context)
{
	for(unsigned int i=0 ; i < (unsigned int)Context._colorBuffers.size() ; i++ )
    {
        VkFramebufferCreateInfo createInfo;
        memset(&createInfo, 0, sizeof(createInfo));
        
//        VkImageView attachments[] = { Context._colorBuffers[i], Context._depthImageViews[i] };
        VkImageView attachments[] = { Context._colorBuffers[i]};
        createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        createInfo.renderPass = Context._renderPass;
        createInfo.attachmentCount = sizeof(attachments) / sizeof(VkImageView);
		createInfo.pAttachments = &attachments[0];
        createInfo.width = Context._swapChainSize.width;
        createInfo.height = Context._swapChainSize.height;
		createInfo.layers = 1;
        
        VkFramebuffer frameBuffer;
        VkResult frameBufferCreationResult = vkCreateFramebuffer(Context._device, &createInfo, nullptr, &frameBuffer);
		assert(frameBufferCreationResult == VK_SUCCESS);
		if (frameBufferCreationResult != VK_SUCCESS)
            return false;
        
        Context._frameBuffers.push_back(frameBuffer);
    }
    
    return true;
}

bool Vulkan::createShaderModules(AppDescriptor & appDesc, Context & context, std::vector<Shader> & shaders)
{
    for (Shader & shader : shaders)
    {
        VkShaderModuleCreateInfo createInfo;
        memset(&createInfo, 0, sizeof(createInfo));
        
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = shader._byteCode.size();
        createInfo.pCode = reinterpret_cast<uint32_t*>(&shader._byteCode[0]);
        
        VkShaderModule module;
        VkResult result;
        result = vkCreateShaderModule(context._device, &createInfo, nullptr, &module);
		assert(result == VK_SUCCESS);
		if (result != VK_SUCCESS)
        {
            SDL_LogError(0, "Failed to create shader module for file %s with error %d\n", shader._filename.c_str(), (int)result);
            return false;
        }
        
        shader._shaderModule = module;
    }
    
    return true;
}

bool Vulkan::createPipelineCache(AppDescriptor & appDesc, Context & context)
{
	if (context._pipelineCache == VK_NULL_HANDLE)
	{
		VkPipelineCacheCreateInfo createInfo;
		createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
		createInfo.flags = 0;
		createInfo.initialDataSize = 0;
		createInfo.pInitialData = nullptr;
		createInfo.pNext = nullptr;

		VkResult creationResult = vkCreatePipelineCache(context._device, &createInfo, VK_NULL_HANDLE, &context._pipelineCache);
		return creationResult == VK_SUCCESS;
	}
	else
		return true;
}

bool Vulkan::createComputePipeline(AppDescriptor& appDesc, Context& context, ComputePipelineCustomizationCallback computePipelineCreationCallback, Vulkan::EffectDescriptor& effect)
{
    VkComputePipelineCreateInfo createInfo;
    memset(&createInfo, 0, sizeof(VkComputePipelineCreateInfo));

    const std::vector<Shader>& shaderModules = effect._shaderModules;

    std::vector<VkPipelineShaderStageCreateInfo> shaderStages;

    for (unsigned int i = 0; i < (unsigned int)shaderModules.size(); i++)
    {
        VkPipelineShaderStageCreateInfo shaderCreateInfo;
        memset(&shaderCreateInfo, 0, sizeof(VkPipelineShaderStageCreateInfo));

        shaderCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderCreateInfo.stage = shaderModules[i]._type;
        shaderCreateInfo.module = shaderModules[i]._shaderModule;
        shaderCreateInfo.pName = "main";

        shaderStages.push_back(shaderCreateInfo);
    }


    createInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    createInfo.pNext = VK_NULL_HANDLE;
    createInfo.flags = 0;
    createInfo.stage = shaderStages[0];

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo;
    memset(&pipelineLayoutCreateInfo, 0, sizeof(VkPipelineLayoutCreateInfo));
    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    //    pipelineLayoutCreateInfo.setLayoutCount = (uint32_t)effect._descriptorSetLayouts.size();

    std::vector<VkDescriptorSetLayout> layouts;
    effect.collectDescriptorSetLayouts(layouts);
    pipelineLayoutCreateInfo.setLayoutCount = (uint32_t)layouts.size();
    pipelineLayoutCreateInfo.pSetLayouts = &layouts[0];
    pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
    pipelineLayoutCreateInfo.pPushConstantRanges = nullptr;

    VkResult createPipelineLayoutResult = vkCreatePipelineLayout(context._device, &pipelineLayoutCreateInfo, nullptr, &effect._pipelineLayout);
    assert(createPipelineLayoutResult == VK_SUCCESS);
    if (createPipelineLayoutResult != VK_SUCCESS)
    {
        SDL_LogError(0, "Failed to create pipeline layout\n");
        return false;
    }
    createInfo.layout = effect._pipelineLayout;
    createInfo.basePipelineHandle = effect._pipeline;
    createInfo.basePipelineIndex = 0;

    const VkResult createComputePipelineResult = vkCreateComputePipelines(context._device, VK_NULL_HANDLE, 1, &createInfo, VK_NULL_HANDLE, &effect._pipeline);
    assert(createComputePipelineResult == VK_SUCCESS);
    if (createComputePipelineResult != VK_SUCCESS)
    {
        SDL_LogError(0, "Failed to create vulkan compute pipeline\n");
        return false;
    }

    return true;

}

bool Vulkan::createGraphicsPipeline(AppDescriptor & appDesc, Context & context, GraphicsPipelineCustomizationCallback graphicsPipelineCreationCallback, Vulkan::EffectDescriptor & effect)
{
    VkGraphicsPipelineCreateInfo createInfo;
    memset(&createInfo, 0, sizeof(VkGraphicsPipelineCreateInfo));

    const std::vector<Shader>& shaderModules = effect._shaderModules;

    std::vector<VkPipelineShaderStageCreateInfo> shaderStages;

    for (unsigned int i = 0; i < (unsigned int)shaderModules.size(); i++)
    {
        VkPipelineShaderStageCreateInfo shaderCreateInfo;
        memset(&shaderCreateInfo, 0, sizeof(VkPipelineShaderStageCreateInfo));

        shaderCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderCreateInfo.stage = shaderModules[i]._type;
        shaderCreateInfo.module = shaderModules[i]._shaderModule;
        shaderCreateInfo.pName = "main";

        shaderStages.push_back(shaderCreateInfo);
    }


    createInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    createInfo.stageCount = (uint32_t)shaderStages.size();
    createInfo.pStages = (createInfo.stageCount == 0) ? nullptr : &shaderStages[0];

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
    viewport.y = (float)context._swapChainSize.height;
    viewport.width = (float)context._swapChainSize.width;
    viewport.height = -(float)context._swapChainSize.height;
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
    rasterizerCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
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

    VkPipelineDepthStencilStateCreateInfo depthStencilCreateInfo;
    memset(&depthStencilCreateInfo, 0, sizeof(depthStencilCreateInfo));
    depthStencilCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencilCreateInfo.depthTestEnable = VK_TRUE;
    depthStencilCreateInfo.depthWriteEnable = VK_TRUE;
    depthStencilCreateInfo.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencilCreateInfo.depthBoundsTestEnable = VK_FALSE;
    depthStencilCreateInfo.stencilTestEnable = VK_FALSE;
//    createInfo.pDepthStencilState = &depthStencilCreateInfo;

    VkPipelineColorBlendAttachmentState colorBlendAttachmentCreateInfo;
    memset(&colorBlendAttachmentCreateInfo, 0, sizeof(VkPipelineColorBlendAttachmentState));
    colorBlendAttachmentCreateInfo.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachmentCreateInfo.blendEnable = VK_TRUE;
    colorBlendAttachmentCreateInfo.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachmentCreateInfo.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachmentCreateInfo.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachmentCreateInfo.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachmentCreateInfo.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
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

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo;
    memset(&pipelineLayoutCreateInfo, 0, sizeof(VkPipelineLayoutCreateInfo));
    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;


    std::vector<VkDescriptorSetLayout> layouts;
    effect.collectDescriptorSetLayouts(layouts);
    pipelineLayoutCreateInfo.setLayoutCount = (uint32_t)layouts.size();
//    pipelineLayoutCreateInfo.setLayoutCount = (uint32_t)effect._descriptorSetLayouts.size();
    pipelineLayoutCreateInfo.pSetLayouts = &layouts[0];
    pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
    pipelineLayoutCreateInfo.pPushConstantRanges = nullptr;

    VkResult createPipelineLayoutResult = vkCreatePipelineLayout(context._device, &pipelineLayoutCreateInfo, nullptr, &effect._pipelineLayout);
    assert(createPipelineLayoutResult == VK_SUCCESS);
    if (createPipelineLayoutResult != VK_SUCCESS)
    {
        SDL_LogError(0, "Failed to create graphics pipeline layout\n");
        return false;
    }
    createInfo.layout = effect._pipelineLayout;
    createInfo.renderPass = context._renderPass;
    createInfo.subpass = 0;
    createInfo.basePipelineHandle = VK_NULL_HANDLE;
    //    createInfo.basePipelineIndex = -1;

    VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo;
    memset(&dynamicStateCreateInfo, 0, sizeof(VkPipelineDynamicStateCreateInfo));
    dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    VkDynamicState dynamicState[] = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
    };
    dynamicStateCreateInfo.dynamicStateCount = 0;
    dynamicStateCreateInfo.pDynamicStates = nullptr;
    //	dynamicStateCreateInfo.pDynamicStates = &dynamicState[0];
    //	createInfo.pDynamicState = &dynamicStateCreateInfo;

    // Pipeline Vertex Input State
    VkPipelineVertexInputStateCreateInfo vertexInputInfo;
    memset(&vertexInputInfo, 0, sizeof(VkPipelineVertexInputStateCreateInfo));
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    std::vector<VkVertexInputBindingDescription> vertexInputBindingDescription;
    std::vector<VkVertexInputAttributeDescription> vertexInputAttributeDescription;
    createInfo.pVertexInputState = &vertexInputInfo;

    VkGraphicsPipelineCreateInfoDescriptor graphicsPipelineCreateInfoDescriptor(
        createInfo,
        shaderStages,
        vertexInputInfo,
        inputAssemblyInfo,
        viewport,
        scissor,
        viewportStateCreateInfo,
        rasterizerCreateInfo,
        multisamplingCreateInfo,
        depthStencilCreateInfo,
        colorBlendAttachmentCreateInfo,
        colorBlendingCreateInfo,
        pipelineLayoutCreateInfo,
        dynamicStateCreateInfo,
        vertexInputBindingDescription,
        vertexInputAttributeDescription
    );
    graphicsPipelineCreationCallback(graphicsPipelineCreateInfoDescriptor);

    // we can't set these variables until after the callback, as the vectors are dynamic in size, and the pointer to the contents might change
    vertexInputInfo.vertexBindingDescriptionCount = (uint32_t)vertexInputBindingDescription.size();
    vertexInputInfo.pVertexBindingDescriptions = &vertexInputBindingDescription[0];
    vertexInputInfo.vertexAttributeDescriptionCount = (uint32_t)vertexInputAttributeDescription.size();
    vertexInputInfo.pVertexAttributeDescriptions = &vertexInputAttributeDescription[0];

    const VkResult createGraphicsPipelineResult = vkCreateGraphicsPipelines(context._device, VK_NULL_HANDLE, 1, &createInfo, nullptr, &effect._pipeline);
    assert(createGraphicsPipelineResult == VK_SUCCESS);
    if (createGraphicsPipelineResult != VK_SUCCESS)
    {
        SDL_LogError(0, "Failed to create vulkan graphics pipeline\n");
        return false;
    }

    return true;
}

bool Vulkan::createDescriptorSetLayout(Context & context, Vulkan::EffectDescriptor& effect)
{

    for (unsigned int stage = 0; stage < static_cast<unsigned int>(Vulkan::ShaderStage::ShaderStageCount); stage++)
    {
        for (size_t i = 0; i < effect._uniforms[stage].size(); i++)
        {
            std::vector<VkDescriptorSetLayoutBinding> layouts;
            Uniform & uniform = effect._uniforms[stage][i];

            VkDescriptorSetLayoutBinding descriptorSetLayoutBinding = {};
            descriptorSetLayoutBinding.binding = (uint32_t)uniform._binding;
            descriptorSetLayoutBinding.descriptorCount = 1;
            descriptorSetLayoutBinding.descriptorType = uniform._type;
            descriptorSetLayoutBinding.pImmutableSamplers = nullptr;
            descriptorSetLayoutBinding.stageFlags = mapFromShaderStage(static_cast<Vulkan::ShaderStage>(stage));
            layouts.push_back(descriptorSetLayoutBinding);

            VkDescriptorSetLayoutCreateInfo createInfo = {};
            createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            createInfo.bindingCount = (uint32_t)layouts.size();
            createInfo.pBindings = layouts.empty() ? nullptr : &layouts[0];

            VkResult creationResult = vkCreateDescriptorSetLayout(context._device, &createInfo, nullptr, &uniform._descriptorSetLayout);
            assert(creationResult == VK_SUCCESS);
            if (creationResult != VK_SUCCESS)
                return false;
        }
    }

    return true;

}



bool Vulkan::createCommandPool(AppDescriptor & appDesc, Context & context, VkCommandPool * result)
{
    VkCommandPoolCreateInfo createInfo;
    memset(&createInfo, 0, sizeof(VkCommandPoolCreateInfo));
    createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    createInfo.queueFamilyIndex = appDesc._chosenPhysicalDevice;
    createInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    VkCommandPool commandPool;
	const VkResult createCommandPoolResult = vkCreateCommandPool(context._device, &createInfo, nullptr, &commandPool);
	assert(createCommandPoolResult == VK_SUCCESS);
	if (createCommandPoolResult != VK_SUCCESS)
    {
        SDL_LogError(0, "Failed to create command pool\n");
        return false;
    }
    *result = commandPool;
    return true;
}

bool Vulkan::resetCommandBuffers(Context & context, std::vector<VkCommandBuffer>& commandBuffers)
{
	for (unsigned int i = 0; i < (unsigned int)commandBuffers.size(); i++)
	{
		if (!context._fences.empty())
		{
			const VkResult waitForFencesResult = vkWaitForFences(context._device, 1, &context._fences[i], VK_TRUE, std::numeric_limits<uint64_t>::max());
//            const VkResult waitForFencesResult = vkWaitForFences(context._device, 1, &context._fences[i], VK_TRUE, 1);
//            assert(waitForFencesResult == VK_SUCCESS);
        }

		VkCommandBufferResetFlags resetFlags = VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT;
		const VkResult resetCommandBufferResult = vkResetCommandBuffer(commandBuffers[i], resetFlags);
		assert(resetCommandBufferResult == VK_SUCCESS);
		if (resetCommandBufferResult != VK_SUCCESS)
		{
			SDL_LogError(0, "call to vkResetCommandBuffer failed, i=%d\n", i);
			return false;
		}
	}
	return true;


}

std::vector<VkFence> Vulkan::createFences(Context & context)
{
    std::vector<VkFence> fences(context._frameBuffers.size());
    for(unsigned int i=0 ; i < (unsigned int)fences.size() ; i++)
    {
        VkFenceCreateInfo createInfo;
        memset(&createInfo, 0, sizeof(VkFenceCreateInfo));
        createInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        createInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        
		const VkResult createFenceResult = vkCreateFence(context._device, &createInfo, nullptr, &fences[i]);
		assert(createFenceResult == VK_SUCCESS);
        if(createFenceResult != VK_SUCCESS)
        {
            SDL_LogError(0, "Failed to create fences (%d)\n", i);
            return std::vector<VkFence>();
        }

    }
    return fences;
    
}

std::vector<VkSemaphore> Vulkan::createSemaphores(Context & context)
{
    std::vector<VkSemaphore> semaphores(context._frameBuffers.size());
    for(unsigned int i=0 ; i < (unsigned int)semaphores.size() ; i++)
    {
        VkSemaphoreCreateInfo createInfo;
        memset(&createInfo, 0, sizeof(VkSemaphoreCreateInfo));
        createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        
		const VkResult createSemaphoreResult = vkCreateSemaphore(context._device, &createInfo, nullptr, &semaphores[i]);
		assert(createSemaphoreResult == VK_SUCCESS);
        if(createSemaphoreResult != VK_SUCCESS)
        {
            SDL_LogError(0, "Failed to create semaphore (%d)\n", i);
            return std::vector<VkSemaphore>();
        }
    }
    return semaphores;
}

bool Vulkan::createSemaphores(AppDescriptor & appDesc, Context & context)
{
    context._imageAvailableSemaphores = createSemaphores(context);
    context._renderFinishedSemaphores = createSemaphores(context);
    context._fences = createFences(context);
    
    return context._imageAvailableSemaphores.size() == context._renderFinishedSemaphores.size()
    && context._imageAvailableSemaphores.size() == context._fences.size()
    && !context._imageAvailableSemaphores.empty();
}

bool Vulkan::createBuffer(Context & context, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, BufferDescriptor & bufDesc)
{
    VkBufferCreateInfo createInfo;
    memset(&createInfo, 0, sizeof(VkBufferCreateInfo));
    createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    createInfo.size = size;
    createInfo.usage = usage;
    createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    VkBuffer vertexBuffer;
	const VkResult createBufferResult = vkCreateBuffer(context._device, &createInfo, nullptr, &vertexBuffer);
	assert(createBufferResult == VK_SUCCESS);
    if(createBufferResult != VK_SUCCESS)
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
	const VkResult allocateMemoryResult = vkAllocateMemory(context._device, &allocInfo, nullptr, &vertexBufferMemory);
	assert(allocateMemoryResult == VK_SUCCESS);
	if (allocateMemoryResult != VK_SUCCESS)
    {
        SDL_LogError(0, "Failed to allocate memory necessary for the vertex buffer\n");
        return false;
    }
    
	const VkResult bindBufferResult = vkBindBufferMemory(context._device, vertexBuffer, vertexBufferMemory, 0);
	assert(bindBufferResult == VK_SUCCESS);
	if (bindBufferResult != VK_SUCCESS)
    {
        SDL_LogError(0, "Failed to bind vertex buffer memory\n");
        return false;
    }
    
    bufDesc._buffer = vertexBuffer;
    bufDesc._memory = vertexBufferMemory;
    bufDesc._size = (unsigned int)size;
    return true;
}

bool Vulkan::createVertexOrIndexBuffer(Context & context, const void * srcData, VkDeviceSize bufferSize, BufferDescriptor & result, BufferType type)
{
    
    BufferDescriptor stagingBufferDescriptor;
    if(!createBuffer(context, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBufferDescriptor))
    {
        SDL_LogError(0, "Failed to crete buffer of size %d bytes\n", (int)bufferSize);
        return false;
    }
    
    BufferDescriptor vertexBufferDescriptor;
    if(!createBuffer(context, bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | ((type == BufferType::Vertex) ? VK_BUFFER_USAGE_VERTEX_BUFFER_BIT : VK_BUFFER_USAGE_INDEX_BUFFER_BIT) , VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBufferDescriptor))
    {
        SDL_LogError(0, "Failed to crete buffer of size %d bytes\n", (int)bufferSize);
        return false;
    }
    
    stagingBufferDescriptor.fill(context._device, srcData, bufferSize);
    vertexBufferDescriptor.copyFrom(context._device, context._commandPool, context._graphicsQueue, stagingBufferDescriptor, bufferSize);
    
    result = vertexBufferDescriptor;
	destroyBufferDescriptor(context, stagingBufferDescriptor);
    return true;
    
}

bool Vulkan::createIndexAndVertexBuffer(AppDescriptor & appDesc, Context & context, std::vector<unsigned char> & vertexData, std::vector<unsigned char> & indexData, void * userData, Vulkan::Mesh & result)
{
    BufferDescriptor indexBuffer;
    BufferDescriptor vertexBuffer;

    if(indexData.empty() || vertexData.empty())
        return false;
    
    {
        // index buffer
        const void * data = indexData.data();
        const VkDeviceSize bufferSize =  indexData.size();
        if (!createVertexOrIndexBuffer(context, data, bufferSize, indexBuffer, BufferType::Index))
            return false;
    }
    
    {
        // vertex buffer
        const void * data = vertexData.data();
        const VkDeviceSize bufferSize = vertexData.size();
        if (!createVertexOrIndexBuffer(context, data, bufferSize, vertexBuffer, BufferType::Vertex))
            return false;
    }
    
    result._indexBuffer = indexBuffer;
    result._vertexBuffer = vertexBuffer;
    result._numIndices = (unsigned int)indexData.size() / sizeof(uint16_t);
    result._userData = userData;
    
    return true;

}

bool Vulkan::createUniformBuffer(AppDescriptor & appDesc, Context & context, VkDeviceSize bufferSize, BufferDescriptor & result)
{
    BufferDescriptor uniforms;
    
    if (!createBuffer(context, bufferSize,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        uniforms))
    {
        SDL_LogError(0, "Failed to create uniform buffer of size=%d\n", (int)bufferSize);
        return false;
    }

    result = uniforms;

    return true;
}

bool Vulkan::createDescriptorPool(Context & context, EffectDescriptor& effect)
{
	VkDescriptorPoolSize poolSizes[2] = {};
    int poolIndex = 0;
    if (effect.totalNumUniformBuffers() > 0)
    {
        poolSizes[poolIndex].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes[poolIndex].descriptorCount = static_cast<uint32_t>(effect.totalNumUniformBuffers() * context._rawImages.size());
        poolIndex++;
    }

    if (effect.totalSamplerCount() > 0)
    {
        poolSizes[poolIndex].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSizes[poolIndex].descriptorCount = static_cast<uint32_t>(effect.totalSamplerCount() * context._rawImages.size());
        poolIndex++;
    }

    if (effect.totalImagesCount() > 0)
    {
        poolSizes[poolIndex].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        poolSizes[poolIndex].descriptorCount = static_cast<uint32_t>(effect.totalImagesCount() * context._rawImages.size());
        poolIndex++;
    }

	VkDescriptorPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = poolIndex;
	poolInfo.pPoolSizes = &poolSizes[0];
    poolInfo.maxSets = static_cast<uint32_t>(effect.totalNumUniforms() * context._rawImages.size());

	VkResult creationResult = vkCreateDescriptorPool(context._device, &poolInfo, nullptr, &effect._descriptorPool);
	assert(creationResult == VK_SUCCESS);
	if (creationResult != VK_SUCCESS)
		return false;
        
	return true;
}

bool Vulkan::createDescriptorSet(AppDescriptor& appDesc, Context& context, EffectDescriptor& effect)
{
    const uint32_t framesCount = (uint32_t)context._rawImages.size();

    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = effect._descriptorPool;
    allocInfo.descriptorSetCount = 1;


    for (uint32_t stage = 0; stage < static_cast<int>(Vulkan::ShaderStage::ShaderStageCount); stage++)
    {
        const uint32_t uniformCount = effect.totalNumUniforms();
        static std::vector<Uniform*> uniforms(1);
        if (uniforms.size() < uniformCount)
            uniforms.resize(2 * (uniformCount + 1)); // amortise resizing

        for (Uniform & uniform : effect._uniforms[stage])
        {           
            std::vector<VkDescriptorSetLayout> descriptorSetLayouts(1, uniform._descriptorSetLayout);
            allocInfo.pSetLayouts = &descriptorSetLayouts[0];

            for (uint32_t frame = 0; frame < framesCount; frame++)
            {

                VkResult allocationResult = vkAllocateDescriptorSets(context._device, &allocInfo, &uniform._frames[frame]._descriptorSet);
                assert(allocationResult == VK_SUCCESS);
                if (allocationResult != VK_SUCCESS)
                    return false;

                if (uniform._type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
                {
                    VkDescriptorBufferInfo bufferInfo;
                    memset(&bufferInfo, 0, sizeof(bufferInfo));
                    bufferInfo.buffer = uniform._frames[frame]._buffer._buffer;
                    bufferInfo.offset = 0;
                    bufferInfo.range = uniform._frames[frame]._buffer._size;

                    VkWriteDescriptorSet descriptorWrite = {};
                    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    descriptorWrite.dstSet = uniform._frames[frame]._descriptorSet;
                    descriptorWrite.dstBinding = uniform._binding;
                    descriptorWrite.dstArrayElement = 0;
                    descriptorWrite.descriptorType = uniform._type;
                    descriptorWrite.descriptorCount = 1;
                    descriptorWrite.pBufferInfo = &bufferInfo;

                    vkUpdateDescriptorSets(context._device, 1, &descriptorWrite, 0, nullptr);
                }
            }

        }
    }

    return true;
}


/*
void Vulkan::updateUniforms(AppDescriptor & appDesc, Context & context, unsigned int bufferIndex, uint32_t meshIndex)
{
    Mesh & mesh = context._vulkanMeshes[meshIndex];
    
    UniformBufferObject ubo = mesh._transformation;

	const VulkanCamera & cam = context.getCamera();
	ubo._view = glm::lookAt(cam._position, cam._lookat, cam._up);
	ubo._projection = glm::perspective(glm::radians(45.0f), context._swapChainSize.width / (float)context._swapChainSize.height, 0.1f, 1000.0f);

	glm::mat4 concatMat = ubo._projection * ubo._view * ubo._model;
	glm::vec3 result = concatMat * glm::vec4{0,0,0, 1};
    
    // set lights
    ubo._lights[0] = glm::vec4{0,0,0,0};
    ubo._numLights = 1;

    void* data = nullptr;
    const VkResult mapMemoryResult = vkMapMemory(context._device,
                mesh._uniformBuffers[bufferIndex]._memory,
                0,
                sizeof(ubo),
                0, &data);
	assert(mapMemoryResult == VK_SUCCESS);
    
    memcpy(data, &ubo, sizeof(ubo));
    vkUnmapMemory(context._device, context._vulkanMeshes[meshIndex]._uniformBuffers[bufferIndex]._memory);
}
*/

void Vulkan::updateUniforms(AppDescriptor & appDesc, Context & context, uint32_t currentImage)
{
    for(EffectDescriptorPtr & effect : context._effects)
    {
        for (uint32_t stage = 0; stage < static_cast<uint32_t>(Vulkan::ShaderStage::ShaderStageCount); stage++)
        {
            static std::vector<unsigned char> updateData;

            const uint32_t uniformCount = effect->totalTypeCount((Vulkan::ShaderStage)stage, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
            static std::vector<Uniform*> uniforms(1);
            if (uniforms.size() < uniformCount)
                uniforms.resize(2 * (uniformCount + 1)); // amortise resizing

            effect->collectUniformsOfType(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, (Vulkan::ShaderStage)stage, &uniforms[0]);
            for (unsigned int uniformIndex = 0 ; uniformIndex < uniformCount ; uniformIndex++)
            {
                Uniform* uniform = uniforms[uniformIndex];
                unsigned int uniformUpdateSize = effect->_updateUniform(static_cast<Vulkan::ShaderStage>(stage), uniform->_binding, updateData);
                if (uniformUpdateSize != 0)
                    uniform->_frames[context._currentFrame]._buffer.fill(context._device, reinterpret_cast<const void*>(&updateData[0]), uniformUpdateSize);
            }

            effect->_recordCommandBuffers(appDesc, context, *effect);
        }

    }
    
}

void Vulkan::destroyMesh(Context & context, Mesh& mesh)
{
	destroyBufferDescriptor(context, mesh._vertexBuffer);
	destroyBufferDescriptor(context, mesh._indexBuffer);
//	for(auto uniformBuffer : mesh._uniformBuffers)
//		destroyBufferDescriptor(context, uniformBuffer);
//	mesh._uniformBuffers.clear();

//	vkDestroyDescriptorPool(context._device, mesh._descriptorPool, VK_NULL_HANDLE);
//	mesh._descriptorPool = VK_NULL_HANDLE;

//	mesh._descriptorSets.clear();

}

void Vulkan::destroyBufferDescriptor(Context & context, BufferDescriptor& descriptor)
{
	vkFreeMemory(context._device, descriptor._memory, VK_NULL_HANDLE);
	vkDestroyBuffer(context._device, descriptor._buffer, VK_NULL_HANDLE);
}


void Vulkan::clearMeshes(Context & context, EffectDescriptor & effect)
{
	resetCommandBuffers(context, effect._commandBuffers);
	for(auto mesh : effect._meshes)
		destroyMesh(context, *mesh);

    effect._meshes.clear();
}

bool Vulkan::addMesh(AppDescriptor & appDesc, 
    Context & context, 
    std::vector<unsigned char> & vertexData, 
    std::vector<unsigned char> & indexData, 
    void * userData, 
    EffectDescriptor & effectDescriptor, 
    Vulkan::Mesh & result)
{
    Vulkan::Mesh mesh;
	if (!createIndexAndVertexBuffer(appDesc, context, vertexData, indexData, userData, mesh))
	{
		SDL_LogError(0, "Failed to create index and vertex buffer\n");
		return false;
	}


    result = mesh;
    return true;
}


bool Vulkan::handleVulkanSetup(AppDescriptor & appDesc, Context & context)
{
    if (!loadVulkanLibrary()) {
        SDL_LogError(0, "Failed to load vulkan library");
        return false;
    }
    
    if (!loadVulkanFunctions()) {
        SDL_LogError(0, "Failed to load vulkan functions");
        return false;
    }
    
    if (!createInstanceAndLoadExtensions(appDesc, context)) {
        SDL_LogError(0, "Failed to create instance and load extensions");
        return false;
    }
    
    if (validationLayersEnabled && !setupDebugCallback(context))
    {
        SDL_LogError(0, "Failed to setup requested debug callback\n");
        return false;
    }
    
    if (!createVulkanSurface(appDesc._window, context)) {
        SDL_LogError(0, "Failed to create vulkan surface");
        return false;
    }
    
    if (!enumeratePhysicalDevices(appDesc, context)) {
        SDL_LogError(0, "Failed to enumerate and choose device");
        return false;
    }
    
    // choose a gpu
    if (!choosePhysicalDevice(appDesc, context)) {
        SDL_LogError(0, "Failed to choose appropriate physical device");
        return false;
    }
    
    // load extensions
    if (!lookupDeviceExtensions(appDesc))
    {
        SDL_LogError(0, "Failed to enumerate device extensions!");
        return false;
    }
    
    if (!createDevice(appDesc, context))
    {
        SDL_LogError(0, "Failed to create device!");
        return false;
    }
    
    if (!createQueue(appDesc, context))
    {
        SDL_LogError(0, "Failed to create queue!");
        return false;
    }
    
    if (!createSwapChain(appDesc, context))
    {
        SDL_LogError(0, "Failed to create and setup swap chain!");
        return false;
    }
    
    if (!createColorBuffers(context))
    {
        SDL_LogError(0, "Failed to create color buffers\n");
        return false;
    }
    
    if (!createRenderPass(context, &context._renderPass, true))
    {
        SDL_LogError(0, "Failed to create standard render pass\n");
        return false;
    }

	
	if (!createPipelineCache(appDesc, context))
	{
		SDL_LogWarn(0, "Failed to create pipeline cache. This is non-fatal.\n");
	}
	
	// create standard command pool
    if(!createCommandPool(appDesc, context, &context._commandPool))
    {
        SDL_LogError(0, "Failed to create standard command pool\n");
        return false;
    }

    /*
	if (!createDepthBuffers(appDesc, context))
	{
		SDL_LogError(0, "Failed to create depth buffers\n");
		return false;
	}
    */

	if (!createFrameBuffers(context))
	{
		SDL_LogError(0, "Failed to create frame buffers\n");
		return false;
	}

    if(!createSemaphores(appDesc, context))
    {
        SDL_LogError(0, "Failed to create semaphores\n");
        return false;
    }
    
    return true;
}

bool Vulkan::createSwapChainDependents(AppDescriptor & appDesc, Context & context)
{
	if (!createSwapChain(appDesc, context))
	{
		SDL_LogError(0, "Failed to create and setup swap chain!");
		return false;
	}

	if (!createColorBuffers(context))
	{
		SDL_LogError(0, "Failed to create color buffers\n");
		return false;
	}

	if (!createRenderPass(context, &context._renderPass, true))
	{
		SDL_LogError(0, "Failed to create standard render pass\n");
		return false;
	}


	if (!createPipelineCache(appDesc, context))
	{
		SDL_LogWarn(0, "Failed to create pipeline cache. This is non-fatal.\n");
	}

    // create standard command pool
    if(!createCommandPool(appDesc, context, &context._commandPool))
    {
        SDL_LogError(0, "Failed to create standard command pool\n");
        return false;
    }
    
    /*
	if (!createDepthBuffers(appDesc, context))
	{
		SDL_LogError(0, "Failed to create depth buffers\n");
		return false;
	}*/

	if (!createFrameBuffers(context))
	{
		SDL_LogError(0, "Failed to create frame buffers\n");
		return false;
	}

	return true;
}

bool Vulkan::recreateSwapChain(AppDescriptor & appDesc, Context & context)
{
	vkDeviceWaitIdle(context._device);

	if (!cleanupSwapChain(appDesc, context))
		return false;

	if (!createSwapChainDependents(appDesc, context))
		return false;

    context._currentFrame = 0;
	return true;
}

bool Vulkan::cleanupSwapChain(AppDescriptor & appDesc, Context & context)
{
	VkDevice device = context._device;

	for (unsigned int i = 0; i < (unsigned int)context._fences.size(); i++)
	{
		if (!context._fences.empty())
		{
			const VkResult waitForFencesResult = vkWaitForFences(context._device, 1, &context._fences[i], VK_TRUE, std::numeric_limits<uint64_t>::max());
			assert(waitForFencesResult == VK_SUCCESS);
        }
	}

	for (auto depthImageView : context._depthImageViews)
		vkDestroyImageView(device, depthImageView, nullptr);
	context._depthImageViews.clear();

	for (auto depthImage : context._depthImages)
		vkDestroyImage(device, depthImage, nullptr);
	context._depthImages.clear();

	for (auto depthMemory : context._depthMemory)
		vkFreeMemory(device, depthMemory, nullptr);
	context._depthMemory.clear();

	for (auto frameBuffer : context._frameBuffers)
		vkDestroyFramebuffer(device, frameBuffer, nullptr);
	context._frameBuffers.clear();
/*
	vkFreeCommandBuffers(device, context._commandPool, (uint32_t)context._commandBuffers.size(), &context._commandBuffers[0]);
	vkDestroyCommandPool(device, context._commandPool, VK_NULL_HANDLE);
	context._commandBuffers.clear();
	vkDestroyRenderPass(device, context._renderPass, VK_NULL_HANDLE);
	context._renderPass = VK_NULL_HANDLE;

	vkDestroyPipeline(device, context._pipeline, nullptr);
	context._pipeline = VK_NULL_HANDLE;
	vkDestroyPipelineLayout(device, context._pipelineLayout, nullptr);
	context._pipelineLayout = VK_NULL_HANDLE;
	vkDestroyPipelineCache(device, context._pipelineCache, VK_NULL_HANDLE);
	context._pipelineCache = VK_NULL_HANDLE;
    */
	for (auto imageView : context._colorBuffers)
		vkDestroyImageView(device, imageView, nullptr);
	context._colorBuffers.clear();

/*	for (auto image : context._rawImages)
		vkDestroyImage(device, image, nullptr);*/
	context._rawImages.clear();

//	vkDestroyDescriptorSetLayout(device, context._descriptorSetLayout, VK_NULL_HANDLE);
//	context._descriptorSetLayout = VK_NULL_HANDLE;

	vkDestroySwapchainKHR(device, context._swapChain, nullptr);
	context._swapChain = VK_NULL_HANDLE;



	return true;
}

bool Vulkan::initEffectDescriptor(AppDescriptor& appDesc, Context& context, Vulkan::EffectDescriptor& effect)
{
    // create uniform buffers
    for (unsigned int stage = 0; stage < static_cast<unsigned int> (Vulkan::ShaderStage::ShaderStageCount); stage++)
    {
        const uint32_t uniformCount = effect.totalTypeCount(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        static std::vector<Uniform*> uniforms(1);
        if (uniforms.size() < uniformCount)
            uniforms.resize(2 * (uniformCount + 1)); // amortise resizing

        effect.collectUniformsOfType(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &uniforms[0]);
        for (unsigned int uniformIndex = 0; uniformIndex < uniformCount; uniformIndex++)
        {
            Uniform* uniform = uniforms[uniformIndex];
            for (unsigned int frame = 0; frame < context._rawImages.size(); frame++)
            {
                Vulkan::BufferDescriptor& buffer = uniform->_frames[frame]._buffer;
                if (!Vulkan::createUniformBuffer(appDesc, context, buffer._size, buffer))
                {
                    SDL_LogError(0, "Failed to create uniform buffer\n");
                    return false;
                }

            }
        }

    }

    if (!createDescriptorSetLayout(context, effect))
    {
        SDL_LogError(0, "Failed to create descriptor set layouts!");
        return false;
    }

    if (!createDescriptorPool(context, effect))
    {
        SDL_LogError(0, "Failed to create descriptor pool\n");
        return false;
    }

    if (!createDescriptorSet(appDesc, context, effect))
    {
        SDL_LogError(0, "Failed to create descriptor set\n");
        return false;
    }


    if (!createCommandBuffers(context, context._commandPool, (unsigned int)context._rawImages.size(), &effect._commandBuffers))
    {
        SDL_LogError(0, "Failed to create command buffers\n");
        return false;
    }

    if (!effect._shaderModules.empty())
    {
        bool createShaderModulesSuccess = Vulkan::createShaderModules(appDesc, context, effect._shaderModules);
        if (!createShaderModulesSuccess)
        {
            SDL_LogError(0, "Failed to create shader modules\n");
            return 1;
        }
    }

    return true;
}

bool Vulkan::initEffectDescriptor(AppDescriptor& appDesc, Context& context, ComputePipelineCustomizationCallback computePipelineCreationCallback, Vulkan::EffectDescriptor& effect)
{
    if (!initEffectDescriptor(appDesc, context, effect))
    {
        SDL_LogError(0, "Failed to create pipeline\n");
        return false;
    }

    if (!createComputePipeline(appDesc, context,  computePipelineCreationCallback, effect))
    {
        SDL_LogError(0, "Failed to create compute pipeline\n");
        return false;
    }

    return true;
}

bool Vulkan::initEffectDescriptor(AppDescriptor& appDesc, Context& context, GraphicsPipelineCustomizationCallback graphicsPipelineCreationCallback, Vulkan::EffectDescriptor& effect)
{
    if (!initEffectDescriptor(appDesc, context, effect))
    {
        SDL_LogError(0, "Failed to create pipeline\n");
        return false;
    }

    if(!createGraphicsPipeline(appDesc, context, graphicsPipelineCreationCallback, effect))
    {
        SDL_LogError(0, "Failed to create graphics pipeline\n");
        return false;
    }

    return true;
}
