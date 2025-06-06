#include <stdexcept>
#include <cstring>

#include "buffer.hpp"
#include "commandbuffer.hpp"

namespace buffer
{

uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    /**
     * The VkPhysicalDeviceMemoryProperties structure has two arrays 
     * memoryTypes and memoryHeaps. 
     * Memory heaps are distinct memory resources like dedicated VRAM 
     * and swap space in RAM for when VRAM runs out. 
     * The different types of memory exist within these heaps. 
     * Right now we'll only concern ourselves with the type of memory 
     * and not the heap it comes from, but you can imagine 
     * that this can affect performance.
     */

    /**
     * The typeFilter parameter will be used to specify the bit field of memory types that are suitable. 
     * That means that we can find the index of a suitable memory type by simply iterating 
     * over them and checking if the corresponding bit is set to 1.
     */

    /**
     * However, we're not just interested in a memory type that is suitable for the vertex buffer. 
     * We also need to be able to write our vertex data to that memory. 
     * The memoryTypes array consists of VkMemoryType structs that specify the heap and properties
     * of each type of memory. The properties define special features of the memory, 
     * like being able to map it so we can write to it from the CPU. This property is indicated 
     * with VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, but we also need to use the 
     * VK_MEMORY_PROPERTY_HOST_COHERENT_BIT property. We'll see why when we map the memory
     */
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("failed to find suitable memory type!");
}

void bindBuffer(
    VkPhysicalDevice physicalDevice,
    VkDevice logicalDevice,
    VkDeviceSize size,
    VkBufferUsageFlags usage,
    VkMemoryPropertyFlags properties,
    VkBuffer& buffer,
    VkDeviceMemory& bufferMemory
) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    // like images in the swap chain vb can be owned by a specific queue family
    // or shared between multiple at the same time
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    // The flags parameter is used to configure sparse buffer memory, 
    // which is not relevant right now. 
    // We'll leave it at the default value of 0.
    bufferInfo.flags = 0;

    if (vkCreateBuffer(logicalDevice, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to create vertex buffer!");
    }

    // buffer has been created but no memory is assigned to it yet
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(logicalDevice, buffer, &memRequirements);
    
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(
        physicalDevice,
        memRequirements.memoryTypeBits, 
        properties
    );

    if (vkAllocateMemory(logicalDevice, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate vertex buffer memory!");
    }

    // memory allocation successful, so bind it to the buffer
    vkBindBufferMemory(logicalDevice, buffer, bufferMemory, 0);
}

void copyBuffer(
    VkDevice logicalDevice,
    VkCommandPool commandPool,
    VkQueue graphicsQueue,
    VkBuffer srcBuffer,
    VkBuffer dstBuffer,
    VkDeviceSize size
) {
    VkCommandBuffer commandBuffer = commandbuffer::beginSingleTimeCommands(
      logicalDevice,
      commandPool  
    );

    // we have to use regions array (of VkBufferCopy structs)
    // we can't use VK_WHOLE_SIZE like VkMapMemory
    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0; // Optional
    copyRegion.dstOffset = 0; // Optional
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

    commandbuffer::endAndExecuteSingleTimeCommands(
        logicalDevice,
        commandPool,
        graphicsQueue,
        commandBuffer
    );
}

void createUniformBuffers(
    VkPhysicalDevice physicalDevice,
    VkDevice logicalDevice,
    int maxFramesInFlight,
    std::vector<VkBuffer>& uniformBuffers,
    std::vector<VkDeviceMemory>& uniformBuffersMemory,
    std::vector<void*>& uniformBuffersMapped
) {
    VkDeviceSize bufferSize = sizeof(UniformBufferObject);

    uniformBuffers.resize(maxFramesInFlight);
    uniformBuffersMemory.resize(maxFramesInFlight);
    uniformBuffersMapped.resize(maxFramesInFlight);

    for (size_t i = 0; i < maxFramesInFlight; i++) {
        buffer::bindBuffer(
            physicalDevice,
            logicalDevice,
            bufferSize,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            uniformBuffers[i],
            uniformBuffersMemory[i]);

        // The buffer stays mapped the whole application time
        // as maping has a cost, it is best to avoid doing it every time
        // this is called "persistent mapping"
        vkMapMemory(logicalDevice, uniformBuffersMemory[i], 0, bufferSize, 0, &uniformBuffersMapped[i]);
    }
}

void createDescriptorPool(
    VkDevice logicalDevice,
    int maxFramesInFlight,
    VkDescriptorPool& descriptorPool
) {
    std::array<VkDescriptorPoolSize, 2> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    // we will allocate one descriptor set by frame
    poolSizes[0].descriptorCount = static_cast<uint32_t>(maxFramesInFlight);
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    // we will allocate one descriptor set by frame
    poolSizes[1].descriptorCount = static_cast<uint32_t>(maxFramesInFlight);

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());;
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = static_cast<uint32_t>(maxFramesInFlight);
    // We're not going to touch the descriptor set after creating it, so we don't need this flag
    // optional
    poolInfo.flags = 0;

    /**
     * Inadequate descriptor pools are a good example of a problem that 
     * the validation layers will not catch: As of Vulkan 1.1, vkAllocateDescriptorSets 
     * may fail with the error code VK_ERROR_POOL_OUT_OF_MEMORY if the pool is not sufficiently large, 
     * but the driver may also try to solve the problem internally. This means that sometimes (depending on 
     * hardware, pool size and allocation size) the driver will let us get away with an allocation that exceeds 
     * the limits of our descriptor pool. Other times, vkAllocateDescriptorSets will fail and return 
     * VK_ERROR_POOL_OUT_OF_MEMORY. This can be particularly frustrating if the allocation succeeds 
     * on some machines, but fails on others.
     * 
     * Since Vulkan shifts the responsiblity for the allocation to the driver, 
     * it is no longer a strict requirement to only allocate as many descriptors 
     * of a certain type (VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, etc.) 
     * as specified by the corresponding descriptorCount members for the creation of the descriptor pool. 
     * However, it remains best practise to do so, and in the future, VK_LAYER_KHRONOS_validation 
     * will warn about this type of problem if you enable Best Practice Validation.
     */

    if (vkCreateDescriptorPool(logicalDevice, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor pool!");
    }
}

void createDescriptorSetLayout(
    VkDevice logicalDevice,
    VkDescriptorSetLayout& descriptorSetLayout
) {
    // For uniform
    VkDescriptorSetLayoutBinding uboLayoutBinding{};
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    // with have only one MVP
    uboLayoutBinding.descriptorCount = 1;
    // in which shader stage it will be referenced
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    // only relevant for image sampling descriptors, optional
    uboLayoutBinding.pImmutableSamplers = nullptr;

    // For texture sampler
    VkDescriptorSetLayoutBinding samplerLayoutBinding{};
    samplerLayoutBinding.binding = 1;
    samplerLayoutBinding.descriptorCount = 1;
    // Combined image sampler descriptor allow shaders to access image resource
    // through a sampler
    samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerLayoutBinding.pImmutableSamplers = nullptr;
    // we intend to use the combined image descriptor sampler in the fragment shader
    // It is also possible to use texture sampling in the vertex shader, 
    // for example to dynamically deform a grid of vertices by a heightmap.
    samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    std::array<VkDescriptorSetLayoutBinding, 2> bindings = {uboLayoutBinding, samplerLayoutBinding};

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(logicalDevice, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor set layout!");
    }
}

void createDescriptorSets(
    VkDevice logicalDevice,
    int maxFramesInFlight,
    const std::vector<VkBuffer>& uniformBuffers,
    const VkDescriptorPool& descriptorPool,
    VkDescriptorSetLayout descriptorSetLayout,
    std::vector<VkDescriptorSet>& descriptorSets
) {
    // one descriptor set for each frame in flight,
    // all with the same layout
    std::vector<VkDescriptorSetLayout> layouts(maxFramesInFlight, descriptorSetLayout);
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(maxFramesInFlight);
    allocInfo.pSetLayouts = layouts.data();

    descriptorSets.resize(maxFramesInFlight);
    // this calls allocate descriptor sets, each with one buffer descriptor
    if (vkAllocateDescriptorSets(logicalDevice, &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate descriptor sets!");
    }

    // configure the descriptor sets we just allocated
    for (size_t i = 0; i < maxFramesInFlight; i++) {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = uniformBuffers[i];
        bufferInfo.offset = 0;
        // we could also use VK_WHOLE_SIZE here as
        // we overwrite the whole buffer
        bufferInfo.range = sizeof(buffer::UniformBufferObject);

        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = descriptorSets[i];
        descriptorWrite.dstBinding = 0;
        // descriptors could be array, here it is not
        // so the index is 0
        descriptorWrite.dstArrayElement = 0;
        // specify the type of decriptor ... again :(
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        // in case of an array, use only 1. Not it is starting at dstArrayElement
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pBufferInfo = &bufferInfo;
        descriptorWrite.pImageInfo = nullptr; // Optional
        descriptorWrite.pTexelBufferView = nullptr; // Optional

        // accepts two kind of array:
        // VkWriteDescriptorSet and an array of VkCopyDescriptorSet
        vkUpdateDescriptorSets(logicalDevice, 1, &descriptorWrite, 0, nullptr);
    }
}

void createDescriptorSets2(
    VkDevice logicalDevice,
    int maxFramesInFlight,
    const std::vector<VkBuffer>& uniformBuffers,
    const VkDescriptorPool& descriptorPool,
    VkDescriptorSetLayout descriptorSetLayout,
    const VkImageView& textureImageView,
    const VkSampler& textureSampler,
    std::vector<VkDescriptorSet>& descriptorSets
) {
    // one descriptor set for each frame in flight,
    // all with the same layout
    std::vector<VkDescriptorSetLayout> layouts(maxFramesInFlight, descriptorSetLayout);
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(maxFramesInFlight);
    allocInfo.pSetLayouts = layouts.data();

    descriptorSets.resize(maxFramesInFlight);
    // this calls allocate descriptor sets, each with one buffer descriptor
    if (vkAllocateDescriptorSets(logicalDevice, &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate descriptor sets!");
    }

    // configure the descriptor sets we just allocated
    for (size_t i = 0; i < maxFramesInFlight; i++) {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = uniformBuffers[i];
        bufferInfo.offset = 0;
        // we could also use VK_WHOLE_SIZE here as
        // we overwrite the whole buffer
        bufferInfo.range = sizeof(buffer::UniformBufferObject);

        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = textureImageView;
        imageInfo.sampler = textureSampler;

        std::array<VkWriteDescriptorSet, 2> descriptorWrites{};

        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = descriptorSets[i];
        descriptorWrites[0].dstBinding = 0;
        // descriptors could be array, here it is not
        // so the index is 0
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        // in case of an array, use only 1. Not it is starting at dstArrayElement
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &bufferInfo;

        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = descriptorSets[i];
        descriptorWrites[1].dstBinding = 1;
        // descriptors could be array, here it is not
        // so the index is 0
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        // in case of an array, use only 1. Not it is starting at dstArrayElement
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pImageInfo = &imageInfo;

        // accepts two kind of array:
        // VkWriteDescriptorSet and an array of VkCopyDescriptorSet
        vkUpdateDescriptorSets(logicalDevice, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
    }
}

}
