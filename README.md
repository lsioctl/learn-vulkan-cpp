# leanr Vulkan cpp

After playing a bit with:

* OpenGL, C++
* Vulkan, Rust
* WebGPU, Rust

Time to try Vulkan with C++


## Reference doc

https://vulkan-tutorial.com


## Installed

```bash
sudo apt install vulkan-tools
sudo apt install libvulkan-dev
# sudo apt install vulkan-validationlayers-dev
sudo apt install vulkan-utility-libraries-dev 
sudo apt install spirv-tools 
sudo apt install libglfw3-dev
sudo apt install libglm-dev
sudo apt install libxxf86vm-dev libxi-dev
```

Note: missing validation layers (commented out because I had issues with apt) so:

```bash
sudo apt-get install vulkan-validationlayers
```

## glslc

Download from github README

Update the `env` copied from `env.example` in shaders folder

## shader compilation

go to the shaders folder, with glslc installed and configured anr run:

```bash
./compile.sh
```

## Coding style

At first wanted to use google's style, but to mostly stick with the tutorial (and glfw, vk style):

* snakecase for vars
* `_` suffix for private var
* snakecase for functions

## Concepts learned

### Image layout transition

either explicitely with pipeline memory barriers or implicitely during render passes

## Real life recommandations

### Staging buffer

> It should be noted that in a real world application, you're not supposed to actually call vkAllocateMemory for every individual buffer. The maximum number of simultaneous memory allocations is limited by the maxMemoryAllocationCount physical device limit, which may be as low as 4096 even on high end hardware like an NVIDIA GTX 1080. The right way to allocate memory for a large number of objects at the same time is to create a custom allocator that splits up a single allocation among many different objects by using the offset parameters that we've seen in many functions.

> You can either implement such an allocator yourself, or use the VulkanMemoryAllocator library provided by the GPUOpen initiative. However, for this tutorial it's okay to use a separate allocation for every resource, because we won't come close to hitting any of these limits for now.


### Index buffer

> The previous chapter already mentioned that you should allocate multiple resources like buffers from a single memory allocation, but in fact you should go a step further. Driver developers recommend that you also store multiple buffers, like the vertex and index buffer, into a single VkBuffer and use offsets in commands like vkCmdBindVertexBuffers. The advantage is that your data is more cache friendly in that case, because it's closer together. It is even possible to reuse the same chunk of memory for multiple resources if they are not used during the same render operations, provided that their data is refreshed, of course. This is known as aliasing and some Vulkan functions have explicit flags to specify that you want to do this.


### Uniforms

In vertex shader:

> Unlike the 2D triangles, the last component of the clip coordinates may not be 1, which will result in a division when converted to the final normalized device coordinates on the screen. This is used in perspective projection as the perspective division and is essential for making closer objects look larger than objects that are further away.

> Using a UBO this way is not the most efficient way to pass frequently changing values to the shader. A more efficient way to pass a small buffer of data to shaders are push constants. We may look at these in a future chapter.

> As some of the structures and function calls hinted at, it is actually possible to bind multiple descriptor sets simultaneously. You need to specify a descriptor layout for each descriptor set when creating the pipeline layout. Shaders can then reference specific descriptor sets like this:

```glsl
layout(set = 0, binding = 0) uniform UniformBufferObject { ... }
```

### mipmaps

> It should be noted that it is uncommon in practice to generate the mipmap levels at runtime anyway. Usually they are pregenerated and stored in the texture file alongside the base level to improve loading speed Implementing resizing in software and loading multiple levels from a file is left as an exercise to the reader.

### RAII and more

Another doc from Vulkan, with:


https://docs.vulkan.org/tutorial/latest/00_Introduction.html


> Compared to the original tutorial, this version of the tutorial is teaching up-to-date concepts:
>
>  Vulkan 1.4 as a baseline
>
>  Dynamic rendering instead of render passes
>
>   Timeline semaphores
>
>   Slang as the primary shading language
>
>   Modern C++ (20) with modules
>
>   Vulkan-Hpp with RAII



### Smart pointers and C API

```c++
// From SO https://stackoverflow.com/questions/35793672/use-unique-ptr-with-glfwwindow
// because the error was not easy to read
struct DestroyglfwWin{
    void operator()(GLFWwindow* ptr){
         glfwDestroyWindow(ptr);
    }
};

// class member declaration, can't assign value at this point
std::unique_ptr<GLFWwindow, DestroyglfwWin> window_;

// somehere in a init window function
window_ = std::unique_ptr<GLFWwindow, DestroyglfwWin>(glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr));
```