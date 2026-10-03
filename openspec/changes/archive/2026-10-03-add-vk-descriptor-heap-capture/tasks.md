# Tasks

## 1. Baseline

- [x] 1.1 Build RenderDoc (`cmake -DCMAKE_BUILD_TYPE=Debug -Bbuild -H. && make -C build`) and reproduce the failure: `build/bin/renderdoccmd capture ~/Work/halcyon/Build/Dev/Bin/HalcyonTestbed TexturedQuads` (with `SDL_VIDEO_DRIVER=x11`, and `VK_ADD_IMPLICIT_LAYER_PATH` pointing at a layer manifest for `build/lib`) fails because `VK_EXT_descriptor_heap` and `VK_KHR_shader_untyped_pointers` are not offered; keep the log for comparison

## 2. Expose the extension

- [x] 2.1 Add `VK_EXT_descriptor_heap` to the supported extension list in `vk_core.cpp` and filter it out unless `descriptorHeapCaptureReplay` is supported and `imageCaptureReplayOpaqueDataSize` fits `FixedOpaqueDescriptorCaptureSize`, logging a warning otherwise; verify Halcyon's device selection accepts the RADV device under `renderdoccmd capture` (the warning branch is checked by reading, as no local device lacks the feature)
- [x] 2.2 Move `VK_EXT_descriptor_heap` into the supported section of `renderdoc/driver/vulkan/extension_support.md`; verify it no longer appears in the unsupported list
- [x] 2.3 Add `VK_KHR_shader_untyped_pointers` to the supported extension list, serialise `VkPhysicalDeviceShaderUntypedPointersFeaturesKHR` in place of its `PNEXT_UNSUPPORTED` entry, and move it to the supported section of `extension_support.md`; verify Halcyon no longer reports it missing under capture

## 3. Device creation

- [x] 3.1 Serialise `VkPhysicalDeviceDescriptorHeapFeaturesEXT` and `VkOpaqueCaptureDataCreateInfoEXT` in `vk_serialise.cpp` in place of their `PNEXT_UNSUPPORTED` entries; verify the capture log has no unsupported-pNext error for Halcyon's `vkCreateDevice`
- [x] 3.2 Add the `DescriptorHeaps()` device flag, force `descriptorHeapCaptureReplay` on when `descriptorHeap` is enabled, and fail replay with `APIHardwareUnsupported` when the replay device lacks it; verify a Halcyon capture opens past device creation in `qrenderdoc`

## 4. Memory and images

- [x] 4.1 Extend the buffer device address capture/replay paths and the whole-memory buffer retry gated on `DescriptorBuffers()` in memory allocation in `wrappers/vk_resource_funcs.cpp` to also apply under `DescriptorHeaps()`; verify heap buffer addresses in replayed `vkCmdBind*HeapEXT` chunks match their buffers' replayed addresses
- [x] 4.2 In `vkCreateImage`, under `DescriptorHeaps()`, add `VK_IMAGE_CREATE_DESCRIPTOR_HEAP_CAPTURE_REPLAY_BIT_EXT`, record `vkGetImageOpaqueCaptureDataEXT` output as a chained `VkOpaqueCaptureDataCreateInfoEXT`, force-reference the image, and recreate it with that data on replay; verify Halcyon's textures appear in the capture's resource list with correct contents and replay creates them without error

## 5. Entry points

- [x] 5.1 Hook `vkWriteResourceDescriptorsEXT` (unwrapping the `VkImage` in each `pView`), `vkWriteSamplerDescriptorsEXT`, `vkGetImageOpaqueCaptureDataEXT`, `vkRegisterCustomBorderColorEXT`, `vkUnregisterCustomBorderColorEXT` and `vkGetPhysicalDeviceDescriptorSizeEXT` as unserialised pass-throughs; verify Halcyon under capture presents the same frames as without RenderDoc, with no new validation layer errors
- [x] 5.2 Serialise and replay `vkCmdBindResourceHeapEXT`, `vkCmdBindSamplerHeapEXT` and `vkCmdPushDataEXT`, recording them into `VulkanRenderState`; verify the commands appear with their parameters in the API inspector for a Halcyon draw

## 6. State restore

- [x] 6.1 Rebind heaps and push data in `VulkanRenderState::BindPipeline` and `BindShaderObjects` when heaps are bound, re-emitting push data with `vkCmdPushDataEXT`; verify that selecting an early event and then the last draw in a Halcyon capture shows the textured quads
- [x] 6.2 Default-initialise `VulkanRenderState::subpassContents`; verify pixel history shader output for dynamic-rendering draws

## 7. Heap shaders

- [x] 7.1 Open a heap-using draw's fragment shader from the pipeline state view and check the replay log for SPIR-V processing or reflection errors; fix only errors that appear; verify the shader loads with its push constant block and no heap resources listed

## 8. Integration

- [x] 8.1 Take a fresh Halcyon capture and walk every scenario in `specs/vulkan-descriptor-heap/spec.md`; verify each passes, noting the replay-failure scenario as checked by reading the existing image creation error path
