# Proposal

## Why

RenderDoc treats `VK_EXT_descriptor_heap` heap memory as opaque bytes and does not record descriptor writes, so a capture cannot show which resources a heap holds. Inspecting Halcyon's textures through its heaps needs each descriptor resolved back to its resource.

## What Changes

- Record each unique result of `vkWriteResourceDescriptorsEXT` and `vkWriteSamplerDescriptorsEXT` in the capture, including writes made before the captured frame.
- On replay, rerun each recorded write and stop with an error if the driver produces different bytes, then add the bytes to a lookup table that resolves descriptor bytes to the resource and view parameters, or sampler parameters, they were written from.
- Show the bound resource and sampler heaps in the Vulkan pipeline state, and let the descriptor viewer list each heap slot with the resource it holds at the current event.
- Decode sampled and storage images, samplers, uniform and storage buffers, uniform and storage texel buffers, acceleration structures, and null descriptors.
- Add a Vulkan demo that writes every decoded descriptor type into heaps and a Python test that checks each heap slot resolves to the right resource.

Out of scope for this change:
- Listing heap descriptors among a draw's per-stage resources, or reporting which heap slots a draw accessed. Heap variables stay unreflected.
- Input attachments, tensors, QCOM image types, YCbCr and custom border color samplers, embedded samplers, and set/binding mapping.
- Resource heaps whose descriptors are not laid out at a single fixed stride.

## Capabilities

### New Capabilities

None.

### Modified Capabilities
- `vulkan-descriptor-heap`: adds identifying the resource behind each heap descriptor and showing bound heaps in pipeline state, and extends replay failure to descriptors whose bytes change on replay.

## Impact

- Vulkan driver: descriptor write wrappers move from `wrappers/vk_get_funcs.cpp` to serialised functions with new chunk types (`vk_common.h`, `vk_core.cpp`, `vk_core.h`, `vk_hookset_defs.h`), a heap descriptor lookup table, descriptor store registration at heap bind (`wrappers/vk_cmd_funcs.cpp`), and heap paths in `GetDescriptors`, `GetSamplerDescriptors`, `GetDescriptorLocations` and pipeline state (`vk_replay.cpp`).
- Replay API: new bound-heap fields in `VKPipe` (`api/replay/vk_pipestate.h`, `replay/renderdoc_serialise.inl`).
- UI: heap rows in `VulkanPipelineStateViewer.cpp` that open the descriptor viewer.
- Tests: new demo in `util/test/demos/vk` (registered in `CMakeLists.txt`, `demos.vcxproj`, `demos.vcxproj.filters`) and a matching test in `util/test/tests/Vulkan`.
- Captures taken before this change still replay, but their heap slots show as unknown until retaken. Captures taken after it contain new chunk types and cannot be opened by earlier builds of this fork.
