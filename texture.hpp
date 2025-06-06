#pragma once

// Let GLFW include by itslef vulkan headers
#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"

namespace texture {

void createTextureImage(
    VkPhysicalDevice physicalDevice,
    VkDevice logicalDevice,
    VkCommandPool commandPool,
    VkQueue graphicsQueue,
    VkImage& textureImage,
    VkDeviceMemory& textureImageMemory
);

// images are used through imageView rather than directly
void createTextureImageView(VkDevice logicalDevice, VkImage textureImage, VkImageView& textureImageView);

/**
 * It is possible for shaders to read texels directly from images, but that is not very common when
 * they are used as textures. Textures are usually accessed through samplers, which will apply 
 * filtering and transformations to compute the final color that is retrieved.
 * 
 * * oversampling: texture mapped to a geometry with more fragments than texel
 * => combine the 4 closests texel with linear interpolation (bilinear filtering)
 * 
 * * undersampling: more texels than fragments
 * => anisotropic filtering
 * 
 * * transformations: what happen when we try reading a texel outside of the image throudh addressing mode, e.g:
 *   * Repeat
 *   * Clamp to Edge
 *   * Clamp to Border
 * 
 * Note the sampler does not reference a VkImage anywhere. The sampler is a distinct object that provides an
 * interface to extract colors from a texture. It can be applied to any image you want, whether it is 1D, 2D or 3D.
 * This is different from many older APIs, which combined texture images and filtering into a single state.
 */
void createTextureSample(VkPhysicalDevice physicalDevice, VkDevice logicalDevice, VkSampler& textureSampler);

}