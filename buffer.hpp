#pragma once

#include <vector>

// Let GLFW include by itslef vulkan headers
#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"

#include "vertex.hpp"

namespace buffer
{

enum Type {
    Vertex,
    Index
};

/**
 * The data in the matrices is binary compatible with the way 
 * the shader expects it, so we can later just memcpy a UniformBufferObject to a VkBuffer.
 * 
 * alignas is to ensure proper memory alignment with Vulkan
 * here for the 3 matricies the default will be OK
 * but beeing explicit could avoid gotchas with more complicated
 * or nested structs
 */
struct UniformBufferObject {
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
};

void bindBuffer(
    VkPhysicalDevice physicalDevice,
    VkDevice logicalDevice,
    VkDeviceSize size,
    VkBufferUsageFlags usage,
    VkMemoryPropertyFlags properties,
    VkBuffer& buffer,
    VkDeviceMemory& bufferMemory
);

void copyBuffer(
    VkDevice logicalDevice,
    VkCommandPool commandPool,
    VkQueue graphicsQueue,
    VkBuffer srcBuffer,
    VkBuffer dstBuffer,
    VkDeviceSize size
);

template <class T>
void createBuffer(
    Type bufferType,
    VkPhysicalDevice physicalDevice,
    VkDevice logicalDevice,
    VkCommandPool commandPool,
    VkQueue graphicsQueue,
    const std::vector<T>& itemList,
    VkBuffer& buffer,
    VkDeviceMemory& bufferMemory
) {
    VkDeviceSize bufferSize = sizeof(itemList[0]) * itemList.size();

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;

    bindBuffer(
        physicalDevice,
        logicalDevice,
        bufferSize,
        // no more VK_BUFFER_USAGE_VERTEX_BUFFER_BIT as we are
        // creating a staging buffer
        // Buffer can be used as source in a memory transfer operation.
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer,
        stagingBufferMemory
    );

    // fill the staging buffer
    void* data;
    vkMapMemory(logicalDevice, stagingBufferMemory, 0, bufferSize, 0, &data);

    /**
     * Unfortunately the driver may not immediately copy the data 
     * into the buffer memory, for example because of caching. 
     * It is also possible that writes to the buffer are not visible 
     * in the mapped memory yet. There are two ways to deal with that problem:
     * * Use a memory heap that is host coherent, indicated with VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
     * * Call vkFlushMappedMemoryRanges after writing to the mapped memory, 
     * and call vkInvalidateMappedMemoryRanges before reading from the mapped memory
     * 
     * We went for the first approach, which ensures that the mapped memory always matches
     * the contents of the allocated memory. Do keep in mind that this may lead to slightly 
     * worse performance than explicit flushing.bufferSize
     * 
     * But now we are dealing with a staging buffer, TODO: does it matter with a staging buffer ?
     */
    memcpy(data, itemList.data(), static_cast<size_t>(bufferSize));
    vkUnmapMemory(logicalDevice, stagingBufferMemory);

    auto buffer_bit = (bufferType == Type::Vertex) ? VK_BUFFER_USAGE_VERTEX_BUFFER_BIT : VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

    bindBuffer(
        physicalDevice,
        logicalDevice,
        bufferSize,
        // vkMap usualy not possible as device local
        // hence we specify it can be used as a transfer destination
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | buffer_bit,
        // The most optimal memory on the GPU, but usually not accessible from the CPU
        // hence the use of a staging buffer
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        buffer,
        bufferMemory
    );

    copyBuffer(logicalDevice, commandPool, graphicsQueue, stagingBuffer, buffer, bufferSize);

    // we can now clean the staging buffer
    vkDestroyBuffer(logicalDevice, stagingBuffer, nullptr);
    vkFreeMemory(logicalDevice, stagingBufferMemory, nullptr);
}

void createUniformBuffers(
    VkPhysicalDevice physicalDevice,
    VkDevice logicalDevice,
    int maxFramesInFlight,
    std::vector<VkBuffer>& uniformBuffers,
    std::vector<VkDeviceMemory>& uniformBuffersMemory,
    std::vector<void*>& uniformBuffersMapped
);

}
