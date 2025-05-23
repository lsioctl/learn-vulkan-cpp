#pragma once

// Let GLFW include by itslef vulkan headers
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace pipeline {

/** the attachments referenced by the pipeline stages and their usage */
void createRenderPass(
    VkDevice logical_device,
    VkFormat swapChainImageFormat,
    VkRenderPass* pRenderPass
);

void createGraphicsPipeline(
    const char* vert_file,
    const char* frag_file,
    VkDevice logical_device,
    VkExtent2D swapChainExtent,
    VkRenderPass renderPass,
    VkPipelineLayout* pPipelineLayout,
    VkPipeline* pGraphicsPipeline
);

}