#version 450

/** 
* Note that dvec3, 64 bits use multiple slots
* layout(location = 0) in dvec3 inPosition;
* layout(location = 2) in vec3 inColor;
*/
layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec3 inColor;

layout(location = 0) out vec3 fragColor;

void main() {
    gl_Position = vec4(inPosition, 0.0, 1.0);
    fragColor = inColor;
}