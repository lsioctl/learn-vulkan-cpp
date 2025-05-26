#include <stdexcept>

// this has to be in one (and only ?) one file in the project
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "texture.hpp"
#include "buffer.hpp"

namespace texture {

void createTextureImage(
    VkPhysicalDevice physicalDevice,
    VkDevice logicalDevice,
    VkImage textureImage,
    VkDeviceMemory textureImageMemory
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
    imageInfo.extent.width = static_cast<uint32_t>(texWidth);
    imageInfo.extent.height = static_cast<uint32_t>(texHeight);
    imageInfo.extent.depth = 1;
    // no mipmapping for now
    imageInfo.mipLevels = 1;
    // not an array
    imageInfo.arrayLayers = 1;
    // we should use the same format for the texels as the pixels in the buffer, 
    // otherwise the copy operation will fail.
    imageInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
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
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
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
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    // only one queue (graphics)
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    // for multisampling
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.flags = 0; // Optional

    if (vkCreateImage(logicalDevice, &imageInfo, nullptr, &textureImage) != VK_SUCCESS) {
        throw std::runtime_error("failed to create image!");
    }

    // Allocating memory for an image works the same way as allocating memory for a buffer
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(logicalDevice, textureImage, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = buffer::findMemoryType(
        physicalDevice,
        memRequirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );

    if (vkAllocateMemory(logicalDevice, &allocInfo, nullptr, &textureImageMemory) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate image memory!");
    }

    vkBindImageMemory(logicalDevice, textureImage, textureImageMemory, 0);

}


}