

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
#include <chrono>

// Let GLFW include by itslef vulkan headers
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include "glm/glm.hpp"
#include <glm/gtc/matrix_transform.hpp>

#include "device.hpp"
#include "swapchain2.hpp"
#include "pipeline3.hpp"
#include "vertex.hpp"

#ifdef NDEBUG
    const bool enableValidationLayers = false;
#else
    const bool ENABLE_VALIDATION_LAYERS = true;
#endif

const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;
/**
 *  We don't want the CPU to be too much ahead of the GPU, hence 2 frames in flight
 *  Note that rougher primitives like vkDeviceWaitIdle are also possible
 */
const int MAX_FRAMES_IN_FLIGHT = 2;

const std::vector<const char*> VALIDATION_LAYERS = {
    "VK_LAYER_KHRONOS_validation"
};

// we use the macro to avoid mispelling
// "VK_KHR_swapchain"
const std::vector<const char*> DEVICE_EXTENSIONS = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

const auto VERT_FILE = "./shaders/spirv/shader3.vert.spirv";
const auto FRAG_FILE = "./shaders/spirv/shader1.frag.spirv";

void errorCallback(int error, const char* description)
{
    fprintf(stderr, "Error: %s\n", description);
}

// interleaving vertex attributes
// attributes combined in one array of vertices
const std::vector<vertex::Vertex> vertices = {
    {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
    {{0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
    {{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
    {{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}}
};

// possible to use uint16_t or uint32_t
// 16 bits are ok are we are way above 65535
const std::vector<uint16_t> indices = {
    0, 1, 2, 2, 3, 0
};

/**
 * The data in the matrices is binary compatible with the way 
 * the shader expects it, so we can later just memcpy a UniformBufferObject to a VkBuffer.
 * 
 * alignas is to ensure proper memory alignment with Vulkan
 * here for the 3 matricies the default will be OK
 * but beeing explicit could avoid gotchas with more complicated
 * or nested structs
 */
struct UniformBufferObject {
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
};

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
    VkDescriptorSetLayout descriptorSetLayout_;
    VkPipelineLayout pipelineLayout_;
    VkPipeline graphicsPipeline_;
    std::vector<VkFramebuffer> swapChainFramebuffers_;
    VkCommandPool commandPool_;
    std::vector<VkCommandBuffer> commandBuffers_;
    /** Semaphore: blocking wait in GPU not in CPU */
    std::vector<VkSemaphore> imageAvailableSemaphores_;
    /** Semaphore: blocking wait in GPU not in CPU */
    std::vector<VkSemaphore> renderFinishedSemaphores_;
    /** Fence blocking wait on CPU that GPU has finished */
    std::vector<VkFence> inFlightFences_;
    /** keep track of the current frame */
    uint32_t currentFrame_ = 0;
    /**
     *  May be used if drivers does not trigger
     *  VK_ERROR_OUT_OF_DATE_KHR when window is resized
     */
    bool framebufferResized_ = false;
    VkBuffer vertexBuffer_;
    VkDeviceMemory vertexBufferMemory_;
    VkBuffer indexBuffer_;
    VkDeviceMemory indexBufferMemory_;
    std::vector<VkBuffer> uniformBuffers_;
    std::vector<VkDeviceMemory> uniformBuffersMemory_;
    std::vector<void*> uniformBuffersMapped_;
    VkDescriptorPool descriptorPool_;
    std::vector<VkDescriptorSet> descriptorSets_;

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
        swapchain2::createSwapChain(
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

    /** 
     * glfw needs static function because it can't handle properly the this pointer
     * this pointer is retrieved through window pointer and a, IMO
     * kind of dirty hook with glfwSetWindowUserPointer and glfwGetWindowUserPointer
     * */
    static void framebufferResizeCallback(GLFWwindow* window, int width, int height) {
        auto app = reinterpret_cast<HelloTriangleApplication*>(glfwGetWindowUserPointer(window));
        app->framebufferResized_ = true;
    }


    void initWindow() {
        glfwSetErrorCallback(errorCallback);
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        // glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        window_ = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
        // this allow to store an arbitrary pointer, and we need this, litteraly :D
        // for framebufferResizeCallback
        glfwSetWindowUserPointer(window_, this);
        glfwSetFramebufferSizeCallback(window_, framebufferResizeCallback);
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
        swapchain2::createImageViews(
            device_,
            swapChainImages_,
            swapChainImageFormat_,
            swapChainImageViews_
        );
    }

    void createRenderPass() {
        pipeline3::createRenderPass(
            device_,
            swapChainImageFormat_,
            renderPass_
        );
    }

    /**
     * A descriptor is a way for shaders to freely access resources like buffers and images.
     * We will use it for uniforms (uniforms exist to avoid copy for exemple a view model projection
     * matric for each fram in a vertex buffer)
     */
    void createDescriptorSetLayout() {
        VkDescriptorSetLayoutBinding uboLayoutBinding{};
        uboLayoutBinding.binding = 0;
        uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        // with have only one MVP
        uboLayoutBinding.descriptorCount = 1;
        // in which shader stage it will be referenced
        uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        // only relevant for image sampling descriptors, optional
        uboLayoutBinding.pImmutableSamplers = nullptr;

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings = &uboLayoutBinding;

        if (vkCreateDescriptorSetLayout(device_, &layoutInfo, nullptr, &descriptorSetLayout_) != VK_SUCCESS) {
            throw std::runtime_error("failed to create descriptor set layout!");
        }
    }

    void createGraphicsPipeline() {
        pipeline3::createGraphicsPipeline(
            VERT_FILE,
            FRAG_FILE,
            device_,
            swapChainExtent_,
            renderPass_,
            descriptorSetLayout_,
            pipelineLayout_,
            graphicsPipeline_
        );
    }

    void createFramebuffers() {
        swapchain2::createFramebuffers(
            device_,
            swapChainImageViews_,
            swapChainExtent_,
            renderPass_,
            swapChainFramebuffers_
        );
    }

    void cleanupSwapChain() {
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
    }

    /**
     * The disadvantage of this approach is that we need to stop all rendering before creating the new swap chain.
     * It is possible to create a new swap chain while drawing commands on an image from the old swap chain
     * are still in-flight. You need to pass the previous swap chain to the oldSwapChain field in the 
     * VkSwapchainCreateInfoKHR struct and destroy the old swap chain as soon as you've finished using it.
     * 
     * Also, note that we don't recreate the renderpass here for simplicity. In theory it can be possible
     * for the swap chain image format to change during an applications' lifetime, e.g. when moving a window
     * from an standard range to an high dynamic range monitor. This may require the application to recreate
     * the renderpass to make sure the change between dynamic ranges is properly reflected
     * 
     * Also, note that in chooseSwapExtent we already query the new window resolution to make sure that the
     * swap chain images have the (new) right size, so there's no need to modify chooseSwapExtent 
     * (remember that we already had to use glfwGetFramebufferSize get the resolution of the surface in 
     * pixels when creating the swap chain).
     */
    void recreateSwapChain() {
        // custom handling of minimization:
        // we wait until it is over
        int width = 0, height = 0;
        glfwGetFramebufferSize(window_, &width, &height);

        while (width == 0 || height == 0) {
            glfwGetFramebufferSize(window_, &width, &height);
            glfwWaitEvents();
        }

        // don't touch resources while they may be in use
        vkDeviceWaitIdle(device_);

        cleanupSwapChain();

        createSwapChain();
        createImageViews();
        createFramebuffers();
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

    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(physicalDevice_, &memProperties);

        /**
         * The VkPhysicalDeviceMemoryProperties structure has two arrays 
         * memoryTypes and memoryHeaps. 
         * Memory heaps are distinct memory resources like dedicated VRAM 
         * and swap space in RAM for when VRAM runs out. 
         * The different types of memory exist within these heaps. 
         * Right now we'll only concern ourselves with the type of memory 
         * and not the heap it comes from, but you can imagine 
         * that this can affect performance.
         */

        /**
         * The typeFilter parameter will be used to specify the bit field of memory types that are suitable. 
         * That means that we can find the index of a suitable memory type by simply iterating 
         * over them and checking if the corresponding bit is set to 1.
         */

        /**
         * However, we're not just interested in a memory type that is suitable for the vertex buffer. 
         * We also need to be able to write our vertex data to that memory. 
         * The memoryTypes array consists of VkMemoryType structs that specify the heap and properties
         * of each type of memory. The properties define special features of the memory, 
         * like being able to map it so we can write to it from the CPU. This property is indicated 
         * with VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, but we also need to use the 
         * VK_MEMORY_PROPERTY_HOST_COHERENT_BIT property. We'll see why when we map the memory
         */
        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
            if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
            }
        }

        throw std::runtime_error("failed to find suitable memory type!");
    }

    /**
    * Unlike the Vulkan objects we've been dealing with so far, buffers
    * do not automatically allocate memory for themselves
    */
    void createBuffer(
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        VkMemoryPropertyFlags properties,
        VkBuffer& buffer,
        VkDeviceMemory& bufferMemory
    ) {
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        // like images in the swap chain vb can be owned by a specific queue family
        // or shared between multiple at the same time
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        // The flags parameter is used to configure sparse buffer memory, 
        // which is not relevant right now. 
        // We'll leave it at the default value of 0.
        bufferInfo.flags = 0;

        if (vkCreateBuffer(device_, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to create vertex buffer!");
        }

        // buffer has been created but no memory is assigned to it yet
        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(device_, buffer, &memRequirements);
        
        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(
            memRequirements.memoryTypeBits, 
            properties
        );

        if (vkAllocateMemory(device_, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate vertex buffer memory!");
        }

        // memory allocation successful, so bind it to the buffer
        vkBindBufferMemory(device_, buffer, bufferMemory, 0);
    }

    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        /**
         * You may wish to create a separate command pool for these kinds of short-lived buffers,
         * because the implementation may be able to apply memory allocation optimizations.
         * You should use the VK_COMMAND_POOL_CREATE_TRANSIENT_BIT flag during command pool
         * generation in that case.
         * 
         * TODO: create a command buffer, pool function
         * TODO: dedicated command pool for memory transfer
         * for now we use the command commandPool_ but WARNING: it has to be created
         * before calling this function, or it will SEGFAULT at vkAllocateCommandBuffers
         */
        allocInfo.commandPool = commandPool_;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer;
        vkAllocateCommandBuffers(device_, &allocInfo, &commandBuffer);

        // start recording
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        // We're only going to use the command buffer once and wait with returning 
        // from the function until the copy operation has finished executing
        // good practice: tell the driver our intent
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(commandBuffer, &beginInfo);

        // we have to use regions array (of VkBufferCopy structs)
        // we can't use VK_WHOLE_SIzE like VkMapMemory
        VkBufferCopy copyRegion{};
        copyRegion.srcOffset = 0; // Optional
        copyRegion.dstOffset = 0; // Optional
        copyRegion.size = size;
        vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

        // end recording
        vkEndCommandBuffer(commandBuffer);

        // execute the command buffer to complete the transfer
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        vkQueueSubmit(graphicsQueue_, 1, &submitInfo, VK_NULL_HANDLE);
        // there is no event to wait, unlike the draw command
        // we juste want to execute immediately
        // two way to wait for the transfer to complete:
        // * vkWaitForFence (would allow us to schedule multiple transfer simultaneously)
        // * vkQueueWaitIdle
        vkQueueWaitIdle(graphicsQueue_);

        vkFreeCommandBuffers(device_, commandPool_, 1, &commandBuffer);
    }

    void createVertexBuffer() {
        VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;

        createBuffer(
            bufferSize,
            // no more VK_BUFFER_USAGE_VERTEX_BUFFER_BIT as we are
            // creating a staging buffer
            // Buffer can be used as source in a memory transfer operation.
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            stagingBuffer,
            stagingBufferMemory
        );

        // fill the staging buffer
        void* data;
        vkMapMemory(device_, stagingBufferMemory, 0, bufferSize, 0, &data);

        /**
         * Unfortunately the driver may not immediately copy the data 
         * into the buffer memory, for example because of caching. 
         * It is also possible that writes to the buffer are not visible 
         * in the mapped memory yet. There are two ways to deal with that problem:
         * * Use a memory heap that is host coherent, indicated with VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
         * * Call vkFlushMappedMemoryRanges after writing to the mapped memory, 
         * and call vkInvalidateMappedMemoryRanges before reading from the mapped memory
         * 
         * We went for the first approach, which ensures that the mapped memory always matches
         * the contents of the allocated memory. Do keep in mind that this may lead to slightly 
         * worse performance than explicit flushing.bufferSize
         * 
         * But now we are deadling with a staging buffer, TODO: does it matter with a staging buffer ?
         */
        memcpy(data, vertices.data(), static_cast<size_t>(bufferSize));
        vkUnmapMemory(device_, stagingBufferMemory);

        createBuffer(
            bufferSize,
            // vkMap usualy not possible as device local
            // hence we specify it can be used as a transfer destination
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            // The most optimal memory on the GPU, but usually not accessible from the CPU
            // hence the use of a staging buffer
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            vertexBuffer_,
            vertexBufferMemory_
        );

        copyBuffer(stagingBuffer, vertexBuffer_, bufferSize);

        // we can now clean the staging buffer
        vkDestroyBuffer(device_, stagingBuffer, nullptr);
        vkFreeMemory(device_, stagingBufferMemory, nullptr);
    }

    void createIndexBuffer() {
        VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        createBuffer(
            bufferSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            stagingBuffer,
            stagingBufferMemory
        );

        void* data;
        vkMapMemory(device_, stagingBufferMemory, 0, bufferSize, 0, &data);
        memcpy(data, indices.data(), (size_t) bufferSize);
        vkUnmapMemory(device_, stagingBufferMemory);

        createBuffer(
            bufferSize,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            indexBuffer_,
            indexBufferMemory_
        );

        copyBuffer(stagingBuffer, indexBuffer_, bufferSize);

        vkDestroyBuffer(device_, stagingBuffer, nullptr);
        vkFreeMemory(device_, stagingBufferMemory, nullptr);
    }

    /**
     * We're going to copy new data to the uniform buffer every frame,
     * so it doesn't really make any sense to have a staging buffer.
     * It would just add extra overhead in this case and likely degrade performance instead of improving it.
     * We should have multiple buffers,
     * because multiple frames may be in flight at the same time and we don't want to update the buffer
     * in preparation of the next frame while a previous one is still reading from it! Thus, we need
     * to have as many uniform buffers as we have frames in flight, and write to a uniform buffer
     * that is not currently being read by the GPU
     */
    void createUniformBuffers() {
        VkDeviceSize bufferSize = sizeof(UniformBufferObject);

        uniformBuffers_.resize(MAX_FRAMES_IN_FLIGHT);
        uniformBuffersMemory_.resize(MAX_FRAMES_IN_FLIGHT);
        uniformBuffersMapped_.resize(MAX_FRAMES_IN_FLIGHT);

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            createBuffer(
                bufferSize,
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                uniformBuffers_[i],
                uniformBuffersMemory_[i]);

            // The buffer stays mapped the whole application time
            // as maping has a cost, it is best to avoid doing it every time
            // this is called "persistent mapping"
            vkMapMemory(device_, uniformBuffersMemory_[i], 0, bufferSize, 0, &uniformBuffersMapped_[i]);
        }
    }

    void createCommandBuffers() {
        commandBuffers_.resize(MAX_FRAMES_IN_FLIGHT);

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = commandPool_;
        // VK_COMMAND_BUFFER_LEVEL_PRIMARY: Can be submitted to a queue 
        // for execution, but cannot be called from other command buffers.
        // VK_COMMAND_BUFFER_LEVEL_SECONDARY: Cannot be submitted directly
        // but can be called from primary command buffers.
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers_.size());

        if (vkAllocateCommandBuffers(device_, &allocInfo, commandBuffers_.data()) != VK_SUCCESS) {
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

        VkBuffer vertexBuffers[] = {vertexBuffer_};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

        // we can have only one index buffer
        // we used 16 bit for storage
        vkCmdBindIndexBuffer(commandBuffer, indexBuffer_, 0, VK_INDEX_TYPE_UINT16);

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

        vkCmdBindDescriptorSets(
            commandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipelineLayout_,
            0,
            1,
            &descriptorSets_[currentFrame_],
            0,
            nullptr
        );

        vkCmdDrawIndexed(
            commandBuffer,
            // now index count instead of vertex count as we draw indexed
            static_cast<uint32_t>(indices.size()),
            // instanceCount: Used for instanced rendering, use 1 if you're not doing that.
            1,
            // first index
            0,
            // offset to add to the indices in the index buffer
            0,
            // firstInstance, we don't use instance.
            0 
        );

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
        imageAvailableSemaphores_.resize(MAX_FRAMES_IN_FLIGHT);
        renderFinishedSemaphores_.resize(MAX_FRAMES_IN_FLIGHT);
        inFlightFences_.resize(MAX_FRAMES_IN_FLIGHT);

        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        // chicken and egg solvgin by the API:
        // the first call to vkWaitForFences() returns immediately
        // since the fence is already signaled.
        // without it it would block forever
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            if (vkCreateSemaphore(device_, &semaphoreInfo, nullptr, &imageAvailableSemaphores_[i]) != VK_SUCCESS ||
            vkCreateSemaphore(device_, &semaphoreInfo, nullptr, &renderFinishedSemaphores_[i]) != VK_SUCCESS ||
            vkCreateFence(device_, &fenceInfo, nullptr, &inFlightFences_[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create semaphores and fences!");
            }
        }
    }

    /** 
     * geometry rotates 90 degrees per second regardless of frame rate.
     * 
     * TODO: proper delta time
     * */
    void updateUniformBuffer(uint32_t currentImage) {
        static auto startTime = std::chrono::high_resolution_clock::now();

        auto currentTime = std::chrono::high_resolution_clock::now();
        float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

        // rotation around the z axis
        UniformBufferObject ubo{};
        ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));

        // look from above at 45 degres angle
        ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));

        ubo.proj = glm::perspective(
            // 45 degrees vertical fov
            glm::radians(45.0f),
            // aspect ratio
            swapChainExtent_.width / static_cast<float>(swapChainExtent_.height),
            // near plane
            0.1f,
            // far plane
            10.0f
        );

        // trick because glm is for opengl, where y axis is inverted
        // here flip the sign of the scaling factor on th y axis
        ubo.proj[1][1] *= -1;

        // all transformations defined, we can copy
        // no staging buffer, and memory already mapped
        // it is not the most optimal way of doing (see push constants)
        memcpy(uniformBuffersMapped_[currentImage], &ubo, sizeof(ubo));
    }

    void drawFrame() {
        vkWaitForFences(device_, 1, &inFlightFences_[currentFrame_], VK_TRUE, UINT64_MAX);
        

        uint32_t imageIndex;
        // extension so vk...KHR naming
        VkResult result = vkAcquireNextImageKHR(
            device_,
            swapChain_,
            // No timeout 
            UINT64_MAX,
            imageAvailableSemaphores_[currentFrame_],
            VK_NULL_HANDLE,
            // vkImage in our swapchain array
            &imageIndex
        );

        // VK_ERROR_OUT_OF_DATE_KHR: swapchain incompatible with the surface.
        // usally happens when window is resized
        // VK_SUBOPTIMAL_KHR: swapchain usable but surface properties are not matched exactly
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            recreateSwapChain();
            // try again in the next drawFrame call
            return;
        // 
        } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            throw std::runtime_error("failed to acquire swap chain image!");
        }

        // Only reset the fence if we are submitting work
        // has we used early return pattern in the lines before
        // vkQueue needs VK_NULL_HANDLE or unsignaled fence
        vkResetFences(device_, 1, &inFlightFences_[currentFrame_]);

        // record the command buffer
        // make sure the command buffer can be recorded
        vkResetCommandBuffer(commandBuffers_[currentFrame_], 0);
        recordCommandBuffer(commandBuffers_[currentFrame_], imageIndex);

        updateUniformBuffer(currentFrame_);

        // submitting the command buffer
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        VkSemaphore waitSemaphores[] = {imageAvailableSemaphores_[currentFrame_]};
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffers_[currentFrame_];
        VkSemaphore signalSemaphores[] = {renderFinishedSemaphores_[currentFrame_]};
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        if (vkQueueSubmit(graphicsQueue_, 1, &submitInfo, inFlightFences_[currentFrame_]) != VK_SUCCESS) {
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
        result = vkQueuePresentKHR(presentationQueue_, &presentInfo);


        /**
         * It is important to do this after vkQueuePresentKHR to ensure that the semaphores are in a consistent 
         * state, otherwise a signaled semaphore may never be properly waited upon
         * 
         * TODO: looks not very DRY with the calls on top of drawFrame
         * why not using vkAcquireNextImageKHR result and early return instead ? 
         */
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized_) {
            framebufferResized_ = false;
            recreateSwapChain();
        } else if (result != VK_SUCCESS) {
            throw std::runtime_error("failed to present swap chain image!");
        }

        currentFrame_ = (currentFrame_ + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    /**
     * Descriptor sets can't be created directly, they must be allocated from a pool like command buffers
     */
    void createDescriptorPool() {
        VkDescriptorPoolSize poolSize{};
        poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        // we will allocate one descriptor set by frame
        poolSize.descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = &poolSize;
        poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
        // We're not going to touch the descriptor set after creating it, so we don't need this flag
        // optional
        poolInfo.flags = 0;

        if (vkCreateDescriptorPool(device_, &poolInfo, nullptr, &descriptorPool_) != VK_SUCCESS) {
            throw std::runtime_error("failed to create descriptor pool!");
        }
    }

    /**
     * The descriptor layout describes the type of descriptors that can be bound.
     * Here we're going to create a descriptor set for each VkBuffer resource to bind
     * it to the uniform buffer descriptor.
     */
    void createDescriptorSets() {
        // one descriptor set for each frame in flight,
        // all with the same layout
        std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, descriptorSetLayout_);
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = descriptorPool_;
        allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
        allocInfo.pSetLayouts = layouts.data();

        descriptorSets_.resize(MAX_FRAMES_IN_FLIGHT);
        // this calls allocate descriptor sets, each with one buffer descriptor
        if (vkAllocateDescriptorSets(device_, &allocInfo, descriptorSets_.data()) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate descriptor sets!");
        }

        // configure the desciptor sets we just allocated
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            VkDescriptorBufferInfo bufferInfo{};
            bufferInfo.buffer = uniformBuffers_[i];
            bufferInfo.offset = 0;
            // we could also use VK_WHOLE_SIZE here as
            // we overwrite the whole buffer
            bufferInfo.range = sizeof(UniformBufferObject);

            VkWriteDescriptorSet descriptorWrite{};
            descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrite.dstSet = descriptorSets_[i];
            descriptorWrite.dstBinding = 0;
            // descriptors could be array, here it is not
            // so the index is 0
            descriptorWrite.dstArrayElement = 0;
            // specify the type of decriptor ... again :(
            descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            // in case of an array, use only 1. Not it is starting at dstArrayElement
            descriptorWrite.descriptorCount = 1;
            descriptorWrite.pBufferInfo = &bufferInfo;
            descriptorWrite.pImageInfo = nullptr; // Optional
            descriptorWrite.pTexelBufferView = nullptr; // Optional

            // accepts two kind of array:
            // VkWriteDescriptorSet and an array of VkCopyDescriptorSet
            vkUpdateDescriptorSets(device_, 1, &descriptorWrite, 0, nullptr);
        }

        
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
        createDescriptorSetLayout();
        createGraphicsPipeline();
        createFramebuffers();
        createCommandPool();
        createVertexBuffer();
        createIndexBuffer();
        createUniformBuffers();
        createDescriptorPool();
        createDescriptorSets();
        createCommandBuffers();
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
        cleanupSwapChain();

        vkDestroyBuffer(device_, vertexBuffer_, nullptr);
        vkFreeMemory(device_, vertexBufferMemory_, nullptr);

        vkDestroyBuffer(device_, indexBuffer_, nullptr);
        vkFreeMemory(device_, indexBufferMemory_, nullptr);

        // glfw doesn't provide method for this, so us vk call instead
        vkDestroySurfaceKHR(instance_, surface_, nullptr);

        vkDestroyRenderPass(device_, renderPass_, nullptr);

        vkDestroyPipeline(device_, graphicsPipeline_, nullptr);

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            vkDestroyBuffer(device_, uniformBuffers_[i], nullptr);
            vkFreeMemory(device_, uniformBuffersMemory_[i], nullptr);
        }

        // this will destroy the pool and its descriptor sets
        vkDestroyDescriptorPool(device_, descriptorPool_, nullptr);

        vkDestroyDescriptorSetLayout(device_, descriptorSetLayout_, nullptr);

        vkDestroyPipelineLayout(device_, pipelineLayout_, nullptr);

        vkDestroyCommandPool(device_, commandPool_, nullptr);

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            vkDestroySemaphore(device_, renderFinishedSemaphores_[i], nullptr);
            vkDestroySemaphore(device_, imageAvailableSemaphores_[i], nullptr);
            vkDestroyFence(device_, inFlightFences_[i], nullptr);
        }

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
