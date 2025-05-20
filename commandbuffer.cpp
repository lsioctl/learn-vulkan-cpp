#include "commandbuffer.hpp"

namespace commandbuffer {

VkCommandBuffer beginSingleTimeCommands(
    VkDevice logicalDevice,
    VkCommandPool commandPool
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

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    // We're only going to use the command buffer once and wait with returning 
    // from the function until the copy operation has finished executing
    // good practice: tell the driver our intent
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    // start recording
    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
}

void endAndExecuteSingleTimeCommands(
    VkDevice logicalDevice,
    VkCommandPool commandPool,
    VkQueue graphicsQueue,
    VkCommandBuffer commandBuffer
) {
    // end recording
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    // execute the command buffer
    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    // there is no event to wait, unlike the draw command
    // we juste want to execute immediately
    // two way to wait for the transfer to complete:
    // * vkWaitForFence (would allow us to schedule multiple transfer simultaneously)
    // * vkQueueWaitIdle
    vkQueueWaitIdle(graphicsQueue);

    vkFreeCommandBuffers(logicalDevice, commandPool, 1, &commandBuffer);
}
}