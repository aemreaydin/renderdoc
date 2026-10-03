# Design

## Context

RenderDoc already captures `VK_EXT_descriptor_buffer`. It requires the descriptor buffer capture/replay feature, forces buffer device address capture/replay on all memory, and creates every image with opaque capture data, so descriptor bytes written at capture time are bit-identical on replay. Descriptor heaps can rely on the same guarantee through `descriptorHeapCaptureReplay`.

The headers, dispatch table, stringisers and pNext unwrapping already know the descriptor heap types; `vk_serialise.cpp` marks its structs as unsupported, and the extension is missing from the supported list in `vk_core.cpp`.

Halcyon, the only target, uses heaps as follows:
- Shader objects created with `VK_SHADER_CREATE_DESCRIPTOR_HEAP_BIT_EXT` and no layouts.
- Shaders index `ResourceHeapEXT`/`SamplerHeapEXT` built-ins through untyped pointers, with no `DescriptorSet`/`Binding` decorations.
- Sampled-image and sampler descriptors are written to host memory, then copied into mapped heap buffers.
- `vkCmdBindResourceHeapEXT`, `vkCmdBindSamplerHeapEXT` and `vkCmdPushDataEXT` are recorded per frame.

RADV on the development machine reports zero-sized reserved heap ranges, unlimited heap sizes, 256 bytes of push data and 8 bytes of image opaque data.

## Goals / Non-Goals

**Goals:**
- Replay correctness without RenderDoc understanding descriptor contents.

**Non-Goals:**
- Reporting smaller heap or push data limits to applications. Nothing in this change injects RenderDoc descriptors into application heaps.

## Decisions

### Descriptors stay opaque bytes
Heap memory is captured and restored like any other buffer memory. Replay relies on images and memory being recreated so that the captured bytes describe the same resources.

Alternative: decode descriptors at capture and rewrite heaps on replay. Rejected because the application copies descriptor bytes into heap memory itself, so RenderDoc only sees them as memory contents. Decoding is needed only to inspect descriptors, which is out of scope.

### Descriptor writes are not serialised
`vkWriteResourceDescriptorsEXT` unwraps the `VkImage` inside each `VkImageViewCreateInfo` and calls the driver. `vkWriteSamplerDescriptorsEXT` passes through. Neither records a chunk.

Alternative: serialise each unique write, as descriptor buffers do per resource, so a later change can map bytes back to resources. Deferred: it adds capture overhead with no use yet. The trade-off is that captures must be retaken once inspection lands.

### A separate `DescriptorHeaps()` gate
Add a device-level flag, set when the application enables `descriptorHeap`, and gate only these paths with it:
- Memory allocated with device address and capture/replay flags (memory allocation in `vk_resource_funcs.cpp`).
- RenderDoc's whole-memory copy buffer is retried with `VK_BUFFER_USAGE_DESCRIPTOR_HEAP_BIT_EXT` when a plain transfer buffer can't bind to the memory type. On RADV, heap buffers only accept memory types a transfer buffer can't use; without the retry, heap contents were captured as zeros.
- Images created with `VK_IMAGE_CREATE_DESCRIPTOR_HEAP_CAPTURE_REPLAY_BIT_EXT`, with opaque data recorded and replayed.
- All images force-referenced, since heap use cannot be seen from the API.

Alternative: reuse `DescriptorBuffers()`. Rejected: its ~40 call sites also fetch descriptor-buffer opaque data for buffers, views and samplers, enlarge descriptor buffers and shrink their limits. All of these call into an extension the application may not have enabled.

### Image opaque data recorded in the create chunk
At capture, `vkCreateImage` adds the capture/replay flag. It then calls `vkGetImageOpaqueCaptureDataEXT`, which does not require bound memory, and serialises the result as a `VkOpaqueCaptureDataCreateInfoEXT` in the image's create info. Replay creates the image with that struct chained. This mirrors `OpaqueDataForSerialising` (`vk_common.h`). RADV's 8 bytes fit the existing 16-byte fixed capture size, and the extension filter rejects devices whose size is larger.

### Heap and push data in render state
`VulkanRenderState` keeps the last resource and sampler `VkBindHeapInfoEXT`, and stores push data in the existing `pushconsts` array. A flag marks push data as heap-style so rebinding uses `vkCmdPushDataEXT` instead of `vkCmdPushConstants` with a layout. Heaps are rebound right after descriptor buffers in `BindPipeline` and `BindShaderObjects`, before descriptor sets, so sets bound after the heaps still take effect as they did in the application. Binding heaps does not clear previously bound sets from the state; Halcyon never binds sets.

Bind infos contain only device addresses, so they serialise and replay unchanged, given buffer device address capture/replay.

### Untyped pointers exposed alongside heaps
`VK_KHR_shader_untyped_pointers` has no entry points; exposing it only needs its feature struct serialised. Shaders using untyped pointers outside heaps go through the same SPIR-V path described below.

### SPIR-V handled only where loading reports errors
The SPIR-V processor ignores untyped variables and `OpConstantSizeOfEXT`. Unknown decorations are stored generically. Reflection walks only typed globals. So heap variables are invisible and nothing fails. Shader feedback finds no descriptor arrays and returns early.

The only error Halcyon captures produced was the disassembler looking up the untyped pointer type, so `OpTypeUntypedPointerKHR` is registered as a type of unknown kind; the disassembly names it `typeN`.

### Default-initialise `subpassContents`
`VulkanRenderState::subpassContents` had no initialiser, and dynamic rendering never sets it, so it held stack garbage from `VulkanRenderState()` temporaries in `vk_core.cpp`. Adding fields to the struct turned that garbage into `VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS` on this machine. `BeginRenderPassAndApplyState` then skipped binding shaders and state, and pixel history reported zero shader output for dynamic-rendering draws. It now defaults to `VK_SUBPASS_CONTENTS_INLINE`. The bug exists upstream; unmodified RenderDoc avoids it only by chance.

### Unused entry points pass through
`vkRegisterCustomBorderColorEXT`, `vkUnregisterCustomBorderColorEXT` and `vkGetPhysicalDeviceDescriptorSizeEXT` are hooked so that wrapped handles never reach the driver. They unwrap and call through without serialising. Tensor entry points stay unhooked because tensors are not supported.

## Risks / Trade-offs

- [RADV may not recreate images bit-identically across processes, as the extension only promises a best effort] → Replay rendering is compared against the captured frame. A mismatch with no creation error is a driver problem, to be checked with a minimal heap sample before changing the approach.
- [Some RenderDoc replay work binds descriptor sets and may not restore heaps afterwards] → Heaps and push data are restored in `BindPipeline`/`BindShaderObjects`; checked with the clear-before-draw overlay, which re-runs a draw from saved state. Any missed path shows as an untextured draw.
- [RenderDoc's in-app overlay raises validation errors every frame] → Pre-existing and unrelated to heaps. Halcyon aborts on validation errors, so its debug output must stay muted under RenderDoc (the default).
- [Captures depend on the exact device and driver, and the new chunk types make them unreadable by upstream RenderDoc] → Accepted for a local fork.
