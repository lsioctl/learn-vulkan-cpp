#include <iostream>
#include <cstring>
#include <set>

#include "device.hpp"
#include "swapchain.hpp"

namespace device {

/***
 * Not used for now, just an example of what we could do to pickup
 * a GPU
 */
// bool IsDeviceSuitableAdvancedExample(VkPhysicalDevice device) {
//     VkPhysicalDeviceProperties deviceProperties;
//     VkPhysicalDeviceFeatures deviceFeatures;
//     vkGetPhysicalDeviceProperties(device, &deviceProperties);
//     vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

//     return deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU &&
//            deviceFeatures.geometryShader;
// }

/***
 * Not used for now
 */
// int RateDeviceSuitability(VkPhysicalDevice device) {
//     VkPhysicalDeviceProperties deviceProperties;
//     VkPhysicalDeviceFeatures deviceFeatures;
//     vkGetPhysicalDeviceProperties(device, &deviceProperties);
//     vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

//     int score = 0;

//     // Discrete GPUs have a significant performance advantage
//     if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
//         score += 1000;
//     }

//     // Maximum possible size of textures affects graphics quality
//     score += deviceProperties.limits.maxImageDimension2D;

//     // Application can't function without geometry shaders
//     if (!deviceFeatures.geometryShader) {
//         return 0;
//     }

//     return score;
// }

/***
 * Not used for now, pickup GPU with the highest score
 */
// void pickPhysicalDeviceByScore() {
//     uint32_t deviceCount = 0;
//     vkEnumeratePhysicalDevices(instance_, &deviceCount, nullptr);

//     if (deviceCount == 0) {
//         throw std::runtime_error("failed to find GPUs with Vulkan support!");
//     }

//     std::vector<VkPhysicalDevice> devices(deviceCount);
//     vkEnumeratePhysicalDevices(instance_, &deviceCount, devices.data());

//     // Use an ordered map to automatically sort candidates by increasing score
//     std::multimap<int, VkPhysicalDevice> candidates;

//     for (const auto& device : devices) {
//         int score = RateDeviceSuitability(device);
//         candidates.insert(std::make_pair(score, device));
//     }

//     // Check if the best candidate is suitable at all
//     if (candidates.rbegin()->first > 0) {
//         physicalDevice_ = candidates.rbegin()->second;
//     } else {
//         throw std::runtime_error("failed to find a suitable GPU!");
//     }
// }


void printExtensions() {
    // retrieve a list of supported extensions
    // could be compared to glfwGetRequiredInstanceExtensions

    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> extensions(extensionCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());
    std::cout << "Available extensions:\n";

    for (const auto& extension : extensions) {
        std::cout << '\t' << extension.extensionName << '\n';
    }

}

VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) {

    std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

    return VK_FALSE;
}

void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    // all types execept VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT 
    // here to receive notifications about possible problems while leaving out 
    // verbose general debug info.
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | 
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | 
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    // all types enabled here
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | 
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | 
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;
    createInfo.pUserData = nullptr; // Optional
}

VkResult createDebugUtilsMessengerEXT(
    VkInstance instance, 
    const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, 
    const VkAllocationCallbacks* pAllocator, 
    VkDebugUtilsMessengerEXT* pDebugMessenger
) {
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    } else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

void setupDebugMessenger(VkInstance instance, bool enable_validation_layers, VkDebugUtilsMessengerEXT* pDebugMessenger) {
    if (!enable_validation_layers) return;

    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    populateDebugMessengerCreateInfo(createInfo);
    
    if (createDebugUtilsMessengerEXT(instance, &createInfo, nullptr, pDebugMessenger) != VK_SUCCESS) {
        throw std::runtime_error("failed to set up debug messenger!");
    }
}


void destroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) {
        func(instance, debugMessenger, pAllocator);
    }
}

std::vector<const char*> getRequiredExtensions(bool enable_validation_layers) {
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions;

    // the extension required by GLFW are always required
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

    if (enable_validation_layers) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    return extensions;
}

bool checkDeviceExtensionSupport(VkPhysicalDevice device, const std::vector<const char*>& device_extensions) {
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

    std::set<std::string> requiredExtensions(device_extensions.begin(), device_extensions.end());

    for (const auto& extension : availableExtensions) {
        requiredExtensions.erase(extension.extensionName);
    }

    return requiredExtensions.empty();
}

QueueFamilyIndices findQueueFamilies(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface) {
    QueueFamilyIndices indices;

    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queue_family_count, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queue_family_count, queueFamilies.data());

    int i = 0;
    for (const auto& queueFamily : queueFamilies) {
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            VkBool32 presentationSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &presentationSupport);
            // Note: likely to be the same queue family
            // we could optimise later and look for a device
            // that support drawing and presentation in the same queue for performance
            if (presentationSupport) {
                indices.presentationFamily = i;
            }
            indices.graphicsFamily = i;
            break;
        }

        i++;
    }

    return indices;
}


bool isPhysicalDeviceSuitable(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, const std::vector<const char*>& device_extensions) {
    QueueFamilyIndices indices = findQueueFamilies(physicalDevice, surface);

    bool extensionsSupported = checkDeviceExtensionSupport(physicalDevice, device_extensions);

    bool swapChainAdequate = false;

    if (extensionsSupported) {
        swapchain::SwapChainSupportDetails swapChainSupport = swapchain::querySwapChainSupport(physicalDevice, surface);
        swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentationModes.empty();
    }

    return indices.isComplete() && extensionsSupported && swapChainAdequate;
}

bool checkValidationLayerSupport(const std::vector<const char*>& validation_layers) {
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for (const char* layerName : validation_layers) {
        bool layerFound = false;

        for (const auto& layerProperties : availableLayers) {
            std::cout << layerName << std::endl;
            std::cout << layerProperties.layerName << std::endl;
            if (strcmp(layerName, layerProperties.layerName) == 0) {
                layerFound = true;
                break;
            }
        }

        if (!layerFound) {
            return false;
        }
    }

    return true;
}

/***
 * pickup the first GPU supporting vulkan
 */
void pickPhysicalDevice(
    VkInstance instance,
    VkSurfaceKHR surface,
    const std::vector<const char*>& device_extensions,
    // TODO: better returning the handle
    VkPhysicalDevice* pPhysicalDevice
) {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

    if (deviceCount == 0) {
        throw std::runtime_error("failed to find GPUs with Vulkan support!");
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    // Pick the first suitable device
    for (const auto& device : devices) {
        if (device::isPhysicalDeviceSuitable(device, surface, device_extensions)) {
            *pPhysicalDevice = device;
            break;
        }
    }

    if (*pPhysicalDevice == VK_NULL_HANDLE) {
        throw std::runtime_error("failed to find a suitable GPU!");
    }
}

void createLogicalDevice(
    VkPhysicalDevice physicalDevice,
    VkSurfaceKHR surface,
    const std::vector<const char*>& device_extensions,
    bool enable_validation_layers,
    const std::vector<const char*>& validation_layers,
    VkDevice* pLogicalDevice,
    VkQueue* pGraphicsQueue,
    VkQueue* pPresentQueue
    ) {
    // Specify the queues to be created
    // TODO: dedicated function ?
    device::QueueFamilyIndices indices = device::findQueueFamilies(physicalDevice, surface);

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    // we are interested in queues with graphics and presentation capabilities
    std::set<uint32_t> uniqueQueueFamilies = {
        indices.graphicsFamily.value(),
        indices.presentationFamily.value()
    };

    // This is required even if there is only a single queue:
    // priority between 0.0 and 1.0
    float queuePriority = 1.0f;

    for (uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        // we need only one queue
        // TODO: understand this
        /**
         * The currently available drivers will only allow you to create a small number 
         * of queues for each queue family and you don't really need more than one. 
         * That's because you can create all of the command buffers on multiple threads 
         * and then submit them all at once on the main thread with a single low-overhead call
         */
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    // TODO: use what we saw previously with vkGetPhysicalDeviceFeatures
    // like geometry shaders
    // for now VK_FALSE
    VkPhysicalDeviceFeatures deviceFeatures{};

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();

    createInfo.pEnabledFeatures = &deviceFeatures;

    // it may look like physical device
    // but we are working with logical device
    // so for example some logical devices will be compute only
    // or graphic only with VK_KHR_swapchain
    createInfo.enabledExtensionCount = static_cast<uint32_t>(device_extensions.size());
    createInfo.ppEnabledExtensionNames = device_extensions.data();

    // Below code if for older versions
    // as newer implementations (since 1.3 ?)
    // do not distinguish between instance and device specific validation layers
    // and below information is discarded
    if (enable_validation_layers) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validation_layers.size());
        createInfo.ppEnabledLayerNames = validation_layers.data();
    } else {
        createInfo.enabledLayerCount = 0;
    }

    if (vkCreateDevice(physicalDevice, &createInfo, nullptr, pLogicalDevice) != VK_SUCCESS) {
        throw std::runtime_error("failed to create logical device!");
    }

    // retrieve queues handles
    // if the queues are the same, it is more than likely than handles will be the same
    vkGetDeviceQueue(*pLogicalDevice, indices.graphicsFamily.value(), 0, pGraphicsQueue);
    vkGetDeviceQueue(*pLogicalDevice, indices.presentationFamily.value(), 0, pPresentQueue);
}


}