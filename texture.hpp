#pragma once

// Let GLFW include by itslef vulkan headers
#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"

namespace texture {

void createTextureImage(VkPhysicalDevice physicalDevice, VkDevice logicalDevice);

}