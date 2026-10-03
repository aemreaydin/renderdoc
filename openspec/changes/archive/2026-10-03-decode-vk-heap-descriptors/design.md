# Design

## Context

Descriptor buffers already map descriptor bytes back to resources. `vkGetDescriptorEXT` records each unique descriptor as a chunk on the owning resource's record. Replay reruns it, fails if the bytes differ, and inserts the bytes into a trie (`RegisterDescriptor`). `LookupDescriptor` then resolves bytes using the descriptor type from the set layout. It tries format-guessing fast paths that call `vkGetDescriptorEXT`, then falls back to the trie.

Heap descriptors differ in three ways:
- The type is chosen by the shader at access time, so a heap slot must be resolved from its bytes alone.
- `vkGetDescriptorEXT` belongs to `VK_EXT_descriptor_buffer`, which a heap application has not enabled.
- Writes describe images and samplers by `VkImageViewCreateInfo` and `VkSamplerCreateInfo`, so no view or sampler object exists. Buffers, texel buffers and acceleration structures are given by device address.

On the development device (RADV), image descriptors are 32 bytes, buffer and sampler descriptors 16, with 4-byte alignment. Halcyon writes its descriptors once at startup, before any captured frame. It stores image descriptors at a 32-byte stride in one buffer and one sampler in another.

Buffers with device addresses and all images are already force-included in captures under descriptor heaps. Acceleration structures are force-included, and indexed by address at capture, only when descriptor buffers are enabled.

## Goals / Non-Goals

**Goals:**
- Exact byte-for-byte identification of every descriptor the application wrote, including writes made before the captured frame.

**Non-Goals:**
- Recognising descriptors from their bit layout. Only bytes produced by a recorded write are identified.
- Reporting smaller descriptor heap limits or changing what the application sees at capture.

## Decisions

### One chunk per written descriptor, on the owning resource's record
`vkWriteResourceDescriptorsEXT` and `vkWriteSamplerDescriptorsEXT` become serialised functions. Each element of a call is recorded as its own chunk with the bytes the driver produced. Elements of one call can belong to different resources.
- Image descriptors go on the image's record. Buffer and texel buffer descriptors go on the record of the buffer found from the address. Acceleration structure descriptors go on the acceleration structure's record. Sampler and null descriptors go on the device record.
- An element is recorded only if its bytes are new for that record. The set of bytes seen is kept with the record, so it is freed when the resource is destroyed.
- Writes outside the captured frame are recorded too, because chunks on a record are written whenever the resource is in the capture.

This follows `vkGetDescriptorEXT`. Alternative: one device-wide list of writes. Rejected because it grows with every texture ever written and replays writes for destroyed resources.

The deduplication key is the output bytes rather than `DescriptorUniquenessKey`, which has no fields for view parameters.

### Acceleration structures included like under descriptor buffers
Under `DescriptorHeaps()`, every acceleration structure is force-included and indexed by address at capture, as already happens under `DescriptorBuffers()`. Otherwise a top-level acceleration structure reachable only through a heap would be missing from the capture, and its descriptor could not be attributed to a record.

### Replay verifies bytes, then registers them
On replay each chunk reruns the write with unwrapped handles and compares the result with the recorded bytes. A size or byte mismatch fails replay with `APIHardwareUnsupported`, naming the type and printing both byte patterns, as `Serialise_vkGetDescriptorEXT` does. Matching bytes are inserted into the heap lookup table.

### A heap lookup table separate from the descriptor-buffer trie
The table maps descriptor bytes to an entry holding:
- the descriptor type and image layout
- the resource ID, plus offset, size and format for buffers and texel buffers
- a `VulkanCreationInfo::ImageView` or `VulkanCreationInfo::Sampler` built from the recorded create info

Lookups are exact matches on the bytes. If two writes produce identical bytes, the first one recorded wins.

The image-view and sampler parts of `FillDescriptor` and `FillSamplerDescriptor` are split out to take these creation-info structs, so heap entries and descriptor sets share them. A heap image is reported with `resource` set to the image and no `view`, because no view object exists.

Alternative: create a replay-only `VkImageView` or `VkSampler` for each unique write and reuse `DescriptorSetSlot` unchanged. Rejected because the UI would show view and sampler IDs for objects the application never created.

The descriptor-buffer trie is not reused because its values reference view and sampler objects. Its fast paths are not reused because they need a type and `vkGetDescriptorEXT`.

### Heaps registered as descriptor stores at first bind
The buffer usage bit is the same for resource and sampler heaps, so a heap's kind is known only when it is bound. While the capture loads, the first replayed `vkCmdBindResourceHeapEXT` or `vkCmdBindSamplerHeapEXT` on a buffer registers that buffer as a descriptor store:
- the first descriptor sits at the bind address's offset into the buffer
- the slot count covers the bound range
- the slot size is the larger of the aligned image and buffer descriptor sizes for resource heaps, and the aligned sampler descriptor size for sampler heaps

The descriptor heap properties are stored on the device at creation for this.

`GetDescriptors`, `GetSamplerDescriptors` and `GetDescriptorLocations` recognise heap stores. For each slot they read the buffer contents at the current event and look the bytes up:
- resource heaps try the image descriptor size, then the buffer descriptor size
- sampler heaps use the sampler descriptor size
- misses report an unknown type

Locations are named by slot index.

Alternative: probe at every alignment step (4 bytes on RADV). Rejected because each image would also match partial windows and fill the view with unknown rows. Heaps laid out at mixed strides are out of scope.

### Bound heaps in `VKPipe::State`
A new `VKPipe::DescriptorHeap` holds the buffer, byte offset, byte size, reserved range offset and reserved range size. `VKPipe::State` gains `resourceHeap` and `samplerHeap`, because heap binds are not tied to a bind point. They are filled from `VulkanRenderState`, resolving the buffer from the bind address the way descriptor buffers are. The pipeline state viewer lists them next to the descriptor sets, and activating a row opens the descriptor viewer on that store. The new struct is added to replay serialisation and the Python bindings.

### A demo covering every decoded type
`vk_descriptor_heap.cpp` creates named resources of every decoded type and writes their descriptors into a resource heap and a sampler heap before the first frame. It also writes:
- a null descriptor
- a slot filled with bytes no write produced
- a mid-frame copy that replaces one slot between two draws

Its GLSL shader (`GL_EXT_descriptor_heap`, compiled by glslang) reads every slot through push data indices and outputs green only if each read returns the expected values. `VK_Descriptor_Heap.py` checks:
- the pixel colour
- the bound heaps in pipeline state
- that every slot, fetched through `GetDescriptors` and `GetSamplerDescriptors`, names the expected resource and parameters, before and after the mid-frame copy

## Risks / Trade-offs

- [Two writes of different types or views can produce identical bytes, e.g. a sampled and a storage descriptor of the same view] → The first recorded entry is reported. The bytes are equivalent to the hardware, so only the displayed type or view parameters can be wrong.
- [If null descriptor bytes are all zero, zero-filled slots match a recorded null write] → They show as null descriptors rather than unknown. The demo's unknown slot uses non-zero filler.
- [glslang emits different heap SPIR-V from Slang, so the demo may hit SPIR-V processing or reflection errors that Halcyon did not] → Fix only errors the demo shader produces, under the existing heap shader requirement. None appeared.
- [A buffer bound with different ranges or as both heap kinds keeps the registration from its first bind] → Accepted. Neither Halcyon nor the demo rebinds heaps differently.
- [Recording every unique write adds capture overhead for applications that write many descriptors] → The bytes are deduplicated per resource, so steady-state rewrites of the same descriptor cost a lookup only. Each write still reads its bytes back from application memory, which is slow if the application writes straight into mapped device memory.
- [Resource heaps are read at a single fixed slot size, the larger of the image and buffer descriptor sizes (32 bytes on RADV)] → Descriptors packed at other offsets, such as 16-byte buffer descriptors between images, show as unknown.
- [The descriptor viewer shows a heap's whole contents at the selected event, not the slots a draw read] → Per-draw access needs heap variables reflected or shader feedback. The per-stage resource list stays empty for heap shaders.
- [A recorded write whose bytes differ on replay now fails the load] → Heap captures are tied to the capturing device and driver, as image opaque data already required.
- [Captures from before this change have no write chunks] → They still replay, with every heap slot unknown. Captures from this change on use chunk types earlier builds of the fork cannot read.
- [The demo and Halcyon both draw with shader objects] → Pipelines created with `VK_PIPELINE_CREATE_2_DESCRIPTOR_HEAP_BIT_EXT` are untested under capture.
