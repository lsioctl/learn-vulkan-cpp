#pragma once

// Let GLFW include by itslef vulkan headers
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <optional>
#include <vector>

namespace utils {

struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentationFamily;
    bool isComplete() {
        return graphicsFamily.has_value() && presentationFamily.has_value();
    }
};

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
    
bool isDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface, const std::vector<const char*>& device_extensions);
bool checkValidationLayerSupport(const std::vector<const char*>& validation_layers);
/***
 * return the required list of extension based on wheter validation
 * layer is set or not
 */
std::vector<const char*> GetRequiredExtensions(bool enable_validation_layers);
/**
 * enumerate the extensions and check if all of the required extensions are amongst them
 * TODO: "private"
 */
bool checkDeviceExtensionSupport(VkPhysicalDevice device, const std::vector<const char*>& device_extensions);
QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface);
SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);




}