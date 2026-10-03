# Spec Delta

## ADDED Requirements

### Requirement: Heap descriptors identify their resources
Each descriptor in a bound heap SHALL be reported, through the replay API and the descriptor viewer, as the resource it was written from, as of the selected event. Slots are a fixed size: the larger of the device's image and buffer descriptor sizes for resource heaps, and the sampler descriptor size for sampler heaps.

#### Scenario: Halcyon textures and sampler
- **WHEN** the user opens the bound resource heap of a Halcyon textured draw in the descriptor viewer
- **THEN** each slot Halcyon wrote lists its texture as a sampled image with the view's format, mip range and layout
- **AND** the sampler heap's first slot lists a sampler with nearest filtering and repeat addressing

#### Scenario: Every decoded descriptor type
- **WHEN** a capture of the heap demo is opened and the heap slots are fetched through the replay API
- **THEN** sampled and storage image slots name their image and view parameters
- **AND** uniform and storage buffer slots name their buffer with the written offset and size
- **AND** uniform and storage texel buffer slots name their buffer with the written offset, size and format
- **AND** the acceleration structure slot names its acceleration structure
- **AND** sampler slots report the written filter, addressing, LOD and compare state

#### Scenario: Null descriptors
- **WHEN** a heap slot holds a null descriptor written by the application
- **THEN** the slot reports its descriptor type with no resource

#### Scenario: Unrecognised bytes
- **WHEN** a heap slot holds bytes that no recorded write produced
- **THEN** the slot is reported as unknown with no resource, and no error is logged

#### Scenario: Descriptors written before the frame
- **WHEN** an application writes descriptors before the captured frame and copies them into heap memory
- **THEN** those descriptors are identified in the capture

#### Scenario: Heap contents change during the frame
- **WHEN** the application copies a different descriptor into a heap slot partway through the frame
- **THEN** selecting an event before the copy shows the old resource in that slot, and an event after it shows the new one

### Requirement: Bound heaps shown in pipeline state
The Vulkan pipeline state SHALL report, at each event, the buffer, byte range and reserved range of the bound resource heap and sampler heap. The pipeline state viewer SHALL list them and open the selected heap's contents in the descriptor viewer.

#### Scenario: Draw with heaps bound
- **WHEN** the user selects a Halcyon draw and views the pipeline state
- **THEN** the resource heap and sampler heap are listed with their buffers, ranges and reserved ranges
- **AND** activating either heap opens the descriptor viewer showing that heap's slots

#### Scenario: No heaps bound
- **WHEN** the user selects an event in a command buffer that has bound no heaps
- **THEN** no heaps are listed

## MODIFIED Requirements

### Requirement: Heap shaders load without error
Shaders that access descriptor heaps through untyped pointers SHALL load during replay without errors, with heap variables omitted from their reflected resources.

#### Scenario: Viewing a heap-using shader
- **WHEN** the user opens the fragment shader of a heap-using draw in the pipeline state view
- **THEN** the shader loads and shows its push constant block
- **AND** no error is reported for the heap variables, which are not listed as resources

#### Scenario: Shader reading every decoded descriptor type
- **WHEN** the heap demo's shader, which reads images, samplers, buffers, texel buffers and an acceleration structure from the heaps, is loaded during replay
- **THEN** it loads with no SPIR-V processing or reflection errors in the replay log

### Requirement: Replay fails clearly when descriptors cannot be reproduced
When the replaying device cannot recreate an image so that its descriptors match the captured ones, or produces different bytes for a recorded descriptor write, replay SHALL stop with an error rather than render with mismatched descriptors.

#### Scenario: Replay on a different driver
- **WHEN** a heap-using capture is opened on a device or driver that rejects the captured image opaque data
- **THEN** loading the capture fails with an image creation error instead of replaying

#### Scenario: Descriptor bytes differ on replay
- **WHEN** a recorded descriptor write produces different bytes on the replaying device than it did at capture
- **THEN** loading the capture fails with an error naming the descriptor type and showing the captured and replayed bytes
