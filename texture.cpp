#include <stdexcept>

// this has to be in one (and only ?) one file in the project
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "texture.hpp"
#include "buffer.hpp"
#include "commandbuffer.hpp"
#include "image.hpp"

namespace texture {

/**
 * If we were still using buffers, then we could now write a function to record and execute vkCmdCopyBufferToImage to finish the job,
 * but this command requires the image to be in the right layout first. 
 */
void transitionImageLayout(
    VkDevice logicalDevice,
    VkCommandPool commandPool,
    VkQueue graphicsQueue,
    VkImage image,
    VkFormat format,
    VkImageLayout oldLayout,
    VkImageLayout newLayout
) {
    VkCommandBuffer commandBuffer = commandbuffer::beginSingleTimeCommands(logicalDevice, commandPool);

    // A pipeline barrier is used to synchronize access to resources
    // like writing to a buffer is complete before reading it
    // It can also be used to transition image layout
    // or transfer queue family ownership when VK_SHARING_MODE_EXCLUSIVE is used
    // there is an equivalent BufferMemoryBarrier for buffers
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    // we don't want to transfer queue family ownership
    // careful: those are not default values
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    /**
     * There are two transitions we need to handle:

      * Undefined → transfer destination: transfer writes that don't need to wait on anything
      * Transfer destination → shader reading: shader reads should wait on transfer writes, 
      specifically the shader reads in the fragment shader, because that's where we're going to use the texture

     */

    /** which pipeline stage the operations occur that should happen before the barrier */
    VkPipelineStageFlags sourceStage;
    /** the pipeline stage in which operations will wait on the barrier */
    VkPipelineStageFlags destinationStage;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        // which operation must happen before the barrier
        // and which must wait on the barrier
        // needed even if we already use vkQueueWaitIdle to manually synchronize
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        
        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        // which operation must happen before the barrier
        // and which must wait on the barrier
        // needed even if we already use vkQueueWaitIdle to manually synchronize
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else {
        throw std::invalid_argument("unsupported layout transition!");
    }

    // All types of pipeline barriers are submitted using the same function.
    vkCmdPipelineBarrier(
        commandBuffer,
        sourceStage,
        destinationStage,
        // 0 or VK_DEPENDENCY_BY_REGION_BIT
        // turns the barrier into a region condition
        0,
        // Not memory barrier
        0,
        nullptr,
        // Not buffer memory barrier
        0,
        nullptr,
        // But image memory barrier
        1,
        &barrier
    );


    commandbuffer::endAndExecuteSingleTimeCommands(
        logicalDevice,
        commandPool,
        graphicsQueue,
        commandBuffer
    );
}

void copyBufferToImage(
    VkDevice logicalDevice,
    VkCommandPool commandPool,
    VkQueue graphicsQueue,
    VkBuffer buffer,
    VkImage image,
    uint32_t width,
    uint32_t height
    ) {
    VkCommandBuffer commandBuffer = commandbuffer::beginSingleTimeCommands(logicalDevice, commandPool);

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    // how pixel are laid out in memory
    // here no padding between the rows of an image
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;

    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;

    region.imageOffset = {0, 0, 0};
    region.imageExtent = {
        width,
        height,
        1
    };

    vkCmdCopyBufferToImage(
        commandBuffer,
        buffer,
        image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &region
    );

    commandbuffer::endAndExecuteSingleTimeCommands(
        logicalDevice,
        commandPool,
        graphicsQueue,
        commandBuffer
    );
}

void bindImage(
    VkPhysicalDevice physicalDevice,
    VkDevice logicalDevice,
    uint32_t width,
    uint32_t height,
    VkFormat format,
    VkImageTiling tiling,
    VkImageUsageFlags usage,
    VkMemoryPropertyFlags properties,
    VkImage& image,
    VkDeviceMemory& imageMemory
) {
    /**
     * Although we could set up the shader to access the pixel values in the buffer, 
     * it's better to use image objects in Vulkan for this purpose. Image objects 
     * will make it easier and faster to retrieve colors by allowing us to use 
     * 2D coordinates, for one.
     * 
     * Pixels within an image object are known as texels
     */
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    // tell Vulkan with what kind of coordinate system the texels in the image are going to be addressed.
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    // extent: dimensions, how many texels in each axis
    imageInfo.extent.width = static_cast<uint32_t>(width);
    imageInfo.extent.height = static_cast<uint32_t>(height);
    imageInfo.extent.depth = 1;
    // no mipmapping for now
    imageInfo.mipLevels = 1;
    // not an array
    imageInfo.arrayLayers = 1;
    // we should use the same format for the texels as the pixels in the buffer, 
    // otherwise the copy operation will fail.
    // TODO: is is safe as a parameter
    imageInfo.format = format;
    /**
     * 
        * VK_IMAGE_TILING_LINEAR: Texels are laid out in row-major order like our pixels array
        * VK_IMAGE_TILING_OPTIMAL: Texels are laid out in an implementation defined order for optimal access

        Unlike the layout of an image, the tiling mode cannot be changed at a later time. 
        If you want to be able to directly access texels in the memory of the image, then you must use 
        VK_IMAGE_TILING_LINEAR. We will be using a staging buffer instead of a staging image, 
        so this won't be necessary. 
        We will be using VK_IMAGE_TILING_OPTIMAL for efficient access from the shader.
     */
    imageInfo.tiling = tiling;
    /**
     * There are only two possible values for the initialLayout of an image:

        * VK_IMAGE_LAYOUT_UNDEFINED: Not usable by the GPU and the very first transition will discard the texels.
        * VK_IMAGE_LAYOUT_PREINITIALIZED: Not usable by the GPU, but the first transition will preserve the texels.

        There are few situations where it is necessary for the texels to be preserved during the first transition. 
        One example, however, would be if you wanted to use an image as a staging image in combination 
        with the VK_IMAGE_TILING_LINEAR layout. In that case, you'd want to upload the texel data 
        to it and then transition the image to be a transfer source without losing the data. 
        In our case, however, we're first going to transition the image to be a transfer destination 
        and then copy texel data to it from a buffer object, so we don't need this property and can 
        safely use VK_IMAGE_LAYOUT_UNDEFINED.
     */
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    // TODO: is it safe as parameters ?
    imageInfo.usage = usage;
    // only one queue (graphics)
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    // for multisampling
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.flags = 0; // Optional

    if (vkCreateImage(logicalDevice, &imageInfo, nullptr, &image) != VK_SUCCESS) {
        throw std::runtime_error("failed to create image!");
    }

    // Allocating memory for an image works the same way as allocating memory for a buffer
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(logicalDevice, image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = buffer::findMemoryType(
        physicalDevice,
        memRequirements.memoryTypeBits,
        properties
    );

    if (vkAllocateMemory(logicalDevice, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate image memory!");
    }

    // Same for binding image memory and buffer memory
    vkBindImageMemory(logicalDevice, image, imageMemory, 0);
}

void createTextureImage(
    VkPhysicalDevice physicalDevice,
    VkDevice logicalDevice,
    VkCommandPool commandPool,
    VkQueue graphicsQueue,
    VkImage& textureImage,
    VkDeviceMemory& textureImageMemory
) {
    const char* texturePath = "textures/statue-1275469_1280.jpg";

    int texWidth;
    int texHeight;
    int texChannels;

    stbi_uc* pixels = stbi_load(texturePath, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    // The pixels are laid out row by row with 4 bytes per pixel in the case of STBI_rgb_alpha
    VkDeviceSize imageSize = texWidth * texHeight * 4;

    if (!pixels) {
        throw std::runtime_error("failed to load texture image!");
    }

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;

    buffer::bindBuffer(
        physicalDevice,
        logicalDevice,
        imageSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer,
        stagingBufferMemory
    );

    void* data;
    vkMapMemory(logicalDevice, stagingBufferMemory, 0, imageSize, 0, &data);
    memcpy(data, pixels, static_cast<size_t>(imageSize));
    vkUnmapMemory(logicalDevice, stagingBufferMemory);

    stbi_image_free(pixels);

    bindImage(
        physicalDevice,
        logicalDevice,
        texWidth,
        texHeight,
        VK_FORMAT_R8G8B8A8_SRGB,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        textureImage,
        textureImageMemory
    );

    transitionImageLayout(
        logicalDevice,
        commandPool,
        graphicsQueue,
        textureImage,
        VK_FORMAT_R8G8B8A8_SRGB,
        // The image was created with the VK_IMAGE_LAYOUT_UNDEFINED layout
        // We can only do that because we don't care of the format
        // before the copy operation
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
    );

    copyBufferToImage(
        logicalDevice,
        commandPool,
        graphicsQueue,
        stagingBuffer,
        textureImage,
        static_cast<uint32_t>(texWidth),
        static_cast<uint32_t>(texHeight)
    );

    // To be able to start sampling from the texture image in the shader, 
    // we need one last transition to prepare it for shader access
    transitionImageLayout(
        logicalDevice,
        commandPool,
        graphicsQueue,
        textureImage,
        VK_FORMAT_R8G8B8A8_SRGB,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    );

    // clean up the stagin buffer
    vkDestroyBuffer(logicalDevice, stagingBuffer, nullptr);
    vkFreeMemory(logicalDevice, stagingBufferMemory, nullptr);

}

void createTextureImageView(VkDevice logicalDevice, VkImage textureImage, VkImageView& textureImageView) {
    textureImageView = image::createImageView(logicalDevice, textureImage, VK_FORMAT_R8G8B8A8_SRGB);
}

void createTextureSample(VkPhysicalDevice physicalDevice, VkDevice logicalDevice, VkSampler& textureSampler) {

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    // could be used for example for floors and walls
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    // no reason to use something else except for performance reason
    // be wary that it is actually an optional device feature
    // so physical device must be checked properly about this
    samplerInfo.anisotropyEnable = VK_TRUE;

    // retriev the maximum quality of the GPU
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(physicalDevice, &properties);

    samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;

    // when accessing beyond the image with clamp to border
    // can be black, white or transparent
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;

    // use normalized coordinate 0,1 0,1
    // instead of 0,texWidth 0,texHeight
    samplerInfo.unnormalizedCoordinates = VK_FALSE;

    // if true used for filter (compare to a value)
    // this could be used for percentage-colser filtering on shadow maps
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

    // mipmaping, we'll see later
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;

    if (vkCreateSampler(logicalDevice, &samplerInfo, nullptr, &textureSampler) != VK_SUCCESS) {
        throw std::runtime_error("failed to create texture sampler!");
    }
}

}
