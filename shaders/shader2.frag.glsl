#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

void main() {
    // print debug alike it allows us to visualize data
    // here texture coordinates
    // green channel: horizontal coordinates
    // red channel: vertical coordinates
    outColor = vec4(fragTexCoord, 0.0, 1.0);
}