# Tasks

## 1. Heap demo and baseline replay

- [x] 1.1 Write `util/test/demos/vk/vk_descriptor_heap.cpp` as described in design.md and register it in `util/test/demos/CMakeLists.txt`, `demos.vcxproj` and `demos.vcxproj.filters`. It needs named resources of every decoded type, descriptors written before the first frame, a null descriptor, a non-zero unknown slot, and a mid-frame slot copy between two draws. Verify it builds (`cmake -S util/test/demos -B build-demos && make -C build-demos`) and renders green for both draws without RenderDoc.
- [x] 1.2 Force-include and index by address every acceleration structure under `DescriptorHeaps()`. Verify a demo capture replays both draws green in `qrenderdoc`; without this the heap-only top-level acceleration structure is missing.
- [x] 1.3 Open the demo's fragment shader in the pipeline state view and check the replay log for SPIR-V processing or reflection errors. Fix only errors that appear, and verify the shader loads with its push constant block.
- [x] 1.4 Add `util/test/tests/Vulkan/VK_Descriptor_Heap.py`, checking the centre pixel of both draws is green. Verify `python3 util/test/run_tests.py -r build/lib -p build/lib --demos-binary build-demos/demos -t VK_Descriptor_Heap` passes.

## 2. Record and verify descriptor writes

- [x] 2.1 Serialise every type `vkWriteResourceDescriptorsEXT` decodes: `VkResourceDescriptorInfoEXT` and the structs its union points to, with images and addresses as resources. Replace any `PNEXT_UNSUPPORTED` entries they hit in `vk_serialise.cpp`. Verify a demo capture has no serialisation errors in the capture log.
- [x] 2.2 Turn `vkWriteResourceDescriptorsEXT` and `vkWriteSamplerDescriptorsEXT` into serialised functions with new chunk types. Record each element whose bytes are new for its owning record, as chunks on that record (see design.md). Verify the demo capture's chunk list in the API inspector has one write per unique descriptor, including those written before the frame.
- [x] 2.3 On replay, rerun each recorded write and fail with `APIHardwareUnsupported` on a size or byte mismatch, printing both byte patterns. Verify both captures load without errors. Verify the failure path by temporarily flipping a byte of the recorded descriptor in the replay handler, then remove that edit.

## 3. Heap lookup and descriptor stores

- [x] 3.1 Store the descriptor heap properties at device creation. Build the heap lookup table from verified replay writes, holding image view and sampler creation info. Split the image-view and sampler parts of `FillDescriptor` and `FillSamplerDescriptor` so both paths share them. Verify existing descriptor-set captures report unchanged descriptors via `VK_Descriptor_Indexing` and `VK_Descriptor_Buffer` in `run_tests.py`.
- [x] 3.2 Register a buffer as a descriptor store at its first replayed heap bind, using the slot sizes in design.md. Resolve heap stores in `GetDescriptors`, `GetSamplerDescriptors` and `GetDescriptorLocations`. Extend `VK_Descriptor_Heap.py` to fetch every slot of both heaps before and after the mid-frame copy, and check each named resource, view parameters, buffer range and format, sampler parameters, null slot and unknown slot. Verify it passes.

## 4. Pipeline state and UI

- [x] 4.1 Add `VKPipe::DescriptorHeap` and `resourceHeap`/`samplerHeap` to `VKPipe::State`, with docs, replay serialisation in `renderdoc_serialise.inl`, and Python bindings. Fill them from `VulkanRenderState`. Extend `VK_Descriptor_Heap.py` to check both heaps' buffers, ranges and reserved ranges, and that an event before the first heap bind reports none. Verify it passes.
- [x] 4.2 List the bound heaps in `VulkanPipelineStateViewer.cpp` next to the descriptor sets, opening the descriptor viewer on activation. Verify in `qrenderdoc` that a demo draw shows both heaps, and that each opens with the decoded slots.

## 5. Integration

- [x] 5.1 Take a fresh Halcyon capture and walk every scenario in `specs/vulkan-descriptor-heap/spec.md` and the existing main spec. Verify each passes, noting the replay-mismatch scenario as checked by 2.3, and run `VK_Descriptor_Heap`, `VK_Descriptor_Buffer` and `VK_Descriptor_Indexing` together in `run_tests.py`.
