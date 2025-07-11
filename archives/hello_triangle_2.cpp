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
#include <fstream>

#include "device.hpp"
#include "swapchain.hpp"
#include "pipeline.hpp"

#ifdef NDEBUG
    const bool enableValidationLayers = false;
#else
    const bool ENABLE_VALIDATION_LAYERS = true;
#endif

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

void errorCallback(int error, const char* description)
{
    fprintf(stderr, "Error: %s\n", description);
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
    VkRenderPass renderPass_;
    VkPipelineLayout pipelineLayout_;
    VkPipeline graphicsPipeline_;
    std::vector<VkFramebuffer> swapChainFramebuffers_;
    VkCommandPool commandPool_;
    VkCommandBuffer commandBuffer_;
    /** Semaphore: blocking wait in GPU not in CPU */
    VkSemaphore imageAvailableSemaphore_;
    /** Semaphore: blocking wait in GPU not in CPU */
    VkSemaphore renderFinishedSemaphore_;
    /** Fence blocking wait on CPU that GPU has finished */
    VkFence inFlightFence_;

    void createSurface() {
        // if the surface object is platform agnostic, its creation is not
        // let glfw handle this for us
        if (glfwCreateWindowSurface(instance_, window_, nullptr, &surface_) != VK_SUCCESS) {
            throw std::runtime_error("failed to create window surface!");
        }
    }

    void setupDebugMessenger() {
        device::setupDebugMessenger(instance_, ENABLE_VALIDATION_LAYERS, &debugMessenger_);
    }

    void createSwapChain() {
        swapchain::createSwapChain(
            window_,
            physicalDevice_,
            surface_,
            device_,
            &swapChain_,
            // TODO: more explicit about mutation in place of this vector ?
            swapChainImages_,
            &swapChainImageFormat_,
            &swapChainExtent_
        );
    }

    void initWindow() {
        glfwSetErrorCallback(errorCallback);
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        window_ = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
        // std::cout << window_ << std::endl;
    }

    void createInstance() {
        if (ENABLE_VALIDATION_LAYERS && !device::checkValidationLayerSupport(VALIDATION_LAYERS)) {
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

            device::populateDebugMessengerCreateInfo(debugCreateInfo);
            createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*) &debugCreateInfo;
        } else {
            createInfo.enabledLayerCount = 0;

            createInfo.pNext = nullptr;
        }

        auto extensions = device::getRequiredExtensions(ENABLE_VALIDATION_LAYERS);
        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();

        // Note: there is a RAII way to do instead of this: find this an migrate
        // to it when the tutorial is finished
        if (vkCreateInstance(&createInfo, nullptr, &instance_) != VK_SUCCESS) {
            throw std::runtime_error("failed to create instance!");
        }
    }

    void pickPhysicalDevice() {
        device::pickPhysicalDevice(
            instance_,
            surface_,
            DEVICE_EXTENSIONS,
            &physicalDevice_
        );
    }

    void createLogicalDevice() {
        device::createLogicalDevice(
            physicalDevice_,
            surface_,
            DEVICE_EXTENSIONS,
            ENABLE_VALIDATION_LAYERS,
            VALIDATION_LAYERS,
            &device_,
            &graphicsQueue_,
            &presentationQueue_
        );
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

    void createRenderPass() {
        pipeline::createRenderPass(
            device_,
            swapChainImageFormat_,
            &renderPass_
        );
    }

    void createGraphicsPipeline() {
        pipeline::createGraphicsPipeline(
            VERT_FILE,
            FRAG_FILE,
            device_,
            swapChainExtent_,
            renderPass_,
            &pipelineLayout_,
            &graphicsPipeline_
        );
    }

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
    void createFramebuffers() {
        auto swapChainImageViewsSize = swapChainImageViews_.size();
        swapChainFramebuffers_.resize(swapChainImageViewsSize);

        for (size_t i = 0; i < swapChainImageViewsSize; i++) {
            // in our case we have only one attachment by framebuffer: 
            // the color attachment
            VkImageView attachments[] = {
                swapChainImageViews_[i]
            };

            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = renderPass_;
            framebufferInfo.attachmentCount = 1;
            framebufferInfo.pAttachments = attachments;
            framebufferInfo.width = swapChainExtent_.width;
            framebufferInfo.height = swapChainExtent_.height;
            // our swapchain images are single images, so nb of layers is 1
            framebufferInfo.layers = 1;

            if (vkCreateFramebuffer(device_, &framebufferInfo, nullptr, &swapChainFramebuffers_[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create framebuffer!");
            }
        }
    }

    void createCommandPool() {
        device::QueueFamilyIndices queueFamilyIndices = device::findQueueFamilies(physicalDevice_, surface_);

        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        // We will be recording a command buffer every frame
        // so we want to be able to reset and rerecord over it
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        // Command buffers are executed by submitting them on one of the device queues
        // record command for drawing so we choose the graphicsFamily queue
        poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

        if (vkCreateCommandPool(device_, &poolInfo, nullptr, &commandPool_) != VK_SUCCESS) {
            throw std::runtime_error("failed to create command pool!");
        }
    }

    void createCommandBuffer() {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = commandPool_;
        // VK_COMMAND_BUFFER_LEVEL_PRIMARY: Can be submitted to a queue 
        // for execution, but cannot be called from other command buffers.
        // VK_COMMAND_BUFFER_LEVEL_SECONDARY: Cannot be submitted directly
        // but can be called from primary command buffers.
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        // only one command buffer allocated
        allocInfo.commandBufferCount = 1;

        if (vkAllocateCommandBuffers(device_, &allocInfo, &commandBuffer_) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate command buffers!");
        }
    }

    /** writes the commands we want to execute into a command buffer. */
    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        // none of the flags applicable for us right now
        beginInfo.flags = 0; // Optional
        // only relevant for secondary command buffer
        beginInfo.pInheritanceInfo = nullptr; // Optional

        // If the command buffer was already recorded once, then a call to vkBeginCommandBuffer 
        // will implicitly reset it. It's not possible to append commands to a buffer at a later time.
        if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("failed to begin recording command buffer!");
        }

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = renderPass_;
        // We created a framebuffer for each swap chain image where it 
        // is specified as a color attachment. 
        // Thus we need to bind the framebuffer for the swapchain image we want to draw to.
        // pick the right framebuffer for the current swapchain image
        renderPassInfo.framebuffer = swapChainFramebuffers_[imageIndex];
        // define the size of the render area
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = swapChainExtent_;
        // for VK_ATTACHMENT_LOAD_OP_CLEAR, which we used as load operation for the color attachment
        // black with 100% opacity
        VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
        renderPassInfo.clearValueCount = 1;
        renderPassInfo.pClearValues = &clearColor;

        // no secondary command buffer so no VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS
        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline_);

        // as we defined viewport and scissor state to be dynamic
        // we need to set them in the command buffer before the draw command
        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(swapChainExtent_.width);
        viewport.height = static_cast<float>(swapChainExtent_.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = swapChainExtent_;
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        /**
         * 
            vertexCount: Even though we don't have a vertex buffer, we technically still have 3 vertices to draw.
            instanceCount: Used for instanced rendering, use 1 if you're not doing that.
            firstVertex: Used as an offset into the vertex buffer, defines the lowest value of gl_VertexIndex.
            firstInstance: Used as an offset for instanced rendering, defines the lowest value of gl_InstanceIndex.
         */
        vkCmdDraw(commandBuffer, 3, 1, 0, 0);

        vkCmdEndRenderPass(commandBuffer);

        // we've finish recording the command buffer
        if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to record command buffer!");
        }
    }

    /**
     * We'll need one semaphore to signal that an image has been acquired from the swapchain
     * and is ready for rendering, another one to signal that rendering has finished 
     * and presentation can happen, and a fence to make sure only one frame is rendering at a time.
     * (1 record on the command buffer for every frame, and we don't want overwriting it while the
     * GPU is using it)
     */
    void createSyncObjects() {
        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        // chicken and egg solvgin by the API:
        // the first call to vkWaitForFences() returns immediately
        // since the fence is already signaled.
        // without it it would block forever
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        if (vkCreateSemaphore(device_, &semaphoreInfo, nullptr, &imageAvailableSemaphore_) != VK_SUCCESS ||
        vkCreateSemaphore(device_, &semaphoreInfo, nullptr, &renderFinishedSemaphore_) != VK_SUCCESS ||
        vkCreateFence(device_, &fenceInfo, nullptr, &inFlightFence_) != VK_SUCCESS) {
            throw std::runtime_error("failed to create semaphores!");
        }
    }

    void drawFrame() {
        vkWaitForFences(device_, 1, &inFlightFence_, VK_TRUE, UINT64_MAX);

        // vkQueue needs VK_NULL_HANDLE or unsignaled fence
        vkResetFences(device_, 1, &inFlightFence_);

        uint32_t imageIndex;
        // extension so vk...KHR naming
        vkAcquireNextImageKHR(
            device_,
            swapChain_,
            // No timeout 
            UINT64_MAX,
            imageAvailableSemaphore_,
            VK_NULL_HANDLE,
            // vkImage in our swapchain array
            &imageIndex
        );

        // record the command buffer
        // make sure the command buffer can be recorded
        vkResetCommandBuffer(commandBuffer_, 0);
        recordCommandBuffer(commandBuffer_, imageIndex);

        // submitting the command buffer
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        VkSemaphore waitSemaphores[] = {imageAvailableSemaphore_};
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer_;
        VkSemaphore signalSemaphores[] = {renderFinishedSemaphore_};
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        if (vkQueueSubmit(graphicsQueue_, 1, &submitInfo, inFlightFence_) != VK_SUCCESS) {
            throw std::runtime_error("failed to submit draw command buffer!");
        }

        // Subpass dependency
        // Suppasses are for transition
        // we have only one subpass but still it needs to be handled
        // TODO: read more about it, I have enough for now and want to draw
        // that first triangle :D
        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0;
        /** the following refer to the dependecies in renderPass
         * renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;
         */
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        // Presentation
        // The last step of drawing a frame is submitting the result back 
        // to the swap chain to have it eventually show up on the screen
        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;
        VkSwapchainKHR swapChains[] = {swapChain_};
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;
        presentInfo.pImageIndices = &imageIndex;
        presentInfo.pResults = nullptr; // Optional

        // submit a request to present an image on the swapchain
        vkQueuePresentKHR(presentationQueue_, &presentInfo);
    }

    void initVulkan() {
        device::printExtensions();
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
        createRenderPass();
        createGraphicsPipeline();
        createFramebuffers();
        createCommandPool();
        createCommandBuffer();
        createSyncObjects();
    }

    void mainLoop() {
        while (!glfwWindowShouldClose(window_)) {
            glfwPollEvents();
            drawFrame();
        }

        vkDeviceWaitIdle(device_);
    }

    void cleanup() {
        // Unlike images, imageViews have been created manually
        // so we need to destroy them
        for (auto imageView : swapChainImageViews_) {
            vkDestroyImageView(device_, imageView, nullptr);
        }

        for (auto framebuffer : swapChainFramebuffers_) {
            vkDestroyFramebuffer(device_, framebuffer, nullptr);
        }

        // Validation Layer error if we do this before destroying the surface
        vkDestroySwapchainKHR(device_, swapChain_, nullptr);

        // glfw doesn't provide method for this, so us vk call instead
        vkDestroySurfaceKHR(instance_, surface_, nullptr);

        vkDestroyRenderPass(device_, renderPass_, nullptr);

        vkDestroyPipeline(device_, graphicsPipeline_, nullptr);

        vkDestroyPipelineLayout(device_, pipelineLayout_, nullptr);

        vkDestroyCommandPool(device_, commandPool_, nullptr);

        vkDestroySemaphore(device_, imageAvailableSemaphore_, nullptr);
        vkDestroySemaphore(device_, renderFinishedSemaphore_, nullptr);
        vkDestroyFence(device_, inFlightFence_, nullptr);

        // This is caught by validation layer message if forgotten
        vkDestroyDevice(device_, nullptr);

        // TODO: why moving this on top of function scope
        // or here doesn't change ?
        // mayberelated to validation layer for create/destroyInstance ?
        if (ENABLE_VALIDATION_LAYERS) {
            std::cout << "Debug Messenger " << debugMessenger_ << std::endl;
            // Removing this triggers validation layer error on vkDestroy
            // ... The Vulkan spec states: All child objects created using instance must 
            // have been destroyed prior to destroying instance ...
            device::destroyDebugUtilsMessengerEXT(instance_, debugMessenger_, nullptr);
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
