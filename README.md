# Taikon

**Taikon** is a GPU emulator for the PS4, Which is in a PS4 emulator(not yet).<br>
You can also use Taikon in your own project.

## Features

### PM4

* [x] Packet type decoding
* [x] Type 3 opcode decoding
* [x] Payload extraction
* [x] Basic opcode coverage
* [ ] Complete game-required PM4 behavior
* [ ] Indirect command buffers
* [ ] Complete synchronization handling

### Graphics State

* [x] Basic graphics state
* [x] Context register processing
* [x] Primitive state tracking
* [x] Index count tracking
* [x] Instance count tracking
* [ ] Complete rasterization state
* [ ] Complete depth/stencil stat
* [ ] Blend state
* [ ] Viewport/scissor state

### Rendering

* [x] Vulkan backbuffer
* [x] Vulkan depth buffer
* [x] Vulkan render pass
* [x] Vulkan framebuffer
* [x] Vulkan command buffer
* [x] Swapchain
* [ ] PS4 render-target translation
* [ ] Texture translation
* [ ] Sampler translation
* [ ] Resource descriptors

### Shader Recompiler

* [ ] PS4 shader decoding
* [ ] Shader intermediate representation
* [ ] GCN → SPIR-V translation
* [ ] Vertex shaders
* [ ] Pixel shaders
* [ ] Compute shaders
* [ ] Geometry/tessellation support

### Compatibility

* [ ] PM4 capture/replay
* [ ] Homebrew rendering test
* [ ] Real-game command streams
* [ ] GPU synchronization validation
* [ ] Render correctness testing
* [ ] Game compatibility

## Current version

This version of Taikon is in v0.0.1-pre-alpha

AS IS UNTIL THE DAY YOU ARE SEEING THIS THIS IS THE **FIRST VERSION** IN THE FUTURE WE WILL ADD MORE VERSIONS

## PS4 Emulator progress

I(MehrsamMod) haven't made the emulator yet.<br>
I'm working on **Taikon** and affter i finish Taikon i will start makeing the Emulator.

## Questions?

**Can i use Taikon in my PS4 emulator right now?** No not yet, i'm still developing Taikon. I will release the v0.0.1 version affter im done.<br>
**What if i'm a gamer?** As i said i will finish Taikon and make the emulator. If you want to play PS4 games Check out <a href="https://github.com/shadps4-emu/shadps4">ShadPS4</a>.<br>
**How big is Taikon?** Well its ~22MB for now. It will get **Bigger** and **Smaller**.<br>
