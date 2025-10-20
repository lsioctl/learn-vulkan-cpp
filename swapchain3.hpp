#pragma once

// Let GLFW include by itslef vulkan headers
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <optional>
#include <vector>

namespace swapchain3 {

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

/**
 * For the color space we'll use SRGB if it is available, 
 * because it results in more accurate perceived colors. 
 * It is also pretty much the standard color space for images, like the textures we'll use later on. 
 * Because of that we should also use an SRGB color format, of which one of the most common 
 * ones is VK_FORMAT_B8G8R8A8_SRGB.
 * 
 * If it fails it will return the first item of availableFormats
 */
VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    
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
VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentationModes);

void createSwapChain(
    GLFWwindow* window,
    VkPhysicalDevice physicalDevice,
    VkSurfaceKHR surface,
    VkDevice logicalDevice,
    VkSwapchainKHR* pSwapChain,
    std::vector<VkImage>& swapChainImages,
    VkFormat* pSwapChainImageFormat,
    VkExtent2D* pSwapChainExtent 
);

void createImageViews(
    VkDevice logicalDevice,
    const std::vector<VkImage>& swapChainImages,
    VkFormat swapChainImageFormat,
    std::vector<VkImageView>& swapChainImageViews,
    uint32_t mipLevels
);

/**
 * we've set up the render pass to expect a single framebuffer with the 
 * same format as the swap chain images
 * 
 * The attachments specified during render pass creation are bound by 
 * wrapping them into a VkFramebuffer object.
 *  
 * A framebuffer object references all of the VkImageView objects 
 * that represent the attachments. 
 * 
 * The image that we have to use for the attachment depends on which image
 * the swap chain returns when we retrieve one for presentation.
 * That means that we have to create a framebuffer for all of the images
 * in the swap chain and use the one that corresponds to the retrieved image 
 * at drawing time.
 */
void createFramebuffers(
    VkDevice logicalDevice,
    const std::vector<VkImageView>& swapChainImageViews,
    VkExtent2D swapChainExtent,
    VkImageView depthImageView,
    VkImageView colorImageView,
    VkRenderPass renderPass,
    std::vector<VkFramebuffer>& swapChainFramebuffers
    
);

}