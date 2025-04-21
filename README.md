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
sudo apt install vulkan-validationlayers-dev
```

## Note on my Linux (no nvidia driver)

`vkcube` runs on embedded intel GPU
