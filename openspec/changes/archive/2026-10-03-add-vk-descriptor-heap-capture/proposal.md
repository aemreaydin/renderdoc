# Proposal

## Why

RenderDoc filters `VK_EXT_descriptor_heap` out of the device extension list, so Halcyon, which binds all resources through descriptor heaps, cannot be captured at all (upstream issue baldurk/renderdoc#3781). This local fork needs capture and replay working now on the development machine (AMD Radeon 890M, Mesa RADV 26.2.4, which reports `descriptorHeapCaptureReplay`).

## What Changes

- Expose `VK_EXT_descriptor_heap` to applications when the device supports `descriptorHeapCaptureReplay`, and keep filtering it out otherwise.
- Expose `VK_KHR_shader_untyped_pointers`, which the heap extension requires and RenderDoc currently filters out, serialising its feature struct at device creation.
- Serialise device creation with the descriptor heap features struct, and force `descriptorHeapCaptureReplay` on.
- Create every image with the descriptor heap capture/replay flag, record its opaque capture data, and recreate it from that data on replay so descriptor bytes written at capture time stay valid.
- Hook the descriptor heap entry points: descriptor writes pass through with handles unwrapped; heap binds and push data are serialised, replayed and tracked in render state.
- Restore bound heaps and push data after RenderDoc's own replay work.
- Accept shaders that access heaps through `SPV_KHR_untyped_pointers` and `SPV_EXT_descriptor_heap` without failing to load; heap variables are not reflected.

Out of scope for this change:
- Showing heap descriptors in pipeline state or as a draw's inputs, and decoding descriptor bytes back to resources.
- Shader debugging, quad overdraw, shader feedback, pixel history and other features that patch heap-using shaders.
- Set/binding mapping (`VkShaderDescriptorSetAndBindingMappingInfoEXT`), embedded samplers, custom border color registration, YCbCr, tensors, device-generated commands, and secondary command buffer heap inheritance.
- Devices or drivers other than RADV.

## Capabilities

### New Capabilities
- `vulkan-descriptor-heap`: capturing and replaying Vulkan applications that use `VK_EXT_descriptor_heap`.

### Modified Capabilities

None.

## Impact

- Vulkan driver: extension filtering and feature checks (`vk_core.cpp`), device creation (`wrappers/vk_device_funcs.cpp`), pNext serialisation (`vk_serialise.cpp`), image creation and opaque data (`wrappers/vk_resource_funcs.cpp`, `vk_common.h`), new command wrappers (`wrappers/vk_cmd_funcs.cpp`), pass-through entry points (`wrappers/vk_get_funcs.cpp`), hooks (`vk_hookset_defs.h`), render state (`vk_state.h`, `vk_state.cpp`), extension list (`extension_support.md`).
- SPIR-V: `spirv_processor.cpp` registers the untyped pointer type so heap shaders disassemble without errors.
- Captures that use descriptor heaps only replay on the same device and driver they were taken on, and cannot be inspected by a later version that decodes descriptors without being retaken.
