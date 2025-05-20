#pragma once

// Let GLFW include by itslef vulkan headers
#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"

namespace commandbuffer {

VkCommandBuffer beginSingleTimeCommands(
    VkDevice logicalDevice,
    VkCommandPool commandPool
);

void endAndExecuteSingleTimeCommands(
    VkDevice logicalDevice,
    VkCommandPool commandPool,
    VkQueue graphicsQueue,
    VkCommandBuffer commandBuffer
);

}