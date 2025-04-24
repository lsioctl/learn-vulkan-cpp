// Let GLFW include by itslef vulkan headers
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <vector>
#include <cstring>
#include <map>
#include <optional>
#include <set>
#include <cstdint> // Necessary for uint32_t
#include <limits> // Necessary for std::numeric_limits
#include <algorithm> // Necessary for std::clamp
#include <fstream>

static std::vector<char> readFile(const std::string& filename) {
    // start reading at the end of the file and no text transformations
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("failed to open file!");
    }

    // as we started at the end of the file
    // we can use the read position for the size
    // and allocate a buffer
    // TODO: more idiomatic way ?
    size_t fileSize = (size_t) file.tellg();
    std::vector<char> buffer(fileSize);

    // TODO: is it really safe ?
    // seek back to the beginning of the file
    // and read all at once
    file.seekg(0);
    file.read(buffer.data(), fileSize);

    file.close();

    return buffer;
}


void errorCallback(int error, const char* description)
{
    fprintf(stderr, "Error: %s\n", description);
}

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


const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

const std::vector<const char*> VALIDATION_LAYERS = {
    "VK_LAYER_KHRONOS_validation"
};

// we use the macro to avoid mispelling
// "VK_KHR_swapchain"
const std::vector<const char*> DEVICE_EXTENSIONS = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

const auto VERT_FILE = "./shaders/spirv/shader1.vert.spirv";
const auto FRAG_FILE = "./shaders/spirv/shader1.frag.spirv";

#ifdef NDEBUG
    const bool enableValidationLayers = false;
#else
    const bool ENABLE_VALIDATION_LAYERS = true;
#endif

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


bool checkValidationLayerSupport() {
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for (const char* layerName : VALIDATION_LAYERS) {
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
 * return the required list of extension based on wheter validation
 * layer is set or not
 */
std::vector<const char*> GetRequiredExtensions() {
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions;

    // the extension required by GLFW are always required
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

    if (ENABLE_VALIDATION_LAYERS) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    return extensions;
}

VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) {

    std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

    return VK_FALSE;
}

/***
 * proxy function
 * vkCreateDebugUtilsMessengerEXT is an extension function and so it not
 * automatically loaded
 * we have to look up the address ourself with vkGetInstanceProcAddr
 */
VkResult CreateDebugUtilsMessengerEXT(
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

/***
 * proxy function
 * vkDestroyDebugUtilsMessengerEXT is an extension function and so it not
 * automatically loaded
 * we have to look up the address ourself with vkGetInstanceProcAddr
 */
void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) {
        func(instance, debugMessenger, pAllocator);
    }
}

/***
 * Not used for now, just an example of what we could do to pickup
 * a GPU
 */
bool IsDeviceSuitableAdvancedExample(VkPhysicalDevice device) {
    VkPhysicalDeviceProperties deviceProperties;
    VkPhysicalDeviceFeatures deviceFeatures;
    vkGetPhysicalDeviceProperties(device, &deviceProperties);
    vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

    return deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU &&
           deviceFeatures.geometryShader;
}

/***
 * Not used for now
 */
int RateDeviceSuitability(VkPhysicalDevice device) {
    VkPhysicalDeviceProperties deviceProperties;
    VkPhysicalDeviceFeatures deviceFeatures;
    vkGetPhysicalDeviceProperties(device, &deviceProperties);
    vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

    int score = 0;

    // Discrete GPUs have a significant performance advantage
    if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
        score += 1000;
    }

    // Maximum possible size of textures affects graphics quality
    score += deviceProperties.limits.maxImageDimension2D;

    // Application can't function without geometry shaders
    if (!deviceFeatures.geometryShader) {
        return 0;
    }

    return score;
}

/**
 * enumerate the extensions and check if all of the required extensions are amongst them
 */
bool checkDeviceExtensionSupport(VkPhysicalDevice device) {
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

    std::set<std::string> requiredExtensions(DEVICE_EXTENSIONS.begin(), DEVICE_EXTENSIONS.end());

    for (const auto& extension : availableExtensions) {
        requiredExtensions.erase(extension.extensionName);
    }

    return requiredExtensions.empty();
}

/**
 * For the color space we'll use SRGB if it is available, 
 * because it results in more accurate perceived colors. 
 * It is also pretty much the standard color space for images, like the textures we'll use later on. 
 * Because of that we should also use an SRGB color format, of which one of the most common 
 * ones is VK_FORMAT_B8G8R8A8_SRGB.
 * 
 * If it fails it will return the first item of availableFormats
 */
VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
    for (const auto& availableFormat : availableFormats) {
        /**
         * The format member specifies the color channels and types. 
         * For example, VK_FORMAT_B8G8R8A8_SRGB means that we store the B, G, R and alpha channels 
         * in that order with an 8 bit unsigned integer for a total of 32 bits per pixel.
         */
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB 
        /**
         * The colorSpace member indicates if the SRGB color space is supported or not using the 
         * VK_COLOR_SPACE_SRGB_NONLINEAR_KHR flag. 
         * Note that this flag used to be called VK_COLORSPACE_SRGB_NONLINEAR_KHR 
         * in old versions of the specification.
         */
            && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
        ) {
            return availableFormat;
        }
    }

    /**
     * if it fails we could start ranking the available formats based on how "good" they are, 
     * but in most cases it's okay to just settle with the first format that is specified.
     */
    return availableFormats[0];
}
/**
 * The presentation mode is arguably the most important setting for the swap chain, 
 * because it represents the actual conditions for showing images to the screen. 
 * There are four possible modes available in Vulkan:
 *   VK_PRESENT_MODE_IMMEDIATE_KHR: Images submitted by your application are 
 * transferred to the screen right away, which may result in tearing.
 *   VK_PRESENT_MODE_FIFO_KHR: The swap chain is a queue where the display 
 * takes an image from the front of the queue when the display is refreshed 
 * and the program inserts rendered images at the back of the queue. 
 * If the queue is full then the program has to wait. 
 * This is most similar to vertical sync as found in modern games. 
 * The moment that the display is refreshed is known as "vertical blank".
 *   VK_PRESENT_MODE_FIFO_RELAXED_KHR: This mode only differs from the previous
 *  one if the application is late and the queue was empty at the last vertical blank. 
 * Instead of waiting for the next vertical blank, the image is transferred right 
 * away when it finally arrives. This may result in visible tearing.
 *   VK_PRESENT_MODE_MAILBOX_KHR: This is another variation of the second mode. 
 * Instead of blocking the application when the queue is full, the images that 
 * are already queued are simply replaced with the newer ones. 
 * This mode can be used to render frames as fast as possible while still avoiding tearing,
 * resulting in fewer latency issues than standard vertical sync. 
 * This is commonly known as "triple buffering", although the existence of three buffers 
 * alone does not necessarily mean that the framerate is unlocked.

 * Only the VK_PRESENT_MODE_FIFO_KHR mode is guaranteed to be available
 * so we return it if we don't find better
 */
VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentationModes) {
    for (const auto& availablePresentMode : availablePresentationModes) {
        // choice of the author of the tutorial
        // he says on mobile device, where consumption is prime
        // it may be better to choose VK_PRESENT_MODE_FIFO_KHR
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return availablePresentMode;
        }
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}




class HelloTriangleApplication {
public:
    void run() {
        initWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }

private:
    GLFWwindow* window_;
    VkInstance instance_;
    VkDebugUtilsMessengerEXT debugMessenger_;
    /**
     * This will be destroyed when vkInstance will be destroyed
     * so no need to do anything in Cleanup() about it
     * */
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    /**
     * logical device
     */
    VkDevice device_;
    /**
     * Queues are created with the logical devices
     * store a handle to the graphics queue
     * Device queues are implicitly cleaned up when the device is destroyed, 
     * so we don't need to do anything in cleanup.
     */
    VkQueue graphicsQueue_;
    VkQueue presentationQueue_;
    VkSurfaceKHR surface_;
    VkSwapchainKHR swapChain_;
    /** Images will be destroyed when Swap Chain is destroyed */
    std::vector<VkImage> swapChainImages_;
    VkFormat swapChainImageFormat_;
    VkExtent2D swapChainExtent_;
    std::vector<VkImageView> swapChainImageViews_;
    VkPipelineLayout pipelineLayout_;
    void createSurface() {
        // if the surface object is platform agnostic, its creation is not
        // let glfw handle this for us
        if (glfwCreateWindowSurface(instance_, window_, nullptr, &surface_) != VK_SUCCESS) {
            throw std::runtime_error("failed to create window surface!");
        }
    }
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) {
        QueueFamilyIndices indices;

        uint32_t queue_family_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, nullptr);

        std::vector<VkQueueFamilyProperties> queueFamilies(queue_family_count);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queueFamilies.data());

        int i = 0;
        for (const auto& queueFamily : queueFamilies) {
            if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                VkBool32 presentationSupport = false;
                vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface_, &presentationSupport);
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
    bool isDeviceSuitable(VkPhysicalDevice device) {
        QueueFamilyIndices indices = findQueueFamilies(device);

        bool extensionsSupported = checkDeviceExtensionSupport(device);

        bool swapChainAdequate = false;

        if (extensionsSupported) {
            SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
            swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentationModes.empty();
        }

        return indices.isComplete() && extensionsSupported && swapChainAdequate;
    }
    /**
     * TODO: make surface_ dependency explicit
     */
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device) {
        SwapChainSupportDetails details;

        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface_, &details.capabilities);

        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &formatCount, nullptr);

        if (formatCount != 0) {
            details.formats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &formatCount, details.formats.data());
        }

        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface_, &presentModeCount, nullptr);

        if (presentModeCount != 0) {
            details.presentationModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface_, &presentModeCount, details.presentationModes.data());
        }

        return details;
    }
    /**
     * The swap extent is the resolution of the swap chain images and it's almost 
     * always exactly equal to the resolution of the window that we're drawing 
     * to in pixels (more on that in a moment). 
     * The range of the possible resolutions is defined in the VkSurfaceCapabilitiesKHR 
     * structure. Vulkan tells us to match the resolution of the window by setting 
     * the width and height in the currentExtent member.
     * However, some window managers do allow us to differ here and this is indicated by 
     * setting the width and height in currentExtent to a special value: the maximum value of uint32_t. 
     * In that case we'll pick the resolution that best matches the window within the minImageExtent
     * and maxImageExtent bounds. 
     * But we must specify the resolution in the correct unit.
     * 
     * GLFW uses two units when measuring sizes: pixels and screen coordinates. 
     * For example, the resolution {WIDTH, HEIGHT} that we specified earlier when creating the window 
     * is measured in screen coordinates. But Vulkan works with pixels, 
     * so the swap chain extent must be specified in pixels as well. 
     * Unfortunately, if you are using a high DPI display (like Apple's Retina display), 
     * screen coordinates don't correspond to pixels. Instead, due to the higher pixel density, 
     * the resolution of the window in pixel will be larger than the resolution in screen coordinates. 
     * So if Vulkan doesn't fix the swap extent for us, we can't just use the original {WIDTH, HEIGHT}. 
     * Instead, we must use glfwGetFramebufferSize to query the resolution of the window in pixel before 
     * matching it against the minimum and maximum image extent.
     */
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
            return capabilities.currentExtent;
        } else {
            int width, height;
            glfwGetFramebufferSize(window_, &width, &height);

            VkExtent2D actualExtent = {
                static_cast<uint32_t>(width),
                static_cast<uint32_t>(height)
            };

            actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
            actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

            return actualExtent;
        }
    }
    void createSwapChain() {
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice_);

        VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
        VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentationModes);
        VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

        // recommended: min image + 1
        uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
        // if maxImageCount == 0 => unlimited
        if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
            imageCount = swapChainSupport.capabilities.maxImageCount;
        }

        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = surface_;
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = extent;
        // always 1 except stereoscopic 3D application
        createInfo.imageArrayLayers = 1;
        // we will render directly to the images in the swapchain
        /**
         * It is also possible that you'll render images to a separate image first to perform 
         * operations like post-processing. In that case you may use a value like VK_IMAGE_USAGE_TRANSFER_DST_BIT 
         * instead and use a memory operation to transfer the rendered image to a swap chain image.
         */
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        QueueFamilyIndices indices = findQueueFamilies(physicalDevice_);
        uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentationFamily.value()};

        // handle swap chain images that may be used across multiple queue families.
        if (indices.graphicsFamily != indices.presentationFamily) {
            // multiple queue families, we use concurrent instead of exclusive to avoid ownership issues
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = queueFamilyIndices;
        } else {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            createInfo.queueFamilyIndexCount = 0; // Optional
            createInfo.pQueueFamilyIndices = nullptr; // Optional
        }
        // we do not want any transformation,(90 degres rotation, horizontal flip, ...)
        // even if available in supportedTransforms in capabilities
        createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
        // ignore the alpha channel
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode = presentMode;
        // we don't care about pixel obscured, e.g. by a window in front of them
        createInfo.clipped = VK_TRUE;
        // a new swapchain may be created if, for example, we resize the window
        // for now we will use only one swap chain
        createInfo.oldSwapchain = VK_NULL_HANDLE;

        if (vkCreateSwapchainKHR(device_, &createInfo, nullptr, &swapChain_) != VK_SUCCESS) {
            throw std::runtime_error("failed to create swap chain!");
        }

        vkGetSwapchainImagesKHR(device_, swapChain_, &imageCount, nullptr);
        swapChainImages_.resize(imageCount);
        vkGetSwapchainImagesKHR(device_, swapChain_, &imageCount, swapChainImages_.data());

        swapChainImageFormat_ = surfaceFormat.format;
        swapChainExtent_ = extent;
    }
    void initWindow() {
        glfwSetErrorCallback(errorCallback);
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        window_ = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
        // std::cout << window_ << std::endl;
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
        createInfo.pfnUserCallback = DebugCallback;
        createInfo.pUserData = nullptr; // Optional
    }

    void createInstance() {
        if (ENABLE_VALIDATION_LAYERS && !checkValidationLayerSupport()) {
            throw std::runtime_error("validation layers requested, but not available!");
        }

        // The instance is the connection between your application and the Vulkan library
        VkApplicationInfo appInfo{};

        // a lot of information on vk is passed through structs instead of function parameters
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Hello Triangle";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "No Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_0;

        // a lot of information on vk is passed through structs instead of function parameters
        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;
        
        // create an additional debuger for vkCreateInstance and vkDestroyInstance
        // TODO: a bit unclear to me why this is written by the doc:
        /***
         * The debugCreateInfo variable is placed outside the if statement to ensure that 
         * it is not destroyed before the vkCreateInstance call. By creating an additional 
         * debug messenger this way it will automatically be used during vkCreateInstance 
         * and vkDestroyInstance and cleaned up after that.
         */
        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
        // Here checkValidationLayerSupport has already passed
        // TODO: clumsy in the control flow
        if (ENABLE_VALIDATION_LAYERS) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(VALIDATION_LAYERS.size());
            createInfo.ppEnabledLayerNames = VALIDATION_LAYERS.data();

            populateDebugMessengerCreateInfo(debugCreateInfo);
            createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*) &debugCreateInfo;
        } else {
            createInfo.enabledLayerCount = 0;

            createInfo.pNext = nullptr;
        }

        auto extensions = GetRequiredExtensions();
        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();

        // Note: there is a RAII way to do instead of this: find this an migrate
        // to it when the tutorial is finished
        if (vkCreateInstance(&createInfo, nullptr, &instance_) != VK_SUCCESS) {
            throw std::runtime_error("failed to create instance!");
        }
    }

    void setupDebugMessenger() {
        if (!ENABLE_VALIDATION_LAYERS) return;

        VkDebugUtilsMessengerCreateInfoEXT createInfo{};
        populateDebugMessengerCreateInfo(createInfo);
        
        if (CreateDebugUtilsMessengerEXT(instance_, &createInfo, nullptr, &debugMessenger_) != VK_SUCCESS) {
            throw std::runtime_error("failed to set up debug messenger!");
        }
    }

    /***
     * pickup the first GPU supporting vulkan
     */
    void pickPhysicalDevice() {
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance_, &deviceCount, nullptr);

        if (deviceCount == 0) {
            throw std::runtime_error("failed to find GPUs with Vulkan support!");
        }

        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance_, &deviceCount, devices.data());

        // Pick the first suitable device
        for (const auto& device : devices) {
            if (isDeviceSuitable(device)) {
                physicalDevice_ = device;
                break;
            }
        }

        if (physicalDevice_ == VK_NULL_HANDLE) {
            throw std::runtime_error("failed to find a suitable GPU!");
        }
    }

    /***
     * Not used for now, pickup GPU with the highest score
     */
    void pickPhysicalDeviceByScore() {
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance_, &deviceCount, nullptr);

        if (deviceCount == 0) {
            throw std::runtime_error("failed to find GPUs with Vulkan support!");
        }

        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance_, &deviceCount, devices.data());

        // Use an ordered map to automatically sort candidates by increasing score
        std::multimap<int, VkPhysicalDevice> candidates;

        for (const auto& device : devices) {
            int score = RateDeviceSuitability(device);
            candidates.insert(std::make_pair(score, device));
        }

        // Check if the best candidate is suitable at all
        if (candidates.rbegin()->first > 0) {
            physicalDevice_ = candidates.rbegin()->second;
        } else {
            throw std::runtime_error("failed to find a suitable GPU!");
        }
    }

    void createLogicalDevice() {
        // Specify the queues to be created
        // TODO: dedicated function ?
        QueueFamilyIndices indices = findQueueFamilies(physicalDevice_);

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
        createInfo.enabledExtensionCount = static_cast<uint32_t>(DEVICE_EXTENSIONS.size());
        createInfo.ppEnabledExtensionNames = DEVICE_EXTENSIONS.data();

        // Below code if for older versions
        // as newer implementations (since 1.3 ?)
        // do not distinguish between instance and device specific validation layers
        // and below information is discarded
        if (ENABLE_VALIDATION_LAYERS) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(VALIDATION_LAYERS.size());
            createInfo.ppEnabledLayerNames = VALIDATION_LAYERS.data();
        } else {
            createInfo.enabledLayerCount = 0;
        }

        if (vkCreateDevice(physicalDevice_, &createInfo, nullptr, &device_) != VK_SUCCESS) {
            throw std::runtime_error("failed to create logical device!");
        }

        // retrieve queues handles
        // if the queues are the same, it is more than likely than handles will be the same
        vkGetDeviceQueue(device_, indices.graphicsFamily.value(), 0, &graphicsQueue_);
        vkGetDeviceQueue(device_, indices.presentationFamily.value(), 0, &presentationQueue_);
    }

    void createImageViews() {
        auto swapChainImageSize = swapChainImages_.size();
        swapChainImageViews_.resize(swapChainImageSize);

        for (size_t i = 0; i < swapChainImageSize; i++) {
            VkImageViewCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            createInfo.image = swapChainImages_[i];
            // could be 1D, 2D, 3D Textures and cube maps
            createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            createInfo.format = swapChainImageFormat_;
            // The components field allows you to swizzle the color channels around.
            // we will stick to the default
            createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            // 1 layer as we are not playing with stereographic 3D application
            // 1 view by eye
            createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            createInfo.subresourceRange.baseMipLevel = 0;
            createInfo.subresourceRange.levelCount = 1;
            createInfo.subresourceRange.baseArrayLayer = 0;
            // 1 layer as we are not playing with stereographic 3D application
            // (1 view by eye) by accessing different layers
            createInfo.subresourceRange.layerCount = 1;

            if (vkCreateImageView(device_, &createInfo, nullptr, &swapChainImageViews_[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create image views!");
            }
        }
    }

    VkShaderModule createShaderModule(const std::vector<char>& code) {
        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = code.size();
        // TODO: why reinterpret_cast and not static_cast ?
        createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

        VkShaderModule shaderModule;
        if (vkCreateShaderModule(device_, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
            throw std::runtime_error("failed to create shader module!");
        }

        return shaderModule;
    }

    void createGraphicsPipeline() {
        auto vertShaderCode = readFile(VERT_FILE);
        auto fragShaderCode = readFile(FRAG_FILE);

        VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
        VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

        VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
        vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertShaderStageInfo.module = vertShaderModule;
        vertShaderStageInfo.pName = "main";
        // pSpecializationInfo could be interesting for constants and optimizations

        VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
        fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragShaderStageInfo.module = fragShaderModule;
        fragShaderStageInfo.pName = "main";

        VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

        // for now Vertex data hardcoded in the shader
        // so tell there is no vertex data for now
        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 0;
        vertexInputInfo.pVertexBindingDescriptions = nullptr; // Optional
        vertexInputInfo.vertexAttributeDescriptionCount = 0;
        vertexInputInfo.pVertexAttributeDescriptions = nullptr; // Optional

        /**
         * 
            VK_PRIMITIVE_TOPOLOGY_POINT_LIST: points from vertices
            VK_PRIMITIVE_TOPOLOGY_LINE_LIST: line from every 2 vertices without reuse
            VK_PRIMITIVE_TOPOLOGY_LINE_STRIP: the end vertex of every line is used as start vertex 
            for the next line
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST: triangle from every 3 vertices without reuse
            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP: the second and third vertex of every triangle are 
            used as first two vertices of the next triangle

            with element buffer we could specify the indices to use yourself
         */
        // We will draw triangles throughout the tutorial
        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        // describes the region of the framebuffer that the output will be rendered to
        // This is almost always (0, 0) to (width, height)
        // size of the swap chain and its images may differ from the WIDTH and HEIGHT 
        // of the window. The swap chain images will be used as framebuffers later on,
        //  so we should stick to their size.
        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float) swapChainExtent_.width;
        viewport.height = (float) swapChainExtent_.height;
        // specify the range of depth values to use for the framebuffer. 
        // values must be within the [0.0f, 1.0f] range, 
        // but minDepth may be higher than maxDepth
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        // to draw to the entire framebuffer, we would specify a scissor rectangle that covers
        // it entirely:
        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = swapChainExtent_;


        // Very little amount of state can be changed on the pipeline which is mostly immutable
        // viewport and scissor can be dynamic without performance penalty
        // viewport and scissor rectangle will have to be set up at drawing time
        std::vector<VkDynamicState> dynamicStates = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
        };

        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();

        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;


        // if we want viewport ans scissor also immutable this could be done instead
        // some GPU could still have multiple so the struct reference an array of them
        // using multiple requires enabling a special GPU feature
        /**
         * VkPipelineViewportStateCreateInfo viewportState{};
            viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
            viewportState.viewportCount = 1;
            viewportState.pViewports = &viewport;
            viewportState.scissorCount = 1;
            viewportState.pScissors = &scissor;
         */

        
        /**
         * The rasterizer takes the geometry that is shaped by the vertices from the vertex
         * shader and turns it into fragments to be colored by the fragment shader. 
         * It also performs depth testing, face culling and the scissor test, 
         * and it can be configured to output fragments that fill entire polygons 
         * or just the edges (wireframe rendering).
         */
        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        // thicker than 1 requires wideLines GPU feature
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
        rasterizer.depthBiasEnable = VK_FALSE;
        rasterizer.depthBiasConstantFactor = 0.0f; // Optional
        rasterizer.depthBiasClamp = 0.0f; // Optional
        rasterizer.depthBiasSlopeFactor = 0.0f; // Optional


        /**
         * Multisampling is one of the ways to perform anti-aliasing. 
         * It works by combining the fragment shader results of multiple polygons
         * that rasterize to the same pixel. 
         * This mainly occurs along edges, 
         * which is also where the most noticeable aliasing artifacts occur. 
         * Because it doesn't need to run the fragment shader multiple times 
         * if only one polygon maps to a pixel, it is significantly less expensive
         * than simply rendering to a higher resolution and then downscaling. 
         * Enabling it requires enabling a GPU feature.
         */
        // For now ms is disabled
        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        multisampling.minSampleShading = 1.0f; // Optional
        multisampling.pSampleMask = nullptr; // Optional
        multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
        multisampling.alphaToOneEnable = VK_FALSE; // Optional

        // Depth/Stencil buffer
        // We do have it right now so pass nullptr instead of a struct
        // for VkPipelineDepthStencilStateCreateInfo


        /**
         * After a fragment shader has returned a color, 
         * it needs to be combined with the color that is already in the framebuffer.
         * This transformation is known as color blending and there are two ways to do it:
         *  Mix the old and new value to produce a final color
         * Combine the old and new value using a bitwise operation
         * 
         */
        // configuration per attached framebuffer
        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT 
            | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        // following parameters for alpha blending
        colorBlendAttachment.blendEnable = VK_TRUE;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

        // global configuration, constants used by colorBlendAttachment
        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
        // we have only one attachment
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;
        colorBlending.blendConstants[0] = 0.0f; // Optional
        colorBlending.blendConstants[1] = 0.0f; // Optional
        colorBlending.blendConstants[2] = 0.0f; // Optional
        colorBlending.blendConstants[3] = 0.0f; // Optional

        // For uniform values
        // for now we don't have any but we still need
        // to create an empty pielineLayout
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 0; // Optional
        pipelineLayoutInfo.pSetLayouts = nullptr; // Optional
        pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
        pipelineLayoutInfo.pPushConstantRanges = nullptr; // Optional

        if (vkCreatePipelineLayout(device_, &pipelineLayoutInfo, nullptr, &pipelineLayout_) != VK_SUCCESS) {
            throw std::runtime_error("failed to create pipeline layout!");
        }

        vkDestroyShaderModule(device_, fragShaderModule, nullptr);
        vkDestroyShaderModule(device_, vertShaderModule, nullptr);
    }

    void initVulkan() {
        printExtensions();
        createInstance();
        setupDebugMessenger();
        // TODO: something is wrong here and on functions call nested:
        // createSurface must be called before pickPhysicalDevice and LogicalDevice
        // or add a docstring ? Really the kind of hidden state I dislike with OOP
        createSurface();
        pickPhysicalDevice();
        createLogicalDevice();
        createSwapChain();
        createImageViews();
        createGraphicsPipeline();
    }

    void mainLoop() {
        while (!glfwWindowShouldClose(window_)) {
            glfwPollEvents();
        }
    }

    void cleanup() {
        // Unlike imgages, imageViews have been created manually
        // so we need to destroy them
        for (auto imageView : swapChainImageViews_) {
            vkDestroyImageView(device_, imageView, nullptr);
        }

        // Validation Layer error if we do this before destroying the surface
        vkDestroySwapchainKHR(device_, swapChain_, nullptr);

        // glfw doesn't provide method for this, so us vk call instead
        vkDestroySurfaceKHR(instance_, surface_, nullptr);

        vkDestroyPipelineLayout(device_, pipelineLayout_, nullptr);

        // This is caught by validation layer message if forgotten
        vkDestroyDevice(device_, nullptr);

        // TODO: why moving this on top of function scope
        // or here doesn't change ?
        // mayberelated to validation layer for create/destroyInstance ?
        if (ENABLE_VALIDATION_LAYERS) {
            // Removing this triggers validation layer error on vkDestroy
            // ... The Vulkan spec states: All child objects created using instance must 
            // have been destroyed prior to destroying instance ...
            DestroyDebugUtilsMessengerEXT(instance_, debugMessenger_, nullptr);
        }

        // As we do not use RAII for now, destroy is needed
        vkDestroyInstance(instance_, nullptr);

        glfwDestroyWindow(window_);

        glfwTerminate();
    }
};

int main() {
    HelloTriangleApplication app;

    try {
        app.run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
