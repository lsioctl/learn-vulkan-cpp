#pragma once

// Let GLFW include by itslef vulkan headers
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <optional>
#include <vector>

namespace device {

struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentationFamily;
    bool isComplete() {
        return graphicsFamily.has_value() && presentationFamily.has_value();
    }
};

void printExtensions();

/**
 * Debug callback function for validation layers
 */
VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData);

void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);

void setupDebugMessenger(VkInstance instance, bool enable_validation_layers, VkDebugUtilsMessengerEXT* pDebugMessenger);

/***
 * proxy function
 * vkCreateDebugUtilsMessengerEXT is an extension function and so it not
 * automatically loaded
 * we have to look up the address ourself with vkGetInstanceProcAddr
 */
VkResult createDebugUtilsMessengerEXT(
    VkInstance instance, 
    const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, 
    const VkAllocationCallbacks* pAllocator, 
    VkDebugUtilsMessengerEXT* pDebugMessenger
);

/***
 * proxy function
 * vkDestroyDebugUtilsMessengerEXT is an extension function and so it not
 * automatically loaded
 * we have to look up the address ourself with vkGetInstanceProcAddr
 */
void destroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator);

bool isPhysicalDeviceSuitable(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, const std::vector<const char*>& device_extensions);
bool checkValidationLayerSupport(const std::vector<const char*>& validation_layers);
/***
 * return the required list of extension based on wheter validation
 * layer is set or not
 */
std::vector<const char*> getRequiredExtensions(bool enable_validation_layers);
/**
 * enumerate the extensions and check if all of the required extensions are amongst them
 * TODO: "private"
 */
bool checkDeviceExtensionSupport(VkPhysicalDevice device, const std::vector<const char*>& device_extensions);
QueueFamilyIndices findQueueFamilies(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface);

/***
 * pickup the first GPU supporting vulkan
 */
void pickPhysicalDevice(
    VkInstance instance,
    VkSurfaceKHR surface,
    const std::vector<const char*>& device_extensions,
    // TODO: better returning the handle
    VkPhysicalDevice* pPhysicalDevice
);

VkSampleCountFlagBits getMaxUsableSampleCount(VkPhysicalDevice physicalDevice);

void createLogicalDevice(
    VkPhysicalDevice physicalDevice,
    VkSurfaceKHR surface,
    const std::vector<const char*>& device_extensions,
    bool enable_validation_layers,
    const std::vector<const char*>& validation_layers,
    VkDevice* pLogicalDevice,
    VkQueue* pGraphicsQueue,
    VkQueue* pPresentQueue
);

/**
 * Unlike the texture image, we don't necessarily need a specific format, because we won't 
 * be directly accessing the texels from the program. It just needs to have a reasonable 
 * accuracy, at least 24 bits is common in real-world applications. 
 * There are several formats that fit this requirement:
    VK_FORMAT_D32_SFLOAT: 32-bit float for depth
    VK_FORMAT_D32_SFLOAT_S8_UINT: 32-bit signed float for depth and 8 bit stencil component
    VK_FORMAT_D24_UNORM_S8_UINT: 24-bit float for depth and 8 bit stencil component
 */
VkFormat findSupportedDepthImageFormat(
    VkPhysicalDevice physicalDevice,
    const std::vector<VkFormat>& candidates,
    VkImageTiling tiling,
    VkFormatFeatureFlags features
);

/**
 * does the depth image format has a stencil component
 */
bool hasStencilComponent(VkFormat format);

}