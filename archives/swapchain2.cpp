#include <iostream>
#include <cstring>
#include <set>
#include <limits> // Necessary for std::numeric_limits
#include <algorithm> // Necessary for std::clamp
#include <vector>

#include "swapchain2.hpp"
#include "device.hpp"
#include "image.hpp"

namespace swapchain2 {


SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface) {
    SwapChainSupportDetails details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

    if (formatCount != 0) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

    if (presentModeCount != 0) {
        details.presentationModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentationModes.data());
    }

    return details;
}


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
VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, GLFWwindow* window) {
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    } else {
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);

        VkExtent2D actualExtent = {
            static_cast<uint32_t>(width),
            static_cast<uint32_t>(height)
        };

        actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

        return actualExtent;
    }
}

void createSwapChain(
    GLFWwindow* window,
    VkPhysicalDevice physicalDevice,
    VkSurfaceKHR surface,
    VkDevice logicalDevice,
    VkSwapchainKHR* pSwapChain,
    std::vector<VkImage>& swapChainImages,
    VkFormat* pSwapChainImageFormat,
    VkExtent2D* pSwapChainExtent 
) {
    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice, surface);
    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentationModes);
    VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities, window);

    // recommended: min image + 1
    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    // if maxImageCount == 0 => unlimited
    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    std::cout << "Min image count for the swapchain: " << imageCount << std::endl;

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface;
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

    device::QueueFamilyIndices indices = device::findQueueFamilies(physicalDevice, surface);
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

    if (vkCreateSwapchainKHR(logicalDevice, &createInfo, nullptr, pSwapChain) != VK_SUCCESS) {
        throw std::runtime_error("failed to create swap chain!");
    }

    vkGetSwapchainImagesKHR(logicalDevice, *pSwapChain, &imageCount, nullptr);
    swapChainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(logicalDevice, *pSwapChain, &imageCount, swapChainImages.data());

    *pSwapChainImageFormat = surfaceFormat.format;
    *pSwapChainExtent = extent;
}

void createImageViews(
    VkDevice logicalDevice,
    const std::vector<VkImage>& swapChainImages,
    VkFormat swapChainImageFormat,
    std::vector<VkImageView>& swapChainImageViews
) {
    auto swapChainImageSize = swapChainImages.size();
    swapChainImageViews.resize(swapChainImageSize);

    for (size_t i = 0; i < swapChainImageSize; i++) {
        swapChainImageViews[i] = image::createImageView(logicalDevice, swapChainImages[i], swapChainImageFormat);
    }
}


void createFramebuffers(
    VkDevice logicalDevice,
    const std::vector<VkImageView>& swapChainImageViews,
    VkExtent2D swapChainExtent,
    VkRenderPass renderPass,
    std::vector<VkFramebuffer>& swapChainFramebuffers
    
) {
    auto swapChainImageViewsSize = swapChainImageViews.size();
    swapChainFramebuffers.resize(swapChainImageViewsSize);

    for (size_t i = 0; i < swapChainImageViewsSize; i++) {
        // in our case we have only one attachment by framebuffer: 
        // the color attachment
        VkImageView attachments[] = {
            swapChainImageViews[i]
        };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = swapChainExtent.width;
        framebufferInfo.height = swapChainExtent.height;
        // our swapchain images are single images, so nb of layers is 1
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(logicalDevice, &framebufferInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create framebuffer!");
        }
    }
}


}