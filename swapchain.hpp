#pragma once

// Let GLFW include by itslef vulkan headers
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <optional>
#include <vector>

namespace swapchain {

struct SwapChainSupportDetails {
    /** 
    Basic surface capabilities (min/max number of images in 
    swap chain, min/max width and height of images)
    */
    VkSurfaceCapabilitiesKHR capabilities;
    /** Surface formats (pixel format, color space) */
    std::vector<VkSurfaceFormatKHR> formats;
    /** Available presentation modes */
    std::vector<VkPresentModeKHR> presentationModes;
};
    
SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);


}