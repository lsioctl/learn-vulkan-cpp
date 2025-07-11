#pragma once

#include <array>

// Let GLFW include by itslef vulkan headers
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include "glm/glm.hpp"


namespace vertex {

// glm provides us types that exactly match
// the ones in the shader language
// position and color are vertex attributes
struct Vertex {
    glm::vec2 pos;
    glm::vec3 color;

    static VkVertexInputBindingDescription getBindingDescription() {
        VkVertexInputBindingDescription bindingDescription{};

        // index of the binding in the array of binding
        bindingDescription.binding = 0;
        // Note that functions in struct do not take any place in the object memory
        // so no impact of sizeof(Vertex)
        // it is just a shortcut for a free function with object pointer
        // this as parameter
        bindingDescription.stride = sizeof(Vertex);
        // two possibilities
        // VK_VERTEX_INPUT_RATE_VERTEX: Move to the next data entry after each vertex
        // VK_VERTEX_INPUT_RATE_INSTANCE: Move to the next data entry after each instance
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        return bindingDescription;
    }

    static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions() {
        std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions{};

        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        // byte size of the attribute. here for vec2
        attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(Vertex, pos);

        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        // byte size of the attribute. here for vec3
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(Vertex, color);

        return attributeDescriptions;
    }
};

}