#include <stdexcept>
#include <cstring>

#include "buffer.hpp"

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
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    /**
     * You may wish to create a separate command pool for these kinds of short-lived buffers,
     * because the implementation may be able to apply memory allocation optimizations.
     * You should use the VK_COMMAND_POOL_CREATE_TRANSIENT_BIT flag during command pool
     * generation in that case.
     * 
     * TODO: create a command buffer, pool function
     * TODO: dedicated command pool for memory transfer
     * for now we use the command commandPool_ but WARNING: it has to be created
     * before calling this function, or it will SEGFAULT at vkAllocateCommandBuffers
     */
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(logicalDevice, &allocInfo, &commandBuffer);

    // start recording
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    // We're only going to use the command buffer once and wait with returning 
    // from the function until the copy operation has finished executing
    // good practice: tell the driver our intent
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    // we have to use regions array (of VkBufferCopy structs)
    // we can't use VK_WHOLE_SIzE like VkMapMemory
    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0; // Optional
    copyRegion.dstOffset = 0; // Optional
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

    // end recording
    vkEndCommandBuffer(commandBuffer);

    // execute the command buffer to complete the transfer
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    // there is no event to wait, unlike the draw command
    // we juste want to execute immediately
    // two way to wait for the transfer to complete:
    // * vkWaitForFence (would allow us to schedule multiple transfer simultaneously)
    // * vkQueueWaitIdle
    vkQueueWaitIdle(graphicsQueue);

    vkFreeCommandBuffers(logicalDevice, commandPool, 1, &commandBuffer);
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
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    // we will allocate one descriptor set by frame
    poolSize.descriptorCount = static_cast<uint32_t>(maxFramesInFlight);

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = static_cast<uint32_t>(maxFramesInFlight);
    // We're not going to touch the descriptor set after creating it, so we don't need this flag
    // optional
    poolInfo.flags = 0;

    if (vkCreateDescriptorPool(logicalDevice, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor pool!");
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

}