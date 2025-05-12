#include <stdexcept>

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

void createBuffer(
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
}