
#include "VulkanSetup.h"

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

#include <map>

////////////////////////////////////// Vulkan method declarations ///////////////////////////////////////////////////////

namespace
{
    Vulkan::Logger * g_logger = new Vulkan::Logger();
    constexpr unsigned int stagingBufferSize = 42 * 1024 * 1024;
    constexpr unsigned int uniformBufferSize = 1 * 1024 * 1024;
}

namespace Vulkan
{
    void clearMeshes(Context& context, EffectDescriptor& effect);
    void destroyMesh(Context& context, Mesh& mesh); 
    void destroyBufferDescriptor(BufferDescriptor& descriptor);
    void destroyImage(ImageDescriptor& descriptor);

    bool setupDebugCallback(Context& context);
    void areValidationLayersAvailable(const std::vector<const char*>& validationLayers, std::vector<const char*> & output);
    bool createInstanceAndLoadExtensions(AppDescriptor& appDesc, Context& context);
    bool createVulkanSurface(SDL_Window* window, Context& context);
    bool enumeratePhysicalDevices(AppDescriptor& appDesc, Context& context);
    bool choosePhysicalDevice(AppDescriptor& appDesc, Context& Context);
    bool lookupDeviceExtensions(AppDescriptor& appDesc);
    bool createDevice(AppDescriptor& appDesc, Context& context);
    bool createSwapChain(AppDescriptor& appDesc, Context& context);
    bool createColorBuffers(Context& context);
    bool createDescriptorSetLayout(Context& Context, EffectDescriptor& effect);
    bool createPipelineCache(AppDescriptor& appDesc, Context& context);
    bool createCommandPools(Context& context);
    bool recordStandardCommandBuffers(AppDescriptor& appDesc, Context& context);
    std::vector<VkSemaphore> createSemaphores(Context& context);
    bool createSemaphores(AppDescriptor& appDesc, Context& context);
    void destroySemaphores(Context& context);
    bool createDescriptorPool(Context& context, EffectDescriptor& effect);
    bool createDescriptorSet(AppDescriptor& appDesc, Context& context, EffectDescriptor& effect);

    void updateUniforms(AppDescriptor& appDesc, Context& context, unsigned int bufferIndex, uint32_t meshIndex);
    bool createSwapChainDependents(AppDescriptor& appDesc, Context& context);
    bool cleanupSwapChain(AppDescriptor& appDesc, Context& context);
    bool initEffectDescriptor(AppDescriptor& appDesc, Context& context, unsigned int queueFlagBits, Vulkan::EffectDescriptor& effect);
    bool setupAllocator(AppDescriptor& appDesc, Context& context);

    bool createGraphicsPipeline(AppDescriptor& appDesc, Context& context, GraphicsPipelineCustomizationCallback graphicsPipelineCreationCallback, Vulkan::EffectDescriptor& effect);
    bool createComputePipeline(AppDescriptor& appDesc, Context& context, ComputePipelineCustomizationCallback computePipelineCreationCallback, Vulkan::EffectDescriptor& effect);
    bool createBuffer(Context& context, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, BufferDescriptor& bufDesc, VmaAllocationInfo* aInfo = nullptr);

    static VmaAllocator g_allocator;
}

///////////////////////////////////// Vulkan Variable ///////////////////////////////////////////////////////////////////

bool Vulkan::validationLayersEnabled = false;

///////////////////////////////////// Vulkan Helper Function ////////////////////////////////////////////////////////////

namespace Vulkan
{
    constexpr unsigned int MAX_STAGING_BUFFER_SIZE = 2048 * 2048 * 4;

    Vulkan::PersistentBufferPtr getPersistentStagingBuffer(Vulkan::Context& context, uint32_t size)
    {
        size = std::min(size, stagingBufferSize); // make sure the staging buffer has a minimum size
        static Vulkan::PersistentBufferPtr stagingBuffer;
        if (stagingBuffer == nullptr || (stagingBuffer->_registeredSize < size && stagingBuffer->_registeredSize <= MAX_STAGING_BUFFER_SIZE))
        {
            stagingBuffer = nullptr; // release memory
            unsigned int newSize = std::min<unsigned int>(size, MAX_STAGING_BUFFER_SIZE);
            stagingBuffer = Vulkan::createPersistentBuffer(context, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, "StagingBuffer", 1);
            assert(stagingBuffer != nullptr);

        }
        return stagingBuffer;
    }


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
}

namespace
{
    bool createCommandBuffer(Vulkan::Context& context, VkCommandPool commandPool,  VkCommandBuffer * result)
    {
        VkCommandBufferAllocateInfo commandBufferAllocateInfo;
        memset(&commandBufferAllocateInfo, 0, sizeof(VkCommandBufferAllocateInfo));
        commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        commandBufferAllocateInfo.commandPool = commandPool;
        commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        commandBufferAllocateInfo.commandBufferCount = 1;
        const VkResult allocateCommandBuffersResult = vkAllocateCommandBuffers(context._device, &commandBufferAllocateInfo, result);
        assert(allocateCommandBuffersResult == VK_SUCCESS);
        if (allocateCommandBuffersResult != VK_SUCCESS)
        {
            g_logger->log(Vulkan::Logger::Level::Error, std::string("Failed to create VkCommandbufferAllocateInfo\n"));
            return false;
        }

        return true;
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
            g_logger->log(Vulkan::Logger::Level::Error, std::string("Failed to create VkCommandbufferAllocateInfo\n"));
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

        const VkResult allocationMemoryResult = vkAllocateMemory(context._device, &allocInfo, nullptr, &result);
        if (allocationMemoryResult != VK_SUCCESS)
            return false;

        vkBindImageMemory(context._device, image, result, 0);
        return true;
    }


}


///////////////////////////////////// Vulkan Context ///////////////////////////////////////////////////////////////////

Vulkan::Context::Context()
    :_currentFrame(0)
    , _swapChain(nullptr)
    , _pipelineCache(VK_NULL_HANDLE)
    , _allocator(nullptr)
    , _debugReportCallback(VK_NULL_HANDLE)
    , _debugUtilsCallback(VK_NULL_HANDLE)
    , _numInflightFrames(0)
{

}

///////////////////////////////////// Vulkan Effect Descriptor ///////////////////////////////////////////////////////////////////

namespace
{
    uint32_t numUniforms(const Vulkan::EffectDescriptor & effect)
    {
        return (uint32_t)effect._uniforms.size();
    }

    Vulkan::Uniform* findUniform(Vulkan::EffectDescriptor& effect, Vulkan::ShaderStage stage, uint32_t binding)
    {
        for (Vulkan::Uniform & uniform : effect._uniforms)
        {
            if (uniform._binding == binding)
            {
                for (Vulkan::ShaderStage existingStage : uniform._stages)
                {
                    if (existingStage == stage)
                        return &uniform;
                }
            }

        }

        return nullptr;
    }
}


void Vulkan::EffectDescriptor::collectDescriptorSetLayouts(std::vector<VkDescriptorSetLayout>& layouts)
{
    if (_descriptorSetLayout != VK_NULL_HANDLE)
        layouts.push_back(_descriptorSetLayout);
}

uint32_t Vulkan::EffectDescriptor::collectUniformsOfType(VkDescriptorType type, Vulkan::ShaderStage stage, Uniform** result)
{
    uint32_t count = 0;
    for (int j = 0; j < (int)_uniforms.size(); j++)
    {
        for (Vulkan::ShaderStage shaderStage : _uniforms[j]._stages)
        {
            if (shaderStage == stage)
            {
                result[count++] = &_uniforms[j];
            }
        }
    }

    return count;
}


uint32_t Vulkan::EffectDescriptor::collectUniformsOfType(VkDescriptorType type, Uniform ** result)
{
    uint32_t count = 0;
    for (int j = 0; j < (int)_uniforms.size(); j++)
    {
        if (_uniforms[j]._type == type)
            result[count++] = &_uniforms[j];
    }

    return count;
}

uint32_t Vulkan::EffectDescriptor::totalTypeCount(VkDescriptorType type) const
{
    uint32_t count = 0;
    for (int j = 0; j < (int)_uniforms.size(); j++)
    {
        if (_uniforms[j]._type == type)
            count++;
    }

    return count;
}

uint32_t Vulkan::EffectDescriptor::totalTypeCount(Vulkan::ShaderStage stage, VkDescriptorType type) const
{
    uint32_t count = 0;
    for (int j = 0; j < (int)_uniforms.size(); j++)
    {
        for (Vulkan::ShaderStage shaderStage : _uniforms[j]._stages)
        {
            if (shaderStage == stage)
            {
                count++;
            }
        }
    }

    return count;
}


uint32_t Vulkan::EffectDescriptor::totalTexelBufferCount() const
{
    return totalTypeCount(VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER);
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

Vulkan::Uniform* Vulkan::EffectDescriptor::getUniformWithBinding(int binding)
{
    for (Vulkan::Uniform& uniform : _uniforms)
    {
        if (uniform._binding == binding)
            return &uniform;
    }
    return nullptr;
}

uint32_t Vulkan::EffectDescriptor::addUniformSamplerOrImage(Vulkan::Context& context, Vulkan::ShaderStage stage, const std::string& name, VkDescriptorType type, int binding)
{
    Vulkan::Uniform* uniform = getUniformWithBinding(binding);
    if (uniform != nullptr)
    {
        assert(uniform->_name == name);
        assert(uniform->_binding == binding);
	assert(uniform->_type == type);
        uniform->_stages.push_back(stage);
        return binding;
    }
    else
    {
        Vulkan::Uniform newUniform{};
        newUniform._name = name;
        newUniform._type = type;
        newUniform._size = 0;
        newUniform._frames.resize(Vulkan::getNumInflightFrames(context));
        newUniform._stages.push_back(stage);

        if (binding < 0)
            newUniform._binding = numUniforms(*this);
        else
            newUniform._binding = binding;

        _uniforms.push_back(newUniform);
        return newUniform._binding;
    }
}


uint32_t Vulkan::EffectDescriptor::addUniformSampler(Vulkan::Context& context, Vulkan::ShaderStage stage, const std::string& name, int binding)
{
    return addUniformSamplerOrImage(context, stage, name, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, binding);
}

uint32_t Vulkan::EffectDescriptor::addUniformImage(Vulkan::Context& context, Vulkan::ShaderStage stage, const std::string& name, int binding)
{
    return addUniformSamplerOrImage(context, stage, name, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, binding);
}

uint32_t Vulkan::EffectDescriptor::addUniformBuffer(Vulkan::Context& context, Vulkan::ShaderStage stage, const std::string& name, uint32_t size, int binding)
{
    Vulkan::Uniform* uniform = getUniformWithBinding(binding);
    if (uniform != nullptr)
    {
        assert(uniform->_name == name);
        assert(uniform->_binding == binding);
	assert(uniform->_type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        uniform->_stages.push_back(stage);
        return binding;
    }
    else
    {
        Vulkan::Uniform newUniform{};
        newUniform._name = name;
        newUniform._type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        newUniform._stages.push_back(stage);
	assert(size > 0);
        newUniform._size = size;

        if (binding < 0)
            newUniform._binding = numUniforms(*this);
        else
            newUniform._binding = binding;

        for (uint32_t i = 0; i < (uint32_t)Vulkan::getNumInflightFrames(context); i++)
        {
            std::string tag = std::string("UniformBuffer") + std::to_string(i);
            UniformAggregate aggregate;
            Vulkan::PersistentBufferPtr buffer = Vulkan::lookupPersistentBuffer(context, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, tag, 1);
            if (buffer == nullptr)
                buffer = Vulkan::createPersistentBuffer(context, uniformBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, tag, 1);
            aggregate._buffer = buffer;
            newUniform._frames.push_back(aggregate);
        }
        _uniforms.push_back(newUniform);

        return newUniform._binding;
    }
}

bool Vulkan::EffectDescriptor::bindTexelBuffer(Vulkan::Context& context, Vulkan::ShaderStage shaderStage, uint32_t binding, VkBufferView bufferView, VkBuffer buffer, unsigned int offset, unsigned int range)
{
    Uniform* uniform = findUniform(*this, shaderStage, binding);
    assert(uniform != nullptr);
    if (uniform == nullptr)
        return false;

    assert(uniform->_type == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER);
    if (uniform->_type != VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER)
        return false;

    unsigned int frame = context._currentFrame;

    {
        UniformAggregate& aggregate = uniform->_frames[frame];
        aggregate._bufferView = bufferView;
        assert(bufferView != nullptr);

        VkDescriptorBufferInfo texelBufferInfo = {};
        texelBufferInfo.buffer = buffer;
        texelBufferInfo.offset = offset;
        texelBufferInfo.range = range;

        VkWriteDescriptorSet writeSet = { };
        writeSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeSet.descriptorCount = 1;
        writeSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
        writeSet.dstArrayElement = 0;
        writeSet.dstBinding = binding;
        writeSet.dstSet = _descriptorSets[frame];
        writeSet.pBufferInfo = &texelBufferInfo;
        writeSet.pImageInfo = VK_NULL_HANDLE;
        writeSet.pNext = VK_NULL_HANDLE;
        writeSet.pTexelBufferView = &bufferView;

        vkUpdateDescriptorSets(context._device, 1, &writeSet, 0, nullptr);
    }

    return true;
}


bool Vulkan::EffectDescriptor::bindImage(Vulkan::Context& context, Vulkan::ShaderStage shaderStage, uint32_t binding, VkImageView imageView, VkImageLayout layout)
{
    Uniform* uniform = findUniform(*this, shaderStage, binding);
    assert(uniform != nullptr);
    if (uniform == nullptr)
        return false;

    assert(uniform->_type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    if (uniform->_type != VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
        return false;

    const unsigned int currentFrame = context._currentFrame;
    UniformAggregate& aggregate = uniform->_frames[currentFrame];
    aggregate._imageView = imageView;

    VkDescriptorImageInfo imageInfo;

    imageInfo.imageLayout = layout;
    imageInfo.imageView = imageView;
    imageInfo.sampler = VK_NULL_HANDLE;

    VkWriteDescriptorSet writeSet = { };
    writeSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeSet.descriptorCount = 1;
    writeSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    writeSet.dstArrayElement = 0;
    writeSet.dstBinding = binding;
    writeSet.dstSet = _descriptorSets[currentFrame];
    writeSet.pBufferInfo = VK_NULL_HANDLE;
    writeSet.pImageInfo = &imageInfo;
    writeSet.pNext = VK_NULL_HANDLE;
    writeSet.pTexelBufferView = VK_NULL_HANDLE;

    vkUpdateDescriptorSets(context._device, 1, &writeSet, 0, nullptr);


    return true;
}

bool Vulkan::EffectDescriptor::bindSampler(Vulkan::Context & context, Vulkan::ShaderStage shaderStage, uint32_t binding, VkImageView imageView, VkImageLayout layout, VkSampler sampler)
{
    Uniform* uniform = findUniform(*this, shaderStage, binding);
    assert(uniform != nullptr);
    if (uniform == nullptr)
        return false;

    assert(uniform->_type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    if (uniform->_type != VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
        return false;

    UniformAggregate& aggregate = uniform->_frames[context._currentFrame];
    aggregate._imageView = imageView;
    aggregate._sampler = sampler;

    VkDescriptorImageInfo imageInfo;

    imageInfo.imageLayout = layout;
    imageInfo.imageView = imageView;
    imageInfo.sampler = sampler;

    VkWriteDescriptorSet writeSet = { };
    writeSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeSet.descriptorCount = 1;
    writeSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writeSet.dstArrayElement = 0;
    writeSet.dstBinding = binding;
    writeSet.dstSet = _descriptorSets[context._currentFrame];
    writeSet.pBufferInfo = VK_NULL_HANDLE;
    writeSet.pImageInfo = &imageInfo;
    writeSet.pNext = VK_NULL_HANDLE;
    writeSet.pTexelBufferView = VK_NULL_HANDLE;

    vkUpdateDescriptorSets(context._device, 1, &writeSet, 0, nullptr);

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
    , _enableVSync(true)
    , _requestedNumSamples(1)
    , _actualNumSamples(1)
    , _drawableSurfaceWidth(0)
    , _drawableSurfaceHeight(0)
    , _hasPreferredSurfaceFormat(false)
{
}

void Vulkan::AppDescriptor::addRequiredInstanceExtensions(const std::vector<std::string>& extensions)
{
    for (const std::string& extension : extensions)
        addRequiredInstanceExtension(extension);
}

void Vulkan::AppDescriptor::addRequiredDeviceExtensions(const std::vector<std::string>& extensions)
{
    for (const std::string& extension : extensions)
        addRequiredDeviceExtension(extension);
}

void Vulkan::AppDescriptor::addRequiredInstanceExtension(const std::string& extension)
{
    if (std::find(_requiredInstanceExtensions.begin(), _requiredInstanceExtensions.end(), extension) == _requiredInstanceExtensions.end())
        _requiredInstanceExtensions.push_back(extension);
}

void Vulkan::AppDescriptor::addRequiredDeviceExtension(const std::string& extension)
{
    if (std::find(_requiredDeviceExtensions.begin(), _requiredDeviceExtensions.end(), extension) == _requiredDeviceExtensions.end())
        _requiredDeviceExtensions.push_back(extension);
}


///////////////////////////////////// Vulkan BufferDescriptor ///////////////////////////////////////////////////////////////////

namespace
{
    constexpr unsigned int g_persistentBufferSize = 32 * 1024 * 1024;

    typedef std::tuple<unsigned int, VkBufferUsageFlags, VkMemoryPropertyFlags, std::string> PersistentBufferKey;
    typedef std::map<PersistentBufferKey,Vulkan::PersistentBufferPtr> PersistentBufferMap;
    PersistentBufferMap g_persistentBuffers;

}

void Vulkan::BufferDescriptor::destroy()
{
    if(_buffer!=VK_NULL_HANDLE && _memory!=VK_NULL_HANDLE)
       vmaDestroyBuffer(g_allocator, _buffer, _memory);
    _buffer = VK_NULL_HANDLE;
    _memory = VK_NULL_HANDLE;
}




bool Vulkan::BufferDescriptor::copyFrom(Vulkan::Context & context, VkCommandPool commandPool, VkQueue queue, const void * srcData, VkDeviceSize amount, VkDeviceSize dstOffset)
{
    if (_mappable)
    {
        void* data = nullptr;
        const VkResult mapResult = vmaMapMemory(g_allocator, _memory, &data);
        assert(mapResult == VK_SUCCESS);
        if (mapResult != VK_SUCCESS)
        {
            g_logger->log(Vulkan::Logger::Level::Error, std::string("Failed to map vertex buffer memory\n"));
            return false;
        }

        unsigned char* dstData = reinterpret_cast<unsigned char*>(data);
        void* fPointer = reinterpret_cast<void*>(dstData + dstOffset);
        memcpy(data, srcData, amount);
        vmaUnmapMemory(g_allocator, _memory);
    }
    else
    {
//         use a staging buffer to copy data
        int64_t amountOfDataLeftToCopy = (int64_t)amount;
        int64_t currentDstOffset = (int64_t)dstOffset;
        int64_t srcOffset = 0;
        while (amountOfDataLeftToCopy > 0)
        {
            // TODO - make this copy async, by creating a temporary buffer and then copy across, store a little struct with
            // the temp staing buffer, the commadnbuffer and fence. Then each frame, check for completed buffers.
            Vulkan::PersistentBufferPtr stagingBuffer = Vulkan::getPersistentStagingBuffer(context, amount);
            const int64_t amountToCopy = std::min(amountOfDataLeftToCopy, (int64_t)stagingBuffer->_registeredSize);

            const char* cData = reinterpret_cast<const char*>(srcData)  + srcOffset;
            stagingBuffer->copyFromAndFlush(context, 0, cData, amountToCopy, 0);
            copyFromAndFlush(context, commandPool, queue, stagingBuffer->_buffers[0], amountToCopy, 0, currentDstOffset);
            currentDstOffset += amountToCopy;
            amountOfDataLeftToCopy -= amountToCopy;
            srcOffset += amountToCopy;
        }

    }


    return true;
}

bool Vulkan::BufferDescriptor::copyFromAndFlush(Vulkan::Context& context, VkCommandPool commandPool, VkQueue queue, BufferDescriptor & src, VkDeviceSize amount, VkDeviceSize srcOffset, VkDeviceSize dstOffset)
{
    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;
    
    VkCommandBuffer commandBuffer;
    const VkResult allocationResult = vkAllocateCommandBuffers(context._device, &allocInfo, &commandBuffer);
	assert(allocationResult == VK_SUCCESS);
    
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    
    const VkResult beginCommandBufferResult = vkBeginCommandBuffer(commandBuffer, &beginInfo);
	assert(beginCommandBufferResult == VK_SUCCESS);

    VkBufferCopy copyRegion = {};
    copyRegion.srcOffset = srcOffset;
    copyRegion.dstOffset = dstOffset;
    copyRegion.size = amount;
    vkCmdCopyBuffer(commandBuffer, src._buffer, _buffer, 1, &copyRegion);
    
	const VkResult endCommandBufferResult = vkEndCommandBuffer(commandBuffer);
    assert(endCommandBufferResult == VK_SUCCESS);

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    
    VkFence fence = Vulkan::createFence(context._device, 0);
    assert(fence != VK_NULL_HANDLE);

    const VkResult submitResult = vkQueueSubmit(queue, 1, &submitInfo, fence);
	assert(submitResult == VK_SUCCESS);

    const VkResult waitForFencesResult = vkWaitForFences(context._device, 1, &fence, VK_TRUE, std::numeric_limits<uint64_t>::max());
    vkDestroyFence(context._device, fence, nullptr);

    vkFreeCommandBuffers(context._device, commandPool, 1, &commandBuffer);
    return true;
}

bool Vulkan::BufferDescriptor::copyToAndFlush(
    VkDevice device,
    VkCommandPool commandPool,
    VkQueue queue,
    VkImage image,
    VkOffset3D offset,
    VkExtent3D extent)
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
    region.imageOffset = offset;
    region.imageExtent = extent;

    vkCmdCopyBufferToImage(commandBuffer, _buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    const VkResult endCommandBufferResult = vkEndCommandBuffer(commandBuffer);
    assert(endCommandBufferResult == VK_SUCCESS);

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    VkFence fence = Vulkan::createFence(device, 0);
    assert(fence != VK_NULL_HANDLE);

    const VkResult submitResult = vkQueueSubmit(queue, 1, &submitInfo, fence);
    assert(submitResult == VK_SUCCESS);

    const VkResult waitForFencesResult = vkWaitForFences(device, 1, &fence, VK_TRUE, std::numeric_limits<uint64_t>::max());
    vkDestroyFence(device, fence, nullptr);

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
    return true;
}

///////////////////////////////////// Vulkan PersistentBuffer ///////////////////////////////////////////////////////////////////

void Vulkan::PersistentBuffer::destroy()
{
    for (auto& buf : _buffers)
        buf.destroy();

    _buffers.clear();
}

bool Vulkan::PersistentBuffer::copyFrom(unsigned int frameIndex, const void* srcData, VkDeviceSize amount, VkDeviceSize offset)
{
    frameIndex = frameIndex % _offsets.size();
    const unsigned int lOffset =  (UINT64_MAX==offset) ? _offsets[frameIndex] : (unsigned int)offset;
    assert(lOffset + (VkDeviceSize)amount <= this->_registeredSize);
    if (lOffset + (VkDeviceSize)amount > _buffers[frameIndex]._size)
        return false;

    unsigned char* dstData = reinterpret_cast<unsigned char*>(_allocInfos[frameIndex].pMappedData);
    // data is mapped - just copy it
    void* fPointer = reinterpret_cast<void*>(dstData + lOffset);
    memcpy(fPointer, srcData, amount);
    _offsets[frameIndex] = lOffset + (unsigned int)amount;

    return true;
}

bool Vulkan::PersistentBuffer::copyFromAndFlush(Vulkan::Context& context, unsigned int frameIndex, const void* srcData, VkDeviceSize amount, VkDeviceSize dstOffset)
{

    const bool successfullyCopied = copyFrom(frameIndex, srcData, amount, dstOffset);
    assert(successfullyCopied);
    const bool successfullyFlushed = flushData(context, frameIndex);
    assert(successfullyFlushed);
    const bool result = successfullyCopied && successfullyFlushed;
    assert(result);
    return result;
}


bool Vulkan::PersistentBuffer::startFrame(unsigned int frameIndex)
{
    for (std::pair<const PersistentBufferKey, PersistentBufferPtr>& keyValue : g_persistentBuffers)
    {
        keyValue.second->_offsets[frameIndex % (unsigned int)keyValue.second->_offsets.size()] = 0;
    }

    return true;
}

bool Vulkan::PersistentBuffer::flushData(Vulkan::Context& context, unsigned int frameIndex)
{
    VkDeviceSize size = _offsets[frameIndex % _offsets.size()];
    size = size + size % context._deviceProperties.limits.nonCoherentAtomSize;
    size = (size > _registeredSize) ? VK_WHOLE_SIZE : size;

    VkResult success = vmaFlushAllocation(g_allocator,
        _buffers[frameIndex % _buffers.size()]._memory,
        0,
        size);
    assert(success == VK_SUCCESS);
    return success == VK_SUCCESS;
}


bool Vulkan::PersistentBuffer::submitFrame(Vulkan::Context& context, unsigned int frameIndex)
{
    for (std::pair<const PersistentBufferKey, PersistentBufferPtr>& keyValue : g_persistentBuffers)
        keyValue.second->flushData(context, frameIndex);
    return true;
}

///////////////////////////////////// Vulkan Methods ///////////////////////////////////////////////////////////////////

void Vulkan::ImageDescriptor::destroy()
{
    if (_image != VK_NULL_HANDLE)
        Vulkan::destroyImage(*this);

    _image = VK_NULL_HANDLE;
    _size = 0;
}

void* Vulkan::ImageDescriptor::map()
{
    assert(_mappedData == nullptr);
    VkResult result = vmaMapMemory(g_allocator, _memory, &_mappedData);
    assert(result == VK_SUCCESS);
    return _mappedData;
}

void Vulkan::ImageDescriptor::unmap()
{
    assert(_mappedData != nullptr);
    if (_mappedData != nullptr)
        vmaUnmapMemory(g_allocator, _memory);
    _mappedData = nullptr;
}



///////////////////////////////////// Image Descriptor ///////////////////////////////////////////////////////////////////

namespace
{
	VKAPI_ATTR VkBool32 VKAPI_CALL VulkanDebugUtilsCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
		VkDebugUtilsMessageTypeFlagsEXT type,
		const VkDebugUtilsMessengerCallbackDataEXT * callbackData,
		void * userData)
	{
        if (type == VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT || type == VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT)
        {
            switch (severity)
            {
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
                g_logger->log(Vulkan::Logger::Level::Info, std::string(callbackData->pMessage));
                break;
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
                g_logger->log(Vulkan::Logger::Level::Warn, std::string(callbackData->pMessage));
                break;
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
                g_logger->log(Vulkan::Logger::Level::Verbose, std::string(callbackData->pMessage));
                break;
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
                g_logger->log(Vulkan::Logger::Level::Debug, std::string(callbackData->pMessage));
                break;
            default:
                g_logger->log(Vulkan::Logger::Level::Info, std::string(callbackData->pMessage));
                break;
            }
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

        switch (flags)
        {
        case VK_DEBUG_REPORT_INFORMATION_BIT_EXT:
            g_logger->log(Vulkan::Logger::Level::Debug, std::string("VulkanDebugReportCallback[INFO]: ") + std::string(pLayerPrefix) + std::string("-") + std::string(pMessage));
            break;
        case VK_DEBUG_REPORT_WARNING_BIT_EXT:
            g_logger->log(Vulkan::Logger::Level::Debug, std::string("VulkanDebugReportCallback[WARN]: ") + std::string(pLayerPrefix) + std::string("-") + std::string(pMessage));
            break;
        case VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT:
            g_logger->log(Vulkan::Logger::Level::Debug, std::string("VulkanDebugReportCallback[PERFORMANCE]: ") + std::string(pLayerPrefix) + std::string("-") + std::string(pMessage));
            break;
        case VK_DEBUG_REPORT_ERROR_BIT_EXT:
            g_logger->log(Vulkan::Logger::Level::Debug, std::string("VulkanDebugReportCallback[ERROR]: ") + std::string(pLayerPrefix) + std::string("-") + std::string(pMessage));
            break;
        case VK_DEBUG_REPORT_DEBUG_BIT_EXT:
            g_logger->log(Vulkan::Logger::Level::Debug, std::string("VulkanDebugReportCallback[DEBUG]: ") + std::string(pLayerPrefix) + std::string("-") + std::string(pMessage));
            break;
        }

		return VK_TRUE;
	}
}

bool Vulkan::allocateAndBindImageMemory(Vulkan::Context& context, VkImage& image, VkDeviceMemory & memory, VkMemoryPropertyFlags memoryProperties)
{
    VkMemoryRequirements imageMemoryRequirements;
    vkGetImageMemoryRequirements(context._device, image, &imageMemoryRequirements);

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = imageMemoryRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(context, imageMemoryRequirements.memoryTypeBits, memoryProperties);

    if (vkAllocateMemory(context._device, &allocInfo, nullptr, &memory) != VK_SUCCESS) {
        return false;
    }

    if (vkBindImageMemory(context._device, image, memory, 0) != VK_SUCCESS) {
        return false;
    }
    return true;
}

bool Vulkan::createImage(Vulkan::Context & context,
    unsigned int width,
    unsigned int height,
    unsigned int depth,
    unsigned int mipMapLevels,
    const unsigned int samplesPrPixels,
    VkFormat requiredFormat,
    VkImageTiling requiredTiling,
    VkImageUsageFlags requiredUsage,
    VkMemoryPropertyFlags memoryProperties,
    Vulkan::ImageDescriptor & resultImage)
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
    createInfo.mipLevels = mipMapLevels;
    createInfo.arrayLayers = 1;
    createInfo.samples = (VkSampleCountFlagBits)samplesPrPixels;
    createInfo.tiling = requiredTiling;
    createInfo.usage = requiredUsage;
    createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.queueFamilyIndexCount = 0;
    createInfo.pQueueFamilyIndices = NULL;
    createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo allocCreateInfo = {};
    allocCreateInfo.flags = VMA_ALLOCATION_CREATE_STRATEGY_MIN_MEMORY_BIT;
    allocCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    allocCreateInfo.requiredFlags = memoryProperties;
    allocCreateInfo.preferredFlags = memoryProperties;

    VmaAllocationInfo allocInfo = {};
    VmaAllocation allocation;

    VkImage image = VK_NULL_HANDLE;
    const VkResult result = vmaCreateImage(g_allocator, &createInfo, &allocCreateInfo, &image, &allocation, &allocInfo);
    if (result != VK_SUCCESS)
        return false;

    assert(image != nullptr);
    if (image == nullptr) {
        int k = 0;
        k = 1;
    }
    resultImage._image = image;
    resultImage._memory = allocation;

    return true;
}

bool Vulkan::updataImageData(Vulkan::Context& context, Vulkan::ImageDescriptor& result, const void* pixels, unsigned int mipMapLevels, const unsigned int& pixelSize, const unsigned int& width, const unsigned int& height, const unsigned int& depth, VkImageLayout finalLayout)
{
    if (!Vulkan::transitionImageLayoutAndSubmit(context, result._image,
        finalLayout,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL))
    {
        g_logger->log(Vulkan::Logger::Level::Error, std::string("createImage - transitionImageLayout : VK_IMAGE_LAYOUT_UNDEFINED -> VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL\n"));
        return false;
    }

    if (pixels != nullptr)
    {
        assert(mipMapLevels > 0);
        VkDeviceSize size = pixelSize * width * height * depth;
        Vulkan::PersistentBufferPtr stagingBuffer = getPersistentStagingBuffer(context, size);
        Context::Queue& queue = getQueue(context, VK_QUEUE_TRANSFER_BIT, { 8,8,1 });
        for (int32_t depthLevel = 0; depthLevel < (int32_t)depth; depthLevel = depthLevel + queue._minGranularity.depth)
        {
            VkDeviceSize amountToCopy = pixelSize * width * height * queue._minGranularity.depth;
            assert(amountToCopy <= stagingBuffer->_registeredSize);

            const char* dataPointer = reinterpret_cast<const char*>(pixels) + depthLevel * amountToCopy;
            if (!stagingBuffer->copyFromAndFlush(context, 0, reinterpret_cast<const void*>(dataPointer), amountToCopy, 0))
            {
                g_logger->log(Vulkan::Logger::Level::Error, std::string("createImage - Failed to fill staging buffer\n"));
                return false;
            }


            if (!stagingBuffer->getBuffer(0).copyToAndFlush(context._device,
                context._commandPools[queue._familyIndex],
                queue._queue,
                result._image,
                VkOffset3D{ 0, 0, depthLevel },
                VkExtent3D{ width, height,  queue._minGranularity.depth }))
            {
                g_logger->log(Vulkan::Logger::Level::Error, std::string("createImage - copyData\n"));
                return false;
            }
        }
    }

    if (!Vulkan::transitionImageLayoutAndSubmit(context,
        result._image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        finalLayout))
    {
        g_logger->log(Vulkan::Logger::Level::Error, std::string("createImage - transitionImageLayout : VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL -> VK_IMAGE_LAYOUT_GENERAL\n"));
        return false;
    }
    return true;
}

bool Vulkan::createImage(Vulkan::Context& context,
    const void* pixels, 
    const unsigned int pixelSize, 
    const unsigned int width, 
    const unsigned int height, 
    const unsigned int depth, 
    const unsigned int samplesPrPixels,
    VkFormat format,
    Vulkan::ImageDescriptor & result, 
    unsigned int mipMapLevels,
    VkImageLayout finalLayout)
{
    if (!Vulkan::createImage(context,
        width,
        height,
        depth,
        mipMapLevels,
        samplesPrPixels,
        format,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        result))
    {
        g_logger->log(Vulkan::Logger::Level::Error, std::string("createImage - Failed to create image\n"));
        return false;
    }

    if (!Vulkan::transitionImageLayoutAndSubmit(context,
        result._image,
        VK_IMAGE_LAYOUT_UNDEFINED,
        finalLayout))
    {
        g_logger->log(Vulkan::Logger::Level::Error, std::string("createImage - transitionImageLayout : VK_IMAGE_LAYOUT_UNDEFINED -> VK_IMAGE_LAYOUT_GENERAL\n"));
        return false;
    }

    if (pixels != nullptr) {
        const bool imageUpdateSucceeded = updataImageData(context, result, pixels, mipMapLevels, pixelSize, width, height, depth, finalLayout);
        return imageUpdateSucceeded;
    }

    return true;
}


bool Vulkan::createImageView(Vulkan::Context& context, VkImage image, VkFormat requiredFormat, VkImageAspectFlags requiredAspectFlags, VkImageViewType imageViewType, VkImageView& result)
{
    VkImageViewCreateInfo createInfo;
    memset(&createInfo, 0, sizeof(createInfo));
    createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    createInfo.pNext = NULL;
    createInfo.flags = 0;
    createInfo.image = image;
    createInfo.viewType = imageViewType;
    createInfo.format = requiredFormat;
    createInfo.components = VkComponentMapping{ VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
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

bool Vulkan::transitionImageLayout(Vulkan::Context& context,
    VkImage image,
    VkImageLayout oldLayout,
    VkImageLayout newLayout,
    VkAccessFlags srcAccessMask,
    VkAccessFlags dstAccessMask,
    VkPipelineStageFlags srcStageFlags,
    VkPipelineStageFlags dstStageFlags,
    VkCommandBuffer commandBuffer)
{
    VkImageMemoryBarrier barrier;
    memset(&barrier, 0, sizeof(barrier));
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.pNext = NULL;
    barrier.srcAccessMask = srcAccessMask;
    barrier.dstAccessMask = dstAccessMask;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL || oldLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
        ? VK_IMAGE_ASPECT_DEPTH_BIT
        : VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    vkCmdPipelineBarrier(
        commandBuffer,
        srcStageFlags,
        dstStageFlags,
        0,
        0,
        nullptr,
        0,
        nullptr,
        1,
        &barrier);

    return true;
}

bool Vulkan::transitionImageLayout(Vulkan::Context& context,
    VkImage image,
    VkImageLayout oldLayout,
    VkImageLayout newLayout,
    VkCommandBuffer commandBuffer)
{
    VkAccessFlags srcAccessMask = 0;
    VkAccessFlags dstAccessMask = 0;

    VkPipelineStageFlags sourceStage = 0;
    VkPipelineStageFlags destinationStage = 0;

    struct LayoutMapping {
        VkAccessFlags accessMask = 0;
        VkPipelineStageFlags stage = 0;

        LayoutMapping() {} 
        LayoutMapping(const LayoutMapping & src) 
        :accessMask(src.accessMask)
        ,stage(src.stage) { }

        LayoutMapping(VkAccessFlags aMask, VkPipelineStageFlags st) 
            :accessMask(aMask)
            ,stage(st) {}
    };

    static const std::map<VkImageLayout, LayoutMapping> dstMappings = {
        {VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, {VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT }},
        {VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, {VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT }},
        {VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, {VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT }},
        {VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, {VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT }},
        {VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, {VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT }},
        {VK_IMAGE_LAYOUT_GENERAL, {0, VK_PIPELINE_STAGE_TRANSFER_BIT }},
        {VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, {VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT }},
    };

    srcAccessMask = 0;
    sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    const auto it = dstMappings.find(newLayout);
    if (it == dstMappings.end()) {
        assert(0);
        return false;
    }

    dstAccessMask = it->second.accessMask;
    destinationStage = it->second.stage;

    return transitionImageLayout(context, image, oldLayout, newLayout, srcAccessMask, dstAccessMask, sourceStage, destinationStage, commandBuffer);
}

bool Vulkan::transitionImageLayoutAndSubmit(Vulkan::Context & context, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout)
{
    VkCommandBuffer commandBuffer;
    Context::Queue& queue = getQueue(context, VK_QUEUE_TRANSFER_BIT);
    if (!::createCommandBuffer(context, context._commandPools[queue._familyIndex], &commandBuffer))
        return false;

    VkCommandBufferBeginInfo beginInfo;
    memset(&beginInfo, 0, sizeof(beginInfo));
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    beginInfo.pInheritanceInfo = NULL;
    beginInfo.pNext = NULL;

    const VkResult beginCommandResult = vkBeginCommandBuffer(commandBuffer, &beginInfo);
    if (beginCommandResult != VK_SUCCESS)
        return false;

    transitionImageLayout(context, image, oldLayout, newLayout, commandBuffer);

    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    VkFence fence = Vulkan::createFence(context._device, 0);
    assert(fence != VK_NULL_HANDLE);

    const VkResult submitResult = vkQueueSubmit(queue._queue, 1, &submitInfo, fence);
    assert(submitResult == VK_SUCCESS);

    FenceCommandBufferPair pair{ fence, commandBuffer, context._commandPools[queue._familyIndex] };
    context._fenceCommandBufferPairs.push_back(pair);

    return true;
}

void Vulkan::checkForFinishedPairCommandBufferBuffers(Context& context) {
    while (!context._fenceCommandBufferPairs.empty()) {
        for (int i = 0; i < context._fenceCommandBufferPairs.size(); ) {
            FenceCommandBufferPair& pair = context._fenceCommandBufferPairs[i];
            const VkResult waitForFencesResult = vkWaitForFences(context._device, 1, &pair._fence, VK_TRUE, 0);
            if (waitForFencesResult == VK_SUCCESS) {
                vkDestroyFence(context._device, pair._fence, nullptr);
                vkFreeCommandBuffers(context._device, pair._pool, 1, &pair._buffer);
                context._fenceCommandBufferPairs[i] = context._fenceCommandBufferPairs[context._fenceCommandBufferPairs.size() - 1];
                context._fenceCommandBufferPairs.pop_back();
            }
            else {
                i++;
            }
        }
    }

}


bool Vulkan::setupDebugCallback(Vulkan::Context & context)
{
    if (!validationLayersEnabled)
        return true; // everything is true about the empty set

	// debug utils
	{
        auto debugUtilsMessengerCreator = vkCreateDebugUtilsMessengerEXT;
		if (debugUtilsMessengerCreator == nullptr)
		{
#if !defined(__APPLE__) // function doesn't seeem to exist with moltenvk
            g_logger->log(Vulkan::Logger::Level::Error, std::string("Could not create function: vkCreateDebugUtilsMessengerEXT") + "\n");
#endif
		}
		else
        {
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
                g_logger->log(Vulkan::Logger::Level::Error, std::string("Failed to create callback for method: ") + "\n");
                // TODO: should probably destroy the callbackCreator here
            }
#endif
        }

	}

	// debug report
	{
        auto debugReportMessengerCreator = vkCreateDebugReportCallbackEXT;
		if (debugReportMessengerCreator == nullptr)
		{
#if defined(__APPLE__) // function doesn't seeem to exist with moltenvk
			return true;
#else
            g_logger->log(Vulkan::Logger::Level::Error, std::string("Could not create function: vkCreateDebugReportCallbackEXT") +  + "\n");
#endif
		}
		else
	    {
            VkDebugReportCallbackCreateInfoEXT createInfo;
            memset(&createInfo, 0, sizeof(createInfo));

            createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT;
            createInfo.pNext = nullptr;
            createInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT
                               | VK_DEBUG_REPORT_WARNING_BIT_EXT
                               | VK_DEBUG_REPORT_DEBUG_BIT_EXT
                               | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
            createInfo.pUserData = nullptr;
            createInfo.pfnCallback = &VulkanDebugReportCallback;

            VkResult debugReportCreationResult = debugReportMessengerCreator(context._instance, &createInfo, nullptr, &context._debugReportCallback);
            assert(debugReportCreationResult == VK_SUCCESS);
            if (debugReportCreationResult != VK_SUCCESS)
            {
                g_logger->log(Vulkan::Logger::Level::Error, std::string("Failed to create callback for method ") + "\n");
            }

        }
	}

    return true;
}

void Vulkan::areValidationLayersAvailable(const std::vector<const char*> & validationLayers, std::vector<const char*> & output)
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
        for (const char * neededLayer : validationLayers)
        {
            bool found = false;
            for (const auto & layer : layers)
            {
                if (strcmp(neededLayer, layer.layerName) == 0)
                {
                    output.push_back(neededLayer);
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                 g_logger->log(Vulkan::Logger::Level::Error, std::string("Could not find needed validation layer ") + neededLayer + "\n");
            }
        }
    }
}

bool Vulkan::createInstanceAndLoadExtensions(Vulkan::AppDescriptor & appDesc, Vulkan::Context & context)
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
    g_logger->log(Vulkan::Logger::Level::Info, std::string("Vulkan Instance Extensions. Count = ") + std::to_string(instanceExtensionCount) + "\n");
    for (unsigned int i = 0; i < (unsigned int)instanceExtensionNames.size(); i++)
        g_logger->log(Vulkan::Logger::Level::Info, std::string("\t") + std::to_string(i) + std::string(": ") + std::string(instanceExtensionNames[i]) + "\n");
    
    
    unsigned int requiredInstanceExtensionCount = 0;
    if (!SDL_Vulkan_GetInstanceExtensions(appDesc._window, &requiredInstanceExtensionCount, nullptr))
    {
        g_logger->log(Vulkan::Logger::Level::Error, std::string("Failed to get number of extensions\n"));
        return false;
    }
    
    std::vector<const char*> requiredInstanceExtensions;
    requiredInstanceExtensions.resize(requiredInstanceExtensionCount);
    if (!SDL_Vulkan_GetInstanceExtensions(appDesc._window, &requiredInstanceExtensionCount, &requiredInstanceExtensions[0]))
    {
        g_logger->log(Vulkan::Logger::Level::Error, std::string("Failed to acquire possible extensions error\n"));
        return false;
    }
        
    for (const char* extension : requiredInstanceExtensions)
        appDesc.addRequiredInstanceExtension(extension);

    for (const char* extensionName : instanceExtensionNames)
    {
        const char* wantedExtensions[] =
        {
            "VK_EXT_debug_report",
            VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
            VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
        };

        for (const char* wantedExtension : wantedExtensions)
        {
            if (strcmp(extensionName, wantedExtension) == 0)
                appDesc.addRequiredInstanceExtension(wantedExtension);
        }
    }
    
    // log all the required extensions
    std::vector<std::string> sRequiredInstanceExtensions = appDesc.getRequiredInstanceExtensions();
    g_logger->log(Vulkan::Logger::Level::Info, std::string("Required Vulkan Instance Extensions. Count = ") + std::to_string(requiredInstanceExtensionCount) + "\n");
    for (unsigned int i = 0; i < requiredInstanceExtensionCount; i++)
        g_logger->log(Vulkan::Logger::Level::Info, std::string("\t") + std::to_string(i) + std::string(": ") + sRequiredInstanceExtensions[i] + "\n");
    
    // check if required extensions are available
    {
        unsigned int requiredIndex = 0;
        for (requiredIndex = 0; requiredIndex < requiredInstanceExtensionCount; requiredIndex++)
        {
            const char* requiredExtension = sRequiredInstanceExtensions[requiredIndex].c_str();
            unsigned int instanceIndex = 0;
            for (unsigned int instanceIndex = 0; instanceIndex < instanceExtensionCount; instanceIndex++)
            {
                const char * extension = instanceExtensionNames[instanceIndex];
                if (strcmp(extension, requiredExtension) == 0)
                    break;
            }
            
            if (instanceIndex == instanceExtensionCount)
            {
                g_logger->log(Vulkan::Logger::Level::Error, std::string("Required vulkan extension ") + requiredExtension + " not found\n");
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
    
    requiredInstanceExtensions.clear();
    for (const std::string& extension : sRequiredInstanceExtensions) {
        g_logger->log(Vulkan::Logger::Level::Verbose, "Loading Instance Extension: " + extension);
        requiredInstanceExtensions.push_back(extension.c_str());
    }


    instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceCreateInfo.pApplicationInfo = &vkAppInfo;
    instanceCreateInfo.enabledExtensionCount = (uint32_t)requiredInstanceExtensions.size();
    instanceCreateInfo.ppEnabledExtensionNames = &requiredInstanceExtensions[0];
    
    std::vector<const char*> output;
    if (validationLayersEnabled)
    {
        static const std::vector<const char*> validationLayers = {
#if defined(_FUGL_APPLE_)
           "MoltenVK",
#endif
//           "VK_LAYER_NV_optimus",
//           "VK_LAYER_RENDERDOC_Capture",
//           "GalaxyOverlayVkLayer",
//           "GalaxyOverlayVkLayer_VERBOSE",
//           "GalaxyOverlayVkLayer_DEBUG",
//           "VK_LAYER_NV_nomad_release_public_2020_2_0",
//           "VK_LAYER_NV_GPU_Trace_release_public_2020_2_0",
//            "VK_LAYER_NV_nsight-sys",
//            "VK_LAYER_NV_nsight",
//            "VK_LAYER_VALVE_steam_overlay",
//            "VK_LAYER_VALVE_steam_fossilize",
//            "VK_LAYER_EOS_Overlay", 
//            "VK_LAYER_LUNARG_api_dump",
//            "VK_LAYER_LUNARG_device_simulation",
            "VK_LAYER_KHRONOS_validation",
//            "VK_LAYER_LUNARG_monitor",
//            "VK_LAYER_LUNARG_screenshot",
//            "VK_LAYER_LUNARG_vktrace" ,
//            "VK_LAYER_LUNARG_standard_validation",
//            "VK_LAYER_AMD_switchable_graphics",
        };
        areValidationLayersAvailable(validationLayers, output);
        if(!output.empty())
        {
            instanceCreateInfo.enabledLayerCount = (uint32_t)output.size();
            instanceCreateInfo.ppEnabledLayerNames = &output[0];
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

namespace
{
    void logChosenPhysicalDeviceFeatures(VkPhysicalDeviceFeatures& features)
    {
        std::string message;
        message += "VkPhysicalDeviceFeatures\n";
        message += "------------------------\n";
        message += "multiDrawIndirect = " + std::to_string(features.multiDrawIndirect) + "\n";
        message += "drawIndirectFirstInstance = " + std::to_string(features.drawIndirectFirstInstance) + "\n";
        message += "fullDrawIndexUint32 = " + std::to_string(features.fullDrawIndexUint32) + "\n";
        message += "robustBufferAccess = " + std::to_string(features.robustBufferAccess) + "\n";
        g_logger->log(Vulkan::Logger::Level::Verbose, message);
    }

    void logPhysicalDeviceProperties(VkPhysicalDeviceProperties& properties)
    {
        std::string message;
        message += "VkPhysicalDeviceProperties\n";
        message += "--------------------------\n";
        message += "apiVersion = " + std::to_string(properties.apiVersion) + "\n";
        message += "driverVersion = " + std::to_string(properties.driverVersion) + "\n";
        message += "vendorID = " + std::to_string(properties.vendorID) + "\n";
        message += "deviceID = " + std::to_string(properties.deviceID) + "\n";
        message += "VkPhysicalDeviceType = " + std::to_string((int)properties.deviceType) + "\n";
        message += "deviceName = " + std::string(properties.deviceName) + "\n";
        message += "limits.minUniformBufferOffsetAlignment = " + std::to_string(properties.limits.minUniformBufferOffsetAlignment) + "\n";
        message += "limits.framebufferDepthSampleCounts = " + std::to_string(properties.limits.framebufferDepthSampleCounts) + "\n";
        message += "limits.framebufferColorSampleCounts = " + std::to_string(properties.limits.framebufferColorSampleCounts) + "\n";
        message += "limits.maxDrawIndexedIndexValue = " + std::to_string(properties.limits.maxDrawIndexedIndexValue) + "\n";
        message += "limits.maxDrawIndirectCount = " + std::to_string(properties.limits.maxDrawIndirectCount) + "\n";
        message += "limits.nonCoherentAtomSize = " + std::to_string(properties.limits.nonCoherentAtomSize) + "\n";
        message += "limits.maxDescriptorSetUniformBuffers = " + std::to_string(properties.limits.maxDescriptorSetUniformBuffers) + "\n";
        message += "limits.maxDescriptorSetUniformBuffersDynamic = " + std::to_string(properties.limits.maxDescriptorSetUniformBuffersDynamic) + "\n";
        message += "limits.maxDescriptorSetStorageBuffers = " + std::to_string(properties.limits.maxDescriptorSetStorageBuffers) + "\n";
        message += "limits.maxDescriptorSetStorageBuffersDynamic = " + std::to_string(properties.limits.maxDescriptorSetStorageBuffersDynamic) + "\n";
        message += "limits.maxVertexInputAttributes = " + std::to_string(properties.limits.maxVertexInputAttributes) + "\n";
        message += "limits.maxVertexInputBindings = " + std::to_string(properties.limits.maxVertexInputBindings) + "\n";
        message += "limits.maxVertexInputAttributeOffset = " + std::to_string(properties.limits.maxVertexInputAttributeOffset) + "\n";
        message += "limits.maxVertexInputBindingStride = " + std::to_string(properties.limits.maxVertexInputBindingStride) + "\n";
        g_logger->log(Vulkan::Logger::Level::Verbose, message);
    }
}


bool Vulkan::enumeratePhysicalDevices(Vulkan::AppDescriptor & appDesc, Vulkan::Context & context)
{
    uint32_t deviceCount = 0;
    // first count the number of physical posibleDevices by passing in NULL as the last argument
    VkResult countResult = vkEnumeratePhysicalDevices(context._instance, &deviceCount, NULL);
	assert(countResult == VK_SUCCESS);
	if (countResult != VK_SUCCESS) {
        g_logger->log(Vulkan::Logger::Level::Error, std::string("vkEnumeratePhysicalDevices returned error code ") + std::to_string(countResult) + "\n");
        return false;
    }
    
    if (deviceCount == 0) {
        g_logger->log(Vulkan::Logger::Level::Error, std::string("vkEnumeratePhysicalDevices returned 0 devices\n"));
        return false;
    }
    
    std::vector<VkPhysicalDevice> posibleDevices(deviceCount);
    std::vector<VkPhysicalDevice> devices;
    VkResult enumerateResult = vkEnumeratePhysicalDevices(context._instance, &deviceCount, &posibleDevices[0]);
	assert(enumerateResult == VK_SUCCESS);
	if (enumerateResult != VK_SUCCESS) {
        g_logger->log(Vulkan::Logger::Level::Error, std::string("vkEnumeratePhysicalDevices returned error code ") + std::to_string(countResult) + "\n");
        return false;
    }
    
    unsigned int deviceIndex = 0;
    for (; deviceIndex < deviceCount; deviceIndex++) {
        VkPhysicalDevice device = posibleDevices[deviceIndex];
        VkPhysicalDeviceProperties deviceProperties = {0};
        vkGetPhysicalDeviceProperties(device, &deviceProperties);
        
        if (deviceProperties.apiVersion < appDesc._requiredVulkanVersion) {
            continue;
        }

        devices.push_back(device);
    }
        
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
    Context._deviceProperties = currentPhysicalProperties;
    Context._physicalDevice = appDesc._physicalDevices[currentPhysicalDevice];
    std::string chosenPhysicalDeviceName(currentPhysicalProperties.deviceName);
    g_logger->log(Vulkan::Logger::Level::Info, std::string("Chosen Vulkan Physical Device = ") + chosenPhysicalDeviceName + ". Driver version = " + std::to_string(currentPhysicalProperties.driverVersion) + "\n");
    
	// get the device features, so we can check against them later on
	vkGetPhysicalDeviceFeatures(Context._physicalDevice, &Context._physicalDeviceFeatures);
    logChosenPhysicalDeviceFeatures(Context._physicalDeviceFeatures);

    // check if chosen device is intel - then return false;
    std::transform(chosenPhysicalDeviceName.begin(), chosenPhysicalDeviceName.end(), chosenPhysicalDeviceName.begin(), ::tolower);
    if (chosenPhysicalDeviceName.find("intel") != std::string::npos) {
        g_logger->log(Vulkan::Logger::Level::Info, "Intel graphics cards not supported\n");
        return false;
    }

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
  std::vector<float> fQueuePriorities;

  // count max size of queues and set all queue priorities to the same value
  unsigned int maxQueueCount = 0;
  for (VkQueueFamilyProperties& queueProperty : queueProperties)
  {
      maxQueueCount = std::max<unsigned int>(maxQueueCount, queueProperty.queueCount);
  }
  fQueuePriorities.resize(maxQueueCount);
  for (float& priority : fQueuePriorities)
      priority = 1.0f;

  std::vector<VkDeviceQueueCreateInfo> deviceQueueCreateInfos;
  for (uint32_t familyIndex = 0; familyIndex < pQueueFamilyCount; familyIndex++)
  {
      VkDeviceQueueCreateInfo deviceQueueCreateInfo;
      memset(&deviceQueueCreateInfo, 0, sizeof(deviceQueueCreateInfo));
      deviceQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
      deviceQueueCreateInfo.queueFamilyIndex = familyIndex;
      deviceQueueCreateInfo.queueCount = queueProperties[familyIndex].queueCount;
      deviceQueueCreateInfo.pQueuePriorities = &fQueuePriorities[0];
      deviceQueueCreateInfos.push_back(deviceQueueCreateInfo);
  }
  
  VkDeviceCreateInfo deviceCreateInfo;
  memset(&deviceCreateInfo, 0, sizeof(deviceCreateInfo));
    
  deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  deviceCreateInfo.queueCreateInfoCount = (uint32_t)deviceQueueCreateInfos.size();
  deviceCreateInfo.pQueueCreateInfos = &deviceQueueCreateInfos[0];
  deviceCreateInfo.pEnabledFeatures = &context._physicalDeviceFeatures;

  // add up required device extensions
  {
      std::vector<const char*> deviceExtensionNames;
      deviceExtensionNames.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
      if (appDesc.hasExtension(std::string("VK_EXT_memory_budget")))
          deviceExtensionNames.push_back("VK_EXT_memory_budget");
      if (appDesc.hasExtension(std::string("VK_KHR_get_physical_device_properties2")))
          deviceExtensionNames.push_back("VK_KHR_get_physical_device_properties2");

      for (const char* extension : deviceExtensionNames)
          appDesc.addRequiredDeviceExtension(extension);
  }

  // validate that all the required extensions are there
  std::vector<std::string> sRequiredDeviceExtensions = appDesc.getRequiredDeviceExtensions();
  for (unsigned int i=0 ; i < sRequiredDeviceExtensions.size() ; )
  {
      const std::string& requiredExtension = sRequiredDeviceExtensions[i];
      if (!appDesc.hasExtension(requiredExtension))
      {
          auto it = std::find(sRequiredDeviceExtensions.begin(), sRequiredDeviceExtensions.end(), requiredExtension);
          sRequiredDeviceExtensions.erase(it);
          i = 0;
      }
      else
          i++;
  }


  std::vector<const char*> cRequiredDeviceExtensions;
  for (const std::string& requiredExtension : sRequiredDeviceExtensions)
      cRequiredDeviceExtensions.push_back(requiredExtension.c_str());

  // log all the required device extensions
  g_logger->log(Vulkan::Logger::Level::Info, std::string("Required Vulkan Device Extensions. Count = ") + std::to_string(sRequiredDeviceExtensions.size()) + "\n");
  for (unsigned int i = 0; i < sRequiredDeviceExtensions.size(); i++)
      g_logger->log(Vulkan::Logger::Level::Info, std::string("\t") + std::to_string(i) + std::string(": ") + sRequiredDeviceExtensions[i] + "\n");


  deviceCreateInfo.ppEnabledExtensionNames = &cRequiredDeviceExtensions[0];
  deviceCreateInfo.enabledExtensionCount = (uint32_t)cRequiredDeviceExtensions.size();

  VkPhysicalDeviceVulkan11Features neededFeatures = { };
  neededFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
  neededFeatures.shaderDrawParameters = 1;
  deviceCreateInfo.pNext = &neededFeatures;

  
  VkResult creationResult = vkCreateDevice(appDesc._physicalDevices[appDesc._chosenPhysicalDevice], &deviceCreateInfo, nullptr /* no allocation callbacks at this time */, &context._device);
  assert(creationResult == VK_SUCCESS);
  if (creationResult != VK_SUCCESS)
  {
      g_logger->log(Vulkan::Logger::Level::Error, std::string("Failed to create vulkan device\n"));
      return false;
  }

  static unsigned int id = 0;
  for (uint32_t familyIndex = 0; familyIndex < pQueueFamilyCount; familyIndex++)
  {
      VkQueueFamilyProperties& queueProperty = queueProperties[familyIndex];
      for (unsigned int i = 0; i < queueProperty.queueCount; i++)
      {
          Vulkan::Context::Queue queue;
          vkGetDeviceQueue(context._device, familyIndex, i, &queue._queue);

          queue._flagBits = queueProperty.queueFlags;
          queue._id = id++;
          queue._familyIndex = familyIndex;
          queue._queueIndex = i;
          queue._minGranularity = queueProperty.minImageTransferGranularity;
          // assign the queue to all the relevant buckets
          for (unsigned int queueMask = 0; queueMask < 8; queueMask++)
          {
              if ((queue._flagBits & queueMask) == queueMask)
                  context._queues[queueMask].push_back(queue);
          }
      }
  }
  context._numQueueFamilies = pQueueFamilyCount;

  // sort all the queues, so that the one with the least supported goes first
  for (unsigned int queueMask = 0; queueMask < 8; queueMask++)
  {
      std::sort(context._queues[queueMask].begin(), context._queues[queueMask].end(), [](const Context::Queue & a, const Context::Queue & b) { return a._flagBits < b._flagBits; });
  }


  return creationResult == VK_SUCCESS;


}

bool Vulkan::createSwapChain(AppDescriptor & appDesc, Context & context)
{
    appDesc._actualNumSamples = requestNumAASamples(context, appDesc._requestedNumSamples);

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
    
    std::vector<VkSurfaceFormatKHR> surfaceFormats;
    surfaceFormats.resize(surfaceFormatsCount);
    VkResult surfaceFormatsResult = vkGetPhysicalDeviceSurfaceFormatsKHR(context._physicalDevice, context._surface, &surfaceFormatsCount, &surfaceFormats[0]);
	assert(surfaceFormatsResult == VK_SUCCESS);
	if (surfaceFormatsResult != VK_SUCCESS || surfaceFormatsCount == 0)
        return false;

	// check if present support for interface
	VkBool32 presentSupported = false;
	VkResult surfaceSupportedResult = vkGetPhysicalDeviceSurfaceSupportKHR(context._physicalDevice, 0, context._surface, &presentSupported);
	assert(surfaceSupportedResult == VK_SUCCESS);
	if(!presentSupported)
		return false;

    context._surfaceFormat = surfaceFormats[0];
    // avoid SRGB if we can
    if(context._surfaceFormat.format==VK_FORMAT_B8G8R8A8_SRGB && surfaceFormats.size()>1) {
      context._surfaceFormat = surfaceFormats[1];
    }
    
    if (appDesc.hasPreferredSurfaceFormat())
    {
        VkSurfaceFormatKHR preferredSurfaceFormat = appDesc.getPreferredSurfaceFormat();
        for (unsigned int i = 0; i < (unsigned int)surfaceFormats.size(); i++)
        {
            if (preferredSurfaceFormat.format == surfaceFormats[i].format &&
                preferredSurfaceFormat.colorSpace == surfaceFormats[i].colorSpace)
            {
                context._surfaceFormat = preferredSurfaceFormat;
                break;
            }
        }
    }


    VkSwapchainCreateInfoKHR swapChainCreateInfo;
    memset(&swapChainCreateInfo, 0, sizeof(swapChainCreateInfo));
    
    const unsigned int imageCount = std::max<uint32_t>(std::min<uint32_t>(3, context._surfaceCapabilities.maxImageCount), context._surfaceCapabilities.minImageCount);
    if (imageCount == 0)
        return false;
    

    uint32_t presentModeCount = 0;
    VkResult getPossiblePresentModes0 = vkGetPhysicalDeviceSurfacePresentModesKHR(context._physicalDevice, context._surface, &presentModeCount, nullptr);
    assert(getPossiblePresentModes0 == VK_SUCCESS && presentModeCount != 0);
    std::vector< VkPresentModeKHR> possiblePresentModes(presentModeCount);
    VkResult getPossiblePresentModes1 = vkGetPhysicalDeviceSurfacePresentModesKHR(context._physicalDevice, context._surface, &presentModeCount, &possiblePresentModes[0]);
    assert(getPossiblePresentModes1 == VK_SUCCESS && presentModeCount != 0);

    // we are betting that at least VK_PRESENT_MODE_FIFO_KHR or VK_PRESENT_MODE_IMMEDIATE_KHR is present
    // TODO should we add mailboxing if possible?
    VkPresentModeKHR presentModeWithVSync = (std::find(possiblePresentModes.begin(), possiblePresentModes.end(), VK_PRESENT_MODE_FIFO_KHR) != possiblePresentModes.end()) ?
        VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_IMMEDIATE_KHR;
    VkPresentModeKHR presentModeWithoutVSync = (std::find(possiblePresentModes.begin(), possiblePresentModes.end(), VK_PRESENT_MODE_IMMEDIATE_KHR) != possiblePresentModes.end()) ?
        VK_PRESENT_MODE_IMMEDIATE_KHR : VK_PRESENT_MODE_FIFO_KHR;

    context._swapChainSize.width = appDesc._drawableSurfaceWidth;
    context._swapChainSize.height = appDesc._drawableSurfaceHeight;
    
    swapChainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapChainCreateInfo.surface = context._surface;
    swapChainCreateInfo.minImageCount = imageCount;
    swapChainCreateInfo.imageFormat = context._surfaceFormat.format;
    swapChainCreateInfo.imageColorSpace = context._surfaceFormat.colorSpace;
    swapChainCreateInfo.imageExtent = context._swapChainSize;
    swapChainCreateInfo.imageArrayLayers = 1;
    swapChainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapChainCreateInfo.preTransform = context._surfaceCapabilities.currentTransform;
    swapChainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapChainCreateInfo.presentMode = appDesc._enableVSync ? presentModeWithVSync : presentModeWithoutVSync;
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
    
    context._swapChainImages.resize(vkImageCount);
    VkResult getSwapChainImagesResult = vkGetSwapchainImagesKHR(context._device, context._swapChain, &vkImageCount, &context._swapChainImages[0]);
	assert(getSwapChainImagesResult == VK_SUCCESS);

    return true;
}

bool Vulkan::createDepthBuffer(Context& context, uint32_t numSamples, VkExtent2D size, Vulkan::ImageDescriptor & image, VkImageView& imageView)
{
    constexpr VkImageTiling requiredTiling = VK_IMAGE_TILING_OPTIMAL;
    VkFormat depthFormat = findDepthFormat(context, requiredTiling);

    if (!createImage(context,
        size.width,
        size.height,
        1,
        1,
        numSamples,
        depthFormat,
        requiredTiling,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        image))
        return false;

    /*
    if (!allocateImageMemory(context, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, image, memory))
        return false;
        */
    if (!createImageView(context, image._image, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_VIEW_TYPE_2D, imageView))
        return false;

    if (!transitionImageLayoutAndSubmit(context, image._image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL))
        return false;

    return true;
}


bool Vulkan::createDepthBuffers(Context & context, uint32_t numSamples, VkExtent2D size, std::vector<Vulkan::ImageDescriptor> & images, std::vector<VkImageView> & imageViews)
{
    const unsigned int numBuffers = (unsigned int)context._swapChainImages.size();
	imageViews.resize(numBuffers);
	images.resize(numBuffers);
	for (unsigned int i = 0; i < numBuffers; i++)
	{
        if (!createDepthBuffer(context, numSamples, size,  images[i], imageViews[i]))
            return false;
	}

	return true;
}

bool Vulkan::createColorBuffers(Context & context)
{
    for (VkImage & image : context._swapChainImages)
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
        
        context._swapChainImageViews.push_back(imageView);
    }
    
    return true;
}

bool Vulkan::createRenderPass(Context & Context, uint32_t numAASamples, VkRenderPass * result, RenderPassCustomizationCallback renderPassCreationCallback)
{
    VkRenderPassCreateInfoDescriptor renderPassInfoDescriptor;

    VkAttachmentReference & colorAttachmentReference = renderPassInfoDescriptor._colorAttachmentReference;
    memset(&colorAttachmentReference, 0, sizeof(colorAttachmentReference));
    colorAttachmentReference.attachment = 0;
    colorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference & depthAttachmentReference = renderPassInfoDescriptor._depthAttachmentReference;
	memset(&depthAttachmentReference, 0, sizeof(depthAttachmentReference));
	depthAttachmentReference.attachment = 1;
	depthAttachmentReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // only used if numSamples > 1
    VkAttachmentReference& colorAttachmentReferenceResolve = renderPassInfoDescriptor._colorAttachmentReferenceResolve;
    memset(&colorAttachmentReferenceResolve, 0, sizeof(colorAttachmentReference));
    colorAttachmentReferenceResolve.attachment = 2;
    colorAttachmentReferenceResolve.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription& subpassDescription = renderPassInfoDescriptor._subpassDescription;
    memset(&subpassDescription, 0, sizeof(subpassDescription));
    subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDescription.colorAttachmentCount = 1; 
    subpassDescription.pColorAttachments = &colorAttachmentReference;
	subpassDescription.pDepthStencilAttachment = &depthAttachmentReference;

    if(numAASamples>1)
        subpassDescription.pResolveAttachments = &colorAttachmentReferenceResolve;

    VkAttachmentDescription& colorAttachment = renderPassInfoDescriptor._colorAttachment;
    memset(&colorAttachment, 0, sizeof(colorAttachment));
    colorAttachment.format = Context._surfaceFormat.format;
    colorAttachment.samples = static_cast<VkSampleCountFlagBits>(numAASamples);
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription& depthAttachment = renderPassInfoDescriptor._depthAttachment;
	memset(&depthAttachment, 0, sizeof(depthAttachment));
	depthAttachment.format = findDepthFormat(Context, VK_IMAGE_TILING_OPTIMAL);
	depthAttachment.samples = static_cast<VkSampleCountFlagBits>(numAASamples);
	depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // only used if numSamples > 1
    VkAttachmentDescription& colorAttachmentResolve = renderPassInfoDescriptor._colorAttachmentResolve;
    memset(&colorAttachmentResolve, 0, sizeof(colorAttachment));
    colorAttachmentResolve.format = Context._surfaceFormat.format;
    colorAttachmentResolve.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachmentResolve.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachmentResolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachmentResolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachmentResolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachmentResolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachmentResolve.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    std::array<VkSubpassDependency,10> & dependencies = renderPassInfoDescriptor._dependency;
    {
        VkSubpassDependency& dependency = dependencies[0];
        memset(&dependency, 0, sizeof(dependency));
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependency.dependencyFlags = 0;
    }

    {
        VkSubpassDependency& dependency = dependencies[1];
        memset(&dependency, 0, sizeof(dependency));
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        dependency.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        dependency.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        dependency.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
        dependency.dependencyFlags = 0;
    };

    VkRenderPassCreateInfo& createInfo = renderPassInfoDescriptor._createInfo;
    memset(&createInfo, 0, sizeof(createInfo));
    std::array<VkAttachmentDescription, 10> & attachmentDescriptions = renderPassInfoDescriptor._attachmentDescriptions;
    createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    createInfo.attachmentCount = (numAASamples> 1) ? 3 : 2;
	createInfo.pAttachments = &attachmentDescriptions[0];
    createInfo.subpassCount = 1;
    createInfo.pSubpasses = &subpassDescription;
	createInfo.dependencyCount = 2;
	createInfo.pDependencies = &dependencies[0];

    renderPassCreationCallback(renderPassInfoDescriptor); // give the user a chance to customize pipeline
    createInfo.attachmentCount = 0;
    attachmentDescriptions[createInfo.attachmentCount++] = colorAttachment;
    if (subpassDescription.pDepthStencilAttachment != nullptr) 
        attachmentDescriptions[createInfo.attachmentCount++] = depthAttachment;

    if (numAASamples > 1)
        attachmentDescriptions[createInfo.attachmentCount++] = colorAttachmentResolve;

	VkRenderPass renderPass;
    VkResult createRenderPassResult = vkCreateRenderPass(Context._device, &createInfo, nullptr, &renderPass);
	assert(createRenderPassResult == VK_SUCCESS);
	if (createRenderPassResult != VK_SUCCESS)
        return false;

	*result = renderPass;
    
    return true;
}

bool Vulkan::createFrameBuffers(VkDevice device, VkExtent2D frameBufferSize, VkRenderPass & renderPass, std::vector<VkImageView>& colorViews, std::vector<VkImageView> & msaaViews, std::vector<VkImageView>& depthViews, std::vector<VkFramebuffer>& result)
{
    result.resize(colorViews.size());
	for(unsigned int i=0 ; i < (unsigned int)colorViews.size() ; i++ )
    {
        VkFramebufferCreateInfo createInfo;
        memset(&createInfo, 0, sizeof(createInfo));

        std::vector<VkImageView> attachments;
        if (msaaViews.empty())
        {
            attachments.push_back(colorViews[i]);
            if (!depthViews.empty())
                attachments.push_back(depthViews[i]);
        }
        else
        {
            if (!msaaViews.empty())
                attachments.push_back(msaaViews[i]);
            if (!depthViews.empty())
                attachments.push_back(depthViews[i]);
            attachments.push_back(colorViews[i]);
        }
        
        createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        createInfo.renderPass = renderPass;
        createInfo.attachmentCount = (uint32_t)attachments.size();
		createInfo.pAttachments = &attachments[0];
        createInfo.width = frameBufferSize.width;
        createInfo.height = frameBufferSize.height;
		createInfo.layers = 1;
        
        VkFramebuffer frameBuffer;
        VkResult frameBufferCreationResult = vkCreateFramebuffer(device, &createInfo, nullptr, &frameBuffer);
		assert(frameBufferCreationResult == VK_SUCCESS);
		if (frameBufferCreationResult != VK_SUCCESS)
            return false;

        result[i] = frameBuffer;
        
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
            g_logger->log(Vulkan::Logger::Level::Error, std::string("Failed to create shader module for file ") + shader._filename + " with error " + std::to_string(result) + "\n");
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

    std::vector<VkDescriptorSetLayout> layouts;
    effect.collectDescriptorSetLayouts(layouts);
    pipelineLayoutCreateInfo.setLayoutCount = (uint32_t)layouts.size();
    pipelineLayoutCreateInfo.pSetLayouts = layouts.empty() ? VK_NULL_HANDLE :  &layouts[0];
    pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
    pipelineLayoutCreateInfo.pPushConstantRanges = nullptr;

    VkResult createPipelineLayoutResult = vkCreatePipelineLayout(context._device, &pipelineLayoutCreateInfo, nullptr, &effect._pipelineLayout);
    assert(createPipelineLayoutResult == VK_SUCCESS);
    if (createPipelineLayoutResult != VK_SUCCESS)
    {
        g_logger->log(Vulkan::Logger::Level::Error, std::string("Failed to create pipeline layout\n"));
        return false;
    }
    createInfo.layout = effect._pipelineLayout;
    createInfo.basePipelineHandle = effect._pipeline;
    createInfo.basePipelineIndex = 0;

    VkComputePipelineCreateInfoDescriptor createDescriptor;
    createDescriptor._createInfo = createInfo;
    computePipelineCreationCallback(createDescriptor);

    const VkResult createComputePipelineResult = vkCreateComputePipelines(context._device, VK_NULL_HANDLE, 1, &createDescriptor._createInfo, VK_NULL_HANDLE, &effect._pipeline);
    assert(createComputePipelineResult == VK_SUCCESS);
    if (createComputePipelineResult != VK_SUCCESS)
    {
        g_logger->log(Vulkan::Logger::Level::Error, std::string("Failed to create vulkan compute pipeline\n"));
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
    multisamplingCreateInfo.rasterizationSamples = (VkSampleCountFlagBits)appDesc._actualNumSamples;
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
    createInfo.pDepthStencilState = &depthStencilCreateInfo;

    VkPipelineColorBlendAttachmentState colorBlendAttachmentCreateInfo;
    memset(&colorBlendAttachmentCreateInfo, 0, sizeof(VkPipelineColorBlendAttachmentState));
    colorBlendAttachmentCreateInfo.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachmentCreateInfo.blendEnable = VK_FALSE;
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

    VkPushConstantRange pushConstantRange = {0};


    std::vector<VkDescriptorSetLayout> layouts;
    effect.collectDescriptorSetLayouts(layouts);
    pipelineLayoutCreateInfo.setLayoutCount = (uint32_t)layouts.size();
//    pipelineLayoutCreateInfo.setLayoutCount = (uint32_t)effect._descriptorSetLayouts.size();
    pipelineLayoutCreateInfo.pSetLayouts = &layouts[0];
    pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
    pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;

    createInfo.renderPass = effect._renderPass;
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
    dynamicStateCreateInfo.dynamicStateCount = 2;
//    dynamicStateCreateInfo.pDynamicStates = nullptr;
    dynamicStateCreateInfo.pDynamicStates = &dynamicState[0];
    createInfo.pDynamicState = &dynamicStateCreateInfo;

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
        vertexInputAttributeDescription,
        pushConstantRange
    );
    graphicsPipelineCreationCallback(graphicsPipelineCreateInfoDescriptor);

    // we can't set these variables until after the callback, as the vectors are dynamic in size, and the pointer to the contents might change
    vertexInputInfo.vertexBindingDescriptionCount = (uint32_t)vertexInputBindingDescription.size();
    vertexInputInfo.pVertexBindingDescriptions = &vertexInputBindingDescription[0];
    vertexInputInfo.vertexAttributeDescriptionCount = (uint32_t)vertexInputAttributeDescription.size();
    vertexInputInfo.pVertexAttributeDescriptions = &vertexInputAttributeDescription[0];

    VkResult createPipelineLayoutResult = vkCreatePipelineLayout(context._device, &pipelineLayoutCreateInfo, nullptr, &effect._pipelineLayout);
    assert(createPipelineLayoutResult == VK_SUCCESS);
    if (createPipelineLayoutResult != VK_SUCCESS)
    {
        g_logger->log(Vulkan::Logger::Level::Error, std::string("Failed to create graphics pipeline layout\n"));
        return false;
    }

    createInfo.layout = effect._pipelineLayout;

    const VkResult createGraphicsPipelineResult = vkCreateGraphicsPipelines(context._device, VK_NULL_HANDLE, 1, &createInfo, nullptr, &effect._pipeline);
    assert(createGraphicsPipelineResult == VK_SUCCESS);
    if (createGraphicsPipelineResult != VK_SUCCESS)
    {
        g_logger->log(Vulkan::Logger::Level::Error, std::string("Failed to create vulkan graphics pipeline\n"));
        return false;
    }

    return true;
}

bool Vulkan::createDescriptorSetLayout(Context & context, Vulkan::EffectDescriptor& effect)
{
    if (effect._uniforms.empty())
        return true;

    std::vector<VkDescriptorSetLayoutBinding> layouts;

    for (size_t i = 0; i < effect._uniforms.size(); i++)
    {

        Uniform& uniform = effect._uniforms[i];

        VkDescriptorSetLayoutBinding descriptorSetLayoutBinding = {};
        descriptorSetLayoutBinding.binding = (uint32_t)uniform._binding;
        descriptorSetLayoutBinding.descriptorCount = 1;
        descriptorSetLayoutBinding.descriptorType = uniform._type;
        descriptorSetLayoutBinding.pImmutableSamplers = nullptr;
        bool found = false;

        for (Vulkan::ShaderStage stage : uniform._stages)
        {
            descriptorSetLayoutBinding.stageFlags |= mapFromShaderStage(stage);
        }

        layouts.push_back(descriptorSetLayoutBinding);

    }

    VkDescriptorSetLayoutCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    createInfo.bindingCount = (uint32_t)layouts.size();
    createInfo.pBindings = layouts.empty() ? nullptr : &layouts[0];

    VkResult creationResult = vkCreateDescriptorSetLayout(context._device, &createInfo, nullptr, &effect._descriptorSetLayout);
    assert(creationResult == VK_SUCCESS);
    if (creationResult != VK_SUCCESS)
        return false;

    return true;

}



bool Vulkan::createCommandPools(Context & context)
{
    bool success = true;
    context._commandPools.resize(context._numQueueFamilies);
    for (unsigned int i = 0; i < context._numQueueFamilies; i++)
    {
        context._commandPools[i] = VK_NULL_HANDLE;

        VkCommandPoolCreateInfo createInfo;
        memset(&createInfo, 0, sizeof(VkCommandPoolCreateInfo));
        createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        createInfo.queueFamilyIndex = i;
        createInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        VkCommandPool commandPool;
        const VkResult createCommandPoolResult = vkCreateCommandPool(context._device, &createInfo, nullptr, &commandPool);
        assert(createCommandPoolResult == VK_SUCCESS);
        if (createCommandPoolResult != VK_SUCCESS)
        {
            g_logger->log(Vulkan::Logger::Level::Error, std::string("Failed to create command pool for queueFamily= \n") + std::to_string(i));
            success = false;
            continue;
        }
        context._commandPools[i] = commandPool;
    }
    return success;
}

bool Vulkan::resetCommandBuffer(Context& context, VkCommandBuffer& commandBuffer, unsigned int index)
{
    if (!context._fences.empty())
    {
        const VkResult waitForFencesResult = vkWaitForFences(context._device, 1, &context._fences[index], VK_TRUE, std::numeric_limits<uint64_t>::max());
    }

    VkCommandBufferResetFlags resetFlags = VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT;
    const VkResult resetCommandBufferResult = vkResetCommandBuffer(commandBuffer, resetFlags);
    assert(resetCommandBufferResult == VK_SUCCESS);
    if (resetCommandBufferResult != VK_SUCCESS)
    {
        g_logger->log(Vulkan::Logger::Level::Error, std::string("Call to vkResetCommandBuffer failed, i=") + std::to_string(index) + "\n");
        return false;
    }
    return true;
}

bool Vulkan::resetCommandBuffers(Context & context, std::vector<VkCommandBuffer>& commandBuffers)
{
    for (unsigned int i = 0; i < (unsigned int)commandBuffers.size(); i++)
    {
        if (!resetCommandBuffer(context, commandBuffers[i], i))
            return false;
    }

    return true;
}

VkFence Vulkan::createFence(VkDevice device, VkFenceCreateFlags flags)
{
    VkFenceCreateInfo createInfo;
    memset(&createInfo, 0, sizeof(VkFenceCreateInfo));
    createInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    createInfo.flags = flags;

    VkFence result = VK_NULL_HANDLE;
    const VkResult createFenceResult = vkCreateFence(device, &createInfo, nullptr, &result);
    assert(createFenceResult == VK_SUCCESS);
    if (createFenceResult != VK_SUCCESS)
    {
        g_logger->log(Vulkan::Logger::Level::Error, std::string("Failed to create fences\n"));
    }
    return result;
}

// VK_FENCE_CREATE_SIGNALED_BIT context._frameBuffers.size()
std::vector<VkFence> Vulkan::createFences(VkDevice device, unsigned int count, VkFenceCreateFlags flags)
{
    std::vector<VkFence> fences;
    for(unsigned int i=0 ; i < count ; i++)
    {
        VkFence result = createFence(device, flags);
        if (result == VK_NULL_HANDLE)
        {
            g_logger->log(Vulkan::Logger::Level::Error, std::string("Failed to create fence (") + std::to_string(i) + ")\n");
            return std::vector<VkFence>();
        }
        fences.push_back(result);
    }
    return fences;
    
}

std::vector<VkSemaphore> Vulkan::createSemaphores(Context & context)
{
    std::vector<VkSemaphore> semaphores(getNumInflightFrames(context));
    for(unsigned int i=0 ; i < (unsigned int)semaphores.size() ; i++)
    {
        VkSemaphoreCreateInfo createInfo;
        memset(&createInfo, 0, sizeof(VkSemaphoreCreateInfo));
        createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        
		const VkResult createSemaphoreResult = vkCreateSemaphore(context._device, &createInfo, nullptr, &semaphores[i]);
		assert(createSemaphoreResult == VK_SUCCESS);
        if(createSemaphoreResult != VK_SUCCESS)
        {
            g_logger->log(Vulkan::Logger::Level::Error, std::string("Failed to create semaphore (") + std::to_string(i) + ")\n");
            return std::vector<VkSemaphore>();
        }
    }
    return semaphores;
}

bool Vulkan::createSemaphores(AppDescriptor & appDesc, Context & context)
{
    context._imageAvailableSemaphores = createSemaphores(context);
    context._renderFinishedSemaphores = createSemaphores(context);
    context._fences = createFences(context._device, (unsigned int)getNumInflightFrames(context), VK_FENCE_CREATE_SIGNALED_BIT);
    
    return context._imageAvailableSemaphores.size() == context._renderFinishedSemaphores.size()
    && context._imageAvailableSemaphores.size() == context._fences.size()
    && !context._imageAvailableSemaphores.empty();
}

void Vulkan::destroySemaphores(Context& context)
{
    for (auto semaphore : context._imageAvailableSemaphores)
        vkDestroySemaphore(context._device, semaphore, nullptr);

    for(auto semaphore : context._renderFinishedSemaphores)
        vkDestroySemaphore(context._device, semaphore, nullptr);

    for (auto fence : context._fences)
        vkDestroyFence(context._device, fence, nullptr);

    context._imageAvailableSemaphores.clear();
    context._renderFinishedSemaphores.clear();
    context._fences.clear();

}

bool Vulkan::createBufferView(Context& context, VkBuffer buffer, VkFormat requiredFormat, VkDeviceSize size, VkDeviceSize offset, VkBufferViewCreateFlags flags, VkBufferView& result)
{
    VkBufferViewCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
    createInfo.pNext = NULL;
    createInfo.buffer = buffer;
    createInfo.format = requiredFormat;
    createInfo.offset = offset;
    createInfo.range = size;
    createInfo.flags = flags;
    VkResult success = vkCreateBufferView(context._device, &createInfo, NULL, &result);
    assert(success == VK_SUCCESS);
    if (success != VK_SUCCESS)
    {
        g_logger->log(Vulkan::Logger::Level::Error, std::string("Failed to create bufferView of Format ") + std::to_string(requiredFormat) + "\n");
        return false;
    }
    return true;
}


Vulkan::BufferDescriptorPtr Vulkan::createBuffer(Context& context, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties)
{
    Vulkan::BufferDescriptorPtr buffer(new Vulkan::BufferDescriptor());
    if (!createBuffer(context, size, usage, properties, *buffer))
        return Vulkan::BufferDescriptorPtr();

    return buffer;
}

Vulkan::PersistentBufferPtr Vulkan::lookupPersistentBuffer(Context& context, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, const std::string tag, int numBuffers)
{
    const int numInternalBuffers = (numBuffers <= 0) ? Vulkan::getNumInflightFrames(context) : numBuffers;
    const PersistentBufferKey key(numInternalBuffers, usage, properties, tag);
    PersistentBufferMap::iterator it = g_persistentBuffers.find(key);
    if (it != g_persistentBuffers.end())
        return it->second;
    return nullptr;
}


Vulkan::PersistentBufferPtr Vulkan::createPersistentBuffer(Context& context, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, const std::string tag, int numBuffers)
{
    auto createBuffers = [&context, usage, properties, numBuffers](Vulkan::PersistentBufferPtr pBuffer, unsigned int size) {
        for (unsigned int i = 0; i < pBuffer->_buffers.size(); i++)
        {
            if (!Vulkan::createBuffer(context, size, usage, properties, pBuffer->_buffers[i], &pBuffer->_allocInfos[i]))
                return false;
        }
        return true;
    };

    const int numInternalBuffers = (numBuffers <= 0) ? (int)Vulkan::getNumInflightFrames(context) : numBuffers;
    const PersistentBufferKey key(numInternalBuffers, usage, properties, tag);
    PersistentBufferMap::iterator it = g_persistentBuffers.find(key);
    if (it == g_persistentBuffers.end() || it->second->_registeredSize < (unsigned int)size)
    {
        Vulkan::PersistentBufferPtr pBuffer;
        if (it == g_persistentBuffers.end())
            pBuffer = Vulkan::PersistentBufferPtr(new Vulkan::PersistentBuffer(numInternalBuffers));
        else
            pBuffer = it->second;

        for (auto & buf : pBuffer->_buffers)
            destroyBufferDescriptor(buf);

        if(!createBuffers(pBuffer, (unsigned int)size))
            return Vulkan::PersistentBufferPtr();

        pBuffer->_registeredSize = (unsigned int)size;
        g_persistentBuffers[key] = pBuffer;
        return pBuffer;
    }
    else
        return it->second;
}


bool Vulkan::createBuffer(Context & context, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, BufferDescriptor & bufDesc, VmaAllocationInfo * aInfo)
{
    VkBufferCreateInfo createInfo;
    memset(&createInfo, 0, sizeof(VkBufferCreateInfo));
    createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    createInfo.size = size;
    createInfo.usage = usage;
    createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocCreateInfo = {};
    allocCreateInfo.flags = 0;
    allocCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    if (aInfo != nullptr)
        allocCreateInfo.flags |= VMA_ALLOCATION_CREATE_MAPPED_BIT;

    allocCreateInfo.requiredFlags = properties;
    allocCreateInfo.preferredFlags = properties;

    VkBuffer vertexBuffer;
    VmaAllocationInfo allocInfo = {};
    VmaAllocation allocation;
    const VkResult createBufferResult = vmaCreateBuffer(g_allocator, &createInfo, &allocCreateInfo, &vertexBuffer, &allocation, &allocInfo);
    assert(createBufferResult == VK_SUCCESS);
    if (createBufferResult != VK_SUCCESS)
    {
        g_logger->log(Vulkan::Logger::Level::Error, std::string("Failed to create vertex buffer\n"));
        return false;
    }

    bufDesc._buffer = vertexBuffer;
    bufDesc._memory = allocation;
    bufDesc._size = (unsigned int)createInfo.size;
    if (aInfo != nullptr) {
        *aInfo = allocInfo;
    }


    if (properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
        bufDesc._mappable = true;

    return true;
}

Vulkan::BufferDescriptorPtr Vulkan::createIndexOrVertexBufferAndCopyData(Context& context, const void* srcData, VkDeviceSize bufferSize, BufferType type) {
    Vulkan::BufferDescriptorPtr dstBuffer = createIndexOrVertexBuffer(context, bufferSize, type);
    if (dstBuffer != nullptr) {
        copyDataToIndexOrVertexBuffer(context, srcData, bufferSize, dstBuffer);
    }
    return dstBuffer;
}

void Vulkan::copyDataToIndexOrVertexBuffer(Context& context, const void* srcData, VkDeviceSize bufferSize, Vulkan::BufferDescriptorPtr dstBuffer) {
    VkDeviceSize amountLeftToCopy = bufferSize;
    VkDeviceSize dstOffset = 0;
    const char* srcDataP = (const char*)(srcData);
    while (amountLeftToCopy > 0)
    {
        Vulkan::PersistentBufferPtr stagingBuffer = getPersistentStagingBuffer(context, bufferSize);
        VkDeviceSize amountToCopy = std::min<VkDeviceSize>(stagingBuffer->_registeredSize, amountLeftToCopy);
        stagingBuffer->copyFromAndFlush(context, 0, (const void *)srcDataP, amountToCopy, 0);

        Context::Queue& queue = getQueue(context, VK_QUEUE_TRANSFER_BIT);
        dstBuffer->copyFromAndFlush(context, context._commandPools[queue._familyIndex], queue._queue, stagingBuffer->getBuffer(0), amountToCopy, 0, dstOffset);

        amountLeftToCopy -= amountToCopy;
        dstOffset += amountToCopy;
        srcDataP += amountToCopy;
    }

}


Vulkan::BufferDescriptorPtr Vulkan::createIndexOrVertexBuffer(Context & context, VkDeviceSize bufferSize, BufferType type)
{    
    Vulkan::BufferDescriptorPtr vertexBufferDescriptor = createBuffer(context, bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | ((type == BufferType::Vertex) ? VK_BUFFER_USAGE_VERTEX_BUFFER_BIT : VK_BUFFER_USAGE_INDEX_BUFFER_BIT), VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if(vertexBufferDescriptor==nullptr)
    {
        g_logger->log(Vulkan::Logger::Level::Error, std::string("Failed to create vertex buffer of size ") + std::to_string((int)bufferSize) + " bytes\n");
        return Vulkan::BufferDescriptorPtr();
    }

    return vertexBufferDescriptor;


    
    
}


bool Vulkan::createUniformBuffer(AppDescriptor & appDesc, Context & context, VkDeviceSize bufferSize, BufferDescriptor & result)
{
    BufferDescriptor uniforms;
    
    if (!createBuffer(context, bufferSize,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        uniforms))
    {
        g_logger->log(Vulkan::Logger::Level::Error, std::string("Failed to create uniform buffer of size=") + std::to_string((int)bufferSize) + "\n");
        return false;
    }

    result = uniforms;

    return true;
}

bool Vulkan::createDescriptorPool(Context & context, EffectDescriptor& effect)
{
	VkDescriptorPoolSize poolSizes[4] = {};
    int poolIndex = 0;
    if (effect.totalNumUniformBuffers() > 0)
    {
        poolSizes[poolIndex].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes[poolIndex].descriptorCount = static_cast<uint32_t>(effect.totalNumUniformBuffers() * Vulkan::getNumInflightFrames(context));
        poolIndex++;
    }

    if (effect.totalSamplerCount() > 0)
    {
        poolSizes[poolIndex].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSizes[poolIndex].descriptorCount = static_cast<uint32_t>(effect.totalSamplerCount() * Vulkan::getNumInflightFrames(context));
        poolIndex++;
    }

    if (effect.totalImagesCount() > 0)
    {
        poolSizes[poolIndex].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        poolSizes[poolIndex].descriptorCount = static_cast<uint32_t>(effect.totalImagesCount() * Vulkan::getNumInflightFrames(context));
        poolIndex++;
    }

    if (effect.totalTexelBufferCount() > 0)
    {
        poolSizes[poolIndex].type = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
        poolSizes[poolIndex].descriptorCount = static_cast<uint32_t>(effect.totalTexelBufferCount() * Vulkan::getNumInflightFrames(context));
        poolIndex++;
    }

	VkDescriptorPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = poolIndex;
	poolInfo.pPoolSizes = &poolSizes[0];
    poolInfo.maxSets = static_cast<uint32_t>(effect.totalNumUniforms() * Vulkan::getNumInflightFrames(context));

	VkResult creationResult = vkCreateDescriptorPool(context._device, &poolInfo, nullptr, &effect._descriptorPool);
	assert(creationResult == VK_SUCCESS);
	if (creationResult != VK_SUCCESS)
		return false;
        
	return true;
}

bool Vulkan::createDescriptorSet(AppDescriptor& appDesc, Context& context, EffectDescriptor& effect)
{
    if (effect._uniforms.empty())
        return true;


    const uint32_t framesCount = (uint32_t)Vulkan::getNumInflightFrames(context);

    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = effect._descriptorPool;
    allocInfo.descriptorSetCount = 1;

    const uint32_t uniformCount = effect.totalNumUniforms();
    static std::vector<Uniform*> uniforms(1);
    if (uniforms.size() < uniformCount)
        uniforms.resize(2 * (uniformCount + 1)); // amortise resizing

    std::vector<VkDescriptorSetLayout> descriptorSetLayouts(1, effect._descriptorSetLayout);
    allocInfo.pSetLayouts = &descriptorSetLayouts[0];

    static VkDeviceSize currentOffset = 0;
    for (Vulkan::Uniform& uniform : effect._uniforms)
    {
        uniform._offset = currentOffset;
        if(context._deviceProperties.limits.minUniformBufferOffsetAlignment != 0  && uniform._size!=0)
        {
            uint32_t offsetRemainder = uniform._size % context._deviceProperties.limits.minUniformBufferOffsetAlignment;
            uint32_t nearestSize = (offsetRemainder == 0 ) 
                ? uniform._size
                : uniform._size + context._deviceProperties.limits.minUniformBufferOffsetAlignment - offsetRemainder;
            currentOffset = currentOffset + nearestSize;
            assert(currentOffset % context._deviceProperties.limits.minUniformBufferOffsetAlignment == 0);
        }
        else
        {
            currentOffset += uniform._size;
        }
    }

    effect._descriptorSets.resize(framesCount);
    for (uint32_t frame = 0; frame < framesCount; frame++)
    {
        VkResult allocationResult = vkAllocateDescriptorSets(context._device, &allocInfo, &effect._descriptorSets[frame]);
        assert(allocationResult == VK_SUCCESS);
        if (allocationResult != VK_SUCCESS)
            return false;

        for (Vulkan::Uniform& uniform : effect._uniforms)
        {
            if (uniform._type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
            {
                VkDescriptorBufferInfo bufferInfo;
                memset(&bufferInfo, 0, sizeof(bufferInfo));
                bufferInfo.buffer = uniform._frames[frame]._buffer->getBuffer(0)._buffer;

                bufferInfo.offset = uniform._offset;
                bufferInfo.range = uniform._size;

                VkWriteDescriptorSet descriptorWrite = {};
                descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                descriptorWrite.dstSet = effect._descriptorSets[frame];
                descriptorWrite.dstBinding = uniform._binding;
                descriptorWrite.dstArrayElement = 0;
                descriptorWrite.descriptorType = uniform._type;
                descriptorWrite.descriptorCount = 1;
                descriptorWrite.pBufferInfo = &bufferInfo;

                vkUpdateDescriptorSets(context._device, 1, &descriptorWrite, 0, nullptr);
            }
        }

    }


    return true;
}

void Vulkan::updateUniforms(AppDescriptor & appDesc, Context & context, uint32_t currentImage)
{
   for(EffectDescriptorPtr & effect : context._potentialEffects)
    {
        static std::vector<unsigned char> updateData;
        const uint32_t uniformCount = effect->totalTypeCount(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        static std::vector<Uniform*> uniforms(1);
        if (uniforms.size() < uniformCount)
            uniforms.resize(2 * (uniformCount + 1)); // amortise resizing

        effect->collectUniformsOfType(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &uniforms[0]);
        for (unsigned int uniformIndex = 0; uniformIndex < uniformCount; uniformIndex++)
        {
            Uniform* uniform = uniforms[uniformIndex];
            unsigned int uniformUpdateSize = effect->_updateUniform(*uniform, updateData);
            if (uniformUpdateSize != 0)
                uniform->_frames[context._currentFrame]._buffer->copyFrom(0, reinterpret_cast<const void*>(&updateData[0]), uniformUpdateSize, uniform->_offset);
        }


        if (/*!effect->getRerecordNeeded(context._currentFrame) || */effect->_recordCommandBuffers(appDesc, context, *effect))
            context._frameReadyEffects.push_back(effect);
    }
    
}

void Vulkan::destroyMesh(Context & context, Mesh& mesh)
{
//	destroyBufferDescriptor(context, mesh._vertexBuffer);
//	destroyBufferDescriptor(context, mesh._indexBuffer);
//	for(auto uniformBuffer : mesh._uniformBuffers)
//		destroyBufferDescriptor(context, uniformBuffer);
//	mesh._uniformBuffers.clear();

//	vkDestroyDescriptorPool(context._device, mesh._descriptorPool, VK_NULL_HANDLE);
//	mesh._descriptorPool = VK_NULL_HANDLE;

//	mesh._descriptorSets.clear();

}

void Vulkan::destroyBufferDescriptor(BufferDescriptor& descriptor)
{
    vmaDestroyBuffer(g_allocator, descriptor._buffer, descriptor._memory);
}

void Vulkan::destroyImage(ImageDescriptor& descriptor)
{
    vmaDestroyImage(g_allocator, descriptor._image, descriptor._memory);
}


void Vulkan::clearMeshes(Context & context, EffectDescriptor & effect)
{
	resetCommandBuffers(context, effect._commandBuffers);
//		destroyMesh(context, *mesh);

}

bool Vulkan::initializeIndexAndVertexBuffers(AppDescriptor & appDesc, 
    Context & context, 
    std::vector<unsigned char> & vertexData, 
    std::vector<unsigned char> & indexData, 
    void * userData, 
    bool alwaysReallocate,
    Vulkan::Mesh & result)
{
    if (!indexData.empty())
    {
        BufferDescriptorPtr indexBuffer = std::dynamic_pointer_cast<BufferDescriptor>(result.getIndexBuffer());
        if (alwaysReallocate || indexBuffer == nullptr || indexBuffer->_size < indexData.size()) {
            indexBuffer = createIndexOrVertexBuffer(context, indexData.size(), BufferType::Index);
            result.setIndexBuffer(indexBuffer);
        }

        if (indexBuffer == nullptr) {
            g_logger->log(Vulkan::Logger::Level::Error, std::string("Failed to create index buffer\n"));
            return false;
        }

        // index buffer
        const void* data = indexData.data();
        const VkDeviceSize bufferSize = indexData.size();

        copyDataToIndexOrVertexBuffer(context, data, bufferSize, indexBuffer);
    }

    if (!vertexData.empty()) {
        BufferDescriptorPtr vertexBuffer = std::dynamic_pointer_cast<BufferDescriptor>(result.getVertexBuffer());
        if (alwaysReallocate || vertexBuffer == nullptr || vertexBuffer->_size < vertexData.size()) {
            vertexBuffer = createIndexOrVertexBuffer(context, vertexData.size(), BufferType::Vertex);
            result.setVertexBuffer(vertexBuffer);
        }

        if (vertexBuffer == nullptr) {
            g_logger->log(Vulkan::Logger::Level::Error, std::string("Failed to create vertex buffer\n"));
            return false;
        }

        // index buffer
        const void* data = vertexData.data();
        const VkDeviceSize bufferSize = vertexData.size();

        copyDataToIndexOrVertexBuffer(context, data, bufferSize, vertexBuffer);
    }

    if (!indexData.empty()) // this is an indexed mesh
        result._numIndices = (unsigned int)indexData.size() / sizeof(uint16_t);
    else
        result._numIndices = 0; // this mesh probably doesn't have any indices - but we have no way of knowing the number of vertices from here

    result._userData = userData;

    return true;

}

namespace
{
    std::array<VkDeviceSize, 10> memoryTypes = {};
    VkDeviceSize totalMem = 0;
    void allocateRecord(
        VmaAllocator VMA_NOT_NULL                    allocator,
        uint32_t                                     memoryType,
        VkDeviceMemory VMA_NOT_NULL_NON_DISPATCHABLE memory,
        VkDeviceSize                                 size,
        void* VMA_NULLABLE                           pUserData)
    {
        totalMem += size;
        memoryTypes[memoryType] += size;
        std::string info = std::string("AllocateGPU: memoryType=") + std::to_string(memoryType) + ", thisAllocation=" + std::to_string(size) + ", MemUsageOfThisType=" + std::to_string(memoryTypes[memoryType]) + ", totalMemUsage=" + std::to_string(totalMem) + " ,megaByteTotal=" + std::to_string(totalMem/(1024*1024)) + "\n";
        g_logger->log(Vulkan::Logger::Level::Verbose, info);
        if (memoryType == 2)
        {
            int k = 0;
            k = 1;
        }
    }

    void deallocateRecord(
        VmaAllocator VMA_NOT_NULL                    allocator,
        uint32_t                                     memoryType,
        VkDeviceMemory VMA_NOT_NULL_NON_DISPATCHABLE memory,
        VkDeviceSize                                 size,
        void* VMA_NULLABLE                           pUserData)
    {
        totalMem -= size;
        memoryTypes[memoryType] -= size;
        std::string info = std::string("DeallocateGPU: memoryType=") + std::to_string(memoryType) + ", thisAllocation=" + std::to_string(size) + ", MemUsageOfThisType=" + std::to_string(memoryTypes[memoryType]) + ", totalMemUsage=" + std::to_string(totalMem) + " ,megaByteTotal=" + std::to_string(totalMem / (1024 * 1024)) + "\n";
        g_logger->log(Vulkan::Logger::Level::Verbose, info);
    }
}

bool Vulkan::setupAllocator(AppDescriptor& appDesc, Context& context)
{
    VmaVulkanFunctions vulkanFunctions = {};
    vulkanFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
    vulkanFunctions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

    static VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice = context._physicalDevice;
    allocatorInfo.device = context._device;
    allocatorInfo.instance = context._instance;
    allocatorInfo.preferredLargeHeapBlockSize = 0;
    allocatorInfo.flags = 0;
    allocatorInfo.vulkanApiVersion = 0;
    allocatorInfo.pVulkanFunctions = &vulkanFunctions;
    allocatorInfo.vulkanApiVersion = appDesc._requiredVulkanVersion;

    // this is for debug info only
    if (validationLayersEnabled)
    {
        VmaDeviceMemoryCallbacks* callbacks = new VmaDeviceMemoryCallbacks();
        callbacks->pfnAllocate = allocateRecord;
        callbacks->pfnFree = deallocateRecord;
        allocatorInfo.pDeviceMemoryCallbacks = callbacks;
    }

//    allocatorInfo.flags = VMA_ALLOCATOR_CREATE_EXTERNALLY_SYNCHRONIZED_BIT;
//    if (appDesc.hasExtension("VK_EXT_memory_budget"))
//        allocatorInfo.flags |= VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;

    vmaCreateAllocator(&allocatorInfo, &g_allocator);
    return true;
}

bool Vulkan::createInstance(AppDescriptor& appDesc, Context& context, bool enableValidationLayers)
{
    Vulkan::validationLayersEnabled = enableValidationLayers;

    if (volkInitialize() != VK_SUCCESS) {
        g_logger->log(Vulkan::Logger::Level::Error, std::string("Volk failed to initialize the Vulkan library\n"));
        return false;
    }
 
    if (!createInstanceAndLoadExtensions(appDesc, context)) {
        g_logger->log(Vulkan::Logger::Level::Error, std::string("Failed to create instance and load extensions\n"));
        return false;
    }

    volkLoadInstance(context._instance);

    if (validationLayersEnabled && !setupDebugCallback(context))
    {
        g_logger->log(Vulkan::Logger::Level::Error, std::string("Failed to setup requested debug callback\n"));
    }

    return true;
}

bool Vulkan::handleVulkanSetup(AppDescriptor & appDesc, Context & context)
{
    
    if (!createVulkanSurface(appDesc._window, context)) {
        g_logger->log(Vulkan::Logger::Level::Error, std::string("Failed to create vulkan surface\n"));
        return false;
    }
    
    if (!enumeratePhysicalDevices(appDesc, context)) {
        g_logger->log(Vulkan::Logger::Level::Error, std::string("Failed to enumerate and choose device\n"));
        return false;
    }
    
    // choose a gpu
    if (!choosePhysicalDevice(appDesc, context)) {
        g_logger->log(Vulkan::Logger::Level::Error, std::string("Failed to choose appropriate physical device\n"));
        return false;
    }
    
    // load extensions
    if (!lookupDeviceExtensions(appDesc))
    {
        g_logger->log(Vulkan::Logger::Level::Error, std::string("Failed to enumerate device extensions!\n"));
        return false;
    }
    
    if (!createDevice(appDesc, context))
    {
        g_logger->log(Vulkan::Logger::Level::Error, std::string("Failed to create device!\n"));
        return false;
    }

    if (!setupAllocator(appDesc, context))
    {
        g_logger->log(Vulkan::Logger::Level::Error, std::string("Failed to setup allocator!\n"));
        return false;
    }

    // create standard command pool
    if (!createCommandPools(context))
    {
        g_logger->log(Vulkan::Logger::Level::Error, std::string("Failed to create all the standard command pools\n"));
        return false;
    }

    if (!createSwapChainDependents(appDesc, context))
    {
        g_logger->log(Vulkan::Logger::Level::Error, std::string("Failed to create and setup swap chain!\n"));
        return false;
    }
    	
	if (!createPipelineCache(appDesc, context))
	{
        g_logger->log(Vulkan::Logger::Level::Warn, std::string("Failed to create pipeline cache. This is non-fatal.\n"));
	}
	
        
    return true;
}

bool Vulkan::createSwapChainDependents(AppDescriptor & appDesc, Context & context)
{
	if (!createSwapChain(appDesc, context))
	{
        g_logger->log(Vulkan::Logger::Level::Error, std::string("Failed to create and setup swap chain!\n"));
		return false;
	}

    if (!createRenderPass(context, appDesc._actualNumSamples, &context._renderPass, [](Vulkan::VkRenderPassCreateInfoDescriptor&) {}))
    {
        g_logger->log(Vulkan::Logger::Level::Error, std::string("Failed to create standard render pass\n"));
        return false;
    }
   
    if (!createColorBuffers(context))
	{
        g_logger->log(Vulkan::Logger::Level::Error, std::string("Failed to create color buffers\n"));
		return false;
	}

    if (!createDepthBuffers(context, appDesc._actualNumSamples, context._swapChainSize, context._depthImages, context._depthImageViews))
    {
        g_logger->log(Vulkan::Logger::Level::Error, std::string("Failed to create depth buffers\n"));
		return false;
	}

    if (appDesc._actualNumSamples > 1)
    {
        const unsigned int width = context._swapChainSize.width;
        const unsigned int height = context._swapChainSize.height;
        const unsigned int numSwapBuffers = (unsigned int)Vulkan::getNumInflightFrames(context);
        context._msaaColourImages.resize(numSwapBuffers);
        context._msaaColourImageViews.resize(numSwapBuffers);
        for (unsigned int i = 0; i < numSwapBuffers; i++)
        {
            if (!createImage(
                context, 
                nullptr,
                4, 
                width, 
                height, 
                1, 
                appDesc._actualNumSamples,
                context._surfaceFormat.format, 
                context._msaaColourImages[i], 
                1,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL))
            {
                g_logger->log(Vulkan::Logger::Level::Error, std::string("Failed to create msaa image ") + std::to_string(i) + "\n");
                return false;
            }

            if (!createImageView(context, context._msaaColourImages[i]._image, context._surfaceFormat.format, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_VIEW_TYPE_2D, context._msaaColourImageViews[i]))
            {
                g_logger->log(Vulkan::Logger::Level::Error, std::string("Failed to create msaa image view ") + std::to_string(i) + "\n");
                return false;
            }
        }
    }

	if (!createFrameBuffers(context._device, context._swapChainSize, context._renderPass, context._swapChainImageViews, context._msaaColourImageViews, context._depthImageViews, context._frameBuffers))
	{
        g_logger->log(Vulkan::Logger::Level::Error, std::string("Failed to create frame buffers\n"));
		return false;
	}

    if (!createSemaphores(appDesc, context))
    {
        g_logger->log(Vulkan::Logger::Level::Error, std::string("Failed to create frame semaphores\n"));
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
        depthImage.destroy();
	context._depthImages.clear();

	for (auto frameBuffer : context._frameBuffers)
		vkDestroyFramebuffer(device, frameBuffer, nullptr);
	context._frameBuffers.clear();

	for (auto imageView : context._swapChainImageViews)
		vkDestroyImageView(device, imageView, nullptr);
	context._swapChainImageViews.clear();

    for (auto msaaColourImageView : context._msaaColourImageViews)
        vkDestroyImageView(device, msaaColourImageView, nullptr);
    context._msaaColourImageViews.clear();

    for (auto msaaColourImage : context._msaaColourImages)
        msaaColourImage.destroy();
    context._msaaColourImages.clear();


/*	for (auto image : context._colorBuffers)
		vkDestroyImage(device, image, nullptr);
	context._colorBuffers.clear();*/

	vkDestroySwapchainKHR(device, context._swapChain, nullptr);
	context._swapChain = VK_NULL_HANDLE;

    vkDestroyRenderPass(device, context._renderPass, nullptr);
    context._renderPass = VK_NULL_HANDLE;

    destroySemaphores(context);

	return true;
}

bool Vulkan::initEffectDescriptor(AppDescriptor& appDesc, Context& context, unsigned int queueFlagBits, Vulkan::EffectDescriptor& effect)
{
    effect._queueFlagBits = queueFlagBits;
    if (!createDescriptorSetLayout(context, effect))
    {
        g_logger->log(Vulkan::Logger::Level::Error, std::string("Failed to create descriptor set layouts!\n"));
        return false;
    }

    if (effect._descriptorPool==VK_NULL_HANDLE && !createDescriptorPool(context, effect))
    {
        g_logger->log(Vulkan::Logger::Level::Error, std::string("Failed to create descriptor pool\n"));
        return false;
    }

    if (!createDescriptorSet(appDesc, context, effect))
    {
        g_logger->log(Vulkan::Logger::Level::Error, std::string("Failed to create descriptor set\n"));
        return false;
    }

    Vulkan::Context::Queue & queue = getQueue(context, queueFlagBits);
    if (!createCommandBuffers(context, context._commandPools[queue._familyIndex], Vulkan::getNumInflightFrames(context), &effect._commandBuffers))
    {
        g_logger->log(Vulkan::Logger::Level::Error, std::string("Failed to create command buffers\n"));
        return false;
    }

    if (!effect._shaderModules.empty())
    {
        bool createShaderModulesSuccess = Vulkan::createShaderModules(appDesc, context, effect._shaderModules);
        if (!createShaderModulesSuccess)
        {
            g_logger->log(Vulkan::Logger::Level::Error, std::string("Failed to create shader modules\n"));
            return false;
        }
    }

    effect._recordCommandsNeeded.resize(Vulkan::getNumInflightFrames(context));
    effect.setRerecordNeeded();

    return true;
}

bool Vulkan::initEffectDescriptor(AppDescriptor& appDesc, Context& context, unsigned int queueFlagBits, ComputePipelineCustomizationCallback computePipelineCreationCallback, Vulkan::EffectDescriptor& effect)
{
    effect._computePipelineCreationCallback = computePipelineCreationCallback;

    if (!initEffectDescriptor(appDesc, context, queueFlagBits, effect))
    {
        g_logger->log(Vulkan::Logger::Level::Error, std::string("Failed to create pipeline\n"));
        return false;
    }

    if (!createComputePipeline(appDesc, context,  computePipelineCreationCallback, effect))
    {
        g_logger->log(Vulkan::Logger::Level::Error, std::string("Failed to create compute pipeline\n"));
        return false;
    }

    return true;
}

bool Vulkan::initEffectDescriptor(AppDescriptor& appDesc, 
    Context& context, 
    unsigned int queueFlagBits,
    const bool createPipeline,
    GraphicsPipelineCustomizationCallback graphicsPipelineCreationCallback, 
    RenderPassCustomizationCallback renderPassCreationCallback,
    Vulkan::EffectDescriptor& effect)
{
    effect._createPipeline = createPipeline;
    effect._graphicsPipelineCreationCallback = graphicsPipelineCreationCallback;
    effect._renderPassCreationCallback = renderPassCreationCallback;

    if (!initEffectDescriptor(appDesc, context, queueFlagBits, effect))
    {
        g_logger->log(Vulkan::Logger::Level::Error, std::string("Failed to create pipeline\n"));
        return false;
    }

    if (!createRenderPass(context, appDesc._actualNumSamples, &effect._renderPass, renderPassCreationCallback))
    {
        g_logger->log(Vulkan::Logger::Level::Error, std::string("Failed to create render pass for effect\n"));
        return false;
    }


    if(createPipeline && !createGraphicsPipeline(appDesc, context, graphicsPipelineCreationCallback, effect))
    {
        g_logger->log(Vulkan::Logger::Level::Error, std::string("Failed to create graphics pipeline\n"));
        return false;
    }

    return true;
}

bool Vulkan::recreateEffectDescriptor(AppDescriptor& appDesc, Context& context, EffectDescriptorPtr effect)
{
    vkDestroyRenderPass(context._device, effect->_renderPass, nullptr);

    if(effect->_pipelineLayout != nullptr)
        vkDestroyPipelineLayout(context._device, effect->_pipelineLayout, nullptr);
    effect->_pipelineLayout = nullptr;

    if(effect->_pipeline != nullptr)
        vkDestroyPipeline(context._device, effect->_pipeline, nullptr);
    effect->_pipeline = nullptr;

    // compute or graphics pipeline???
    if (effect->_graphicsPipelineCreationCallback != nullptr)
    {
        if (!createRenderPass(context, appDesc._actualNumSamples, &effect->_renderPass, effect->_renderPassCreationCallback))
        {
            g_logger->log(Vulkan::Logger::Level::Error, std::string("Failed to recreate render pass for effect\n"));
            return false;
        }


        if (effect->_createPipeline && !createGraphicsPipeline(appDesc, context, effect->_graphicsPipelineCreationCallback, *effect))
        {
            g_logger->log(Vulkan::Logger::Level::Error, std::string("Failed to recreate graphics pipeline\n"));
            return false;
        }
    }
    else if (effect->_computePipelineCreationCallback != nullptr)
    {
        if (!createComputePipeline(appDesc, context, effect->_computePipelineCreationCallback, *effect))
        {
            g_logger->log(Vulkan::Logger::Level::Error, std::string("Failed to recreate compute pipeline\n"));
            return false;
        }
    }
    return true;
}


bool Vulkan::createSampler(Vulkan::Context& context, VkSampler& sampler, VkSamplerCreateInfo & samplerCreateInfo)
{
    VkResult createSamplerResult = vkCreateSampler(context._device, &samplerCreateInfo, VK_NULL_HANDLE, &sampler);
    assert(createSamplerResult == VK_SUCCESS);

    return createSamplerResult == VK_SUCCESS;
}

bool Vulkan::createSampler(Vulkan::Context& context, VkSampler& sampler)
{
    VkSamplerCreateInfo samplerCreateInfo = { };
    samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
    samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
    samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerCreateInfo.anisotropyEnable = VK_FALSE;
    samplerCreateInfo.maxAnisotropy = 1;
    samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
    samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;
    samplerCreateInfo.compareEnable = VK_FALSE;
    samplerCreateInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerCreateInfo.mipLodBias = 0;
    samplerCreateInfo.minLod = 0.0f;
    samplerCreateInfo.maxLod = 0.0f;

    return Vulkan::createSampler(context, sampler, samplerCreateInfo);
}

VkCommandBuffer Vulkan::createCommandBuffer(Vulkan::Context& context, VkCommandPool commandPool, bool beginCommandBuffer)
{
    VkCommandBuffer commandBuffer;

    VkCommandBufferAllocateInfo commandBufferAllocateInfo;
    memset(&commandBufferAllocateInfo, 0, sizeof(VkCommandBufferAllocateInfo));
    commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocateInfo.commandPool = commandPool;
    commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferAllocateInfo.commandBufferCount = 1;
    const VkResult allocateCommandBuffersResult = vkAllocateCommandBuffers(context._device, &commandBufferAllocateInfo, &commandBuffer);
    assert(allocateCommandBuffersResult == VK_SUCCESS);
    if (allocateCommandBuffersResult != VK_SUCCESS)
    {
        g_logger->log(Vulkan::Logger::Level::Error, std::string("Failed to create VkCommandbufferAllocateInfo\n"));
        return commandBuffer;
    }

    if (beginCommandBuffer)
    {
        VkCommandBufferBeginInfo beginInfo;
        memset(&beginInfo, 0, sizeof(beginInfo));
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        beginInfo.pInheritanceInfo = NULL;
        beginInfo.pNext = NULL;

        const VkResult beginCommandResult = vkBeginCommandBuffer(commandBuffer, &beginInfo);
        if (beginCommandResult != VK_SUCCESS)
        {
            g_logger->log(Vulkan::Logger::Level::Error, std::string("Failed to call vkBeginCommandBuffer\n"));
            return commandBuffer;
        }
    }

    return commandBuffer;
}

void Vulkan::setLogger(Vulkan::Logger * logger) 
{ 
    g_logger = logger; 
}

uint32_t Vulkan::maxAASamples(Context& context)
{
    if (context._physicalDevice == VK_NULL_HANDLE)
        return 1;

    const uint32_t dAA = static_cast<uint32_t>(context._deviceProperties.limits.framebufferDepthSampleCounts);
    const uint32_t cAA = static_cast<uint32_t>(context._deviceProperties.limits.framebufferColorSampleCounts);
    return std::min(dAA, cAA);
}

uint32_t Vulkan::requestNumAASamples(Context& context, uint32_t count)
{
    const uint32_t maxAA = maxAASamples(context);
    const uint32_t log2AA = uint32_t(log2(maxAA));
    const uint32_t correctedValue = 1 << log2AA;
    const uint32_t rValue = std::min(count, correctedValue);
    return rValue;
}
