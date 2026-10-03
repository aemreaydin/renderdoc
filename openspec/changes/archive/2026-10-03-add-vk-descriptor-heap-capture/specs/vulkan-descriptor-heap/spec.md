# Spec Delta

## Purpose

Lets RenderDoc capture and replay Vulkan applications that bind their resources through `VK_EXT_descriptor_heap`, with replay reproducing the rendering seen at capture time.

## ADDED Requirements

### Requirement: Extension exposed only with capture/replay support
RenderDoc SHALL report `VK_EXT_descriptor_heap` as an available device extension during capture only when the device supports the `descriptorHeapCaptureReplay` feature.

#### Scenario: Device supports descriptor heap capture/replay
- **WHEN** an application enumerates device extensions under capture on a device reporting `descriptorHeapCaptureReplay`
- **THEN** `VK_EXT_descriptor_heap` is listed

#### Scenario: Device lacks descriptor heap capture/replay
- **WHEN** an application enumerates device extensions under capture on a device that supports `VK_EXT_descriptor_heap` but not `descriptorHeapCaptureReplay`
- **THEN** `VK_EXT_descriptor_heap` is not listed
- **AND** RenderDoc logs a warning naming the missing feature

### Requirement: Untyped pointers exposed
RenderDoc SHALL report `VK_KHR_shader_untyped_pointers` as an available device extension during capture when the device supports it, since `VK_EXT_descriptor_heap` depends on it.

#### Scenario: Device supports untyped pointers
- **WHEN** an application enumerates device extensions under capture on a device supporting `VK_KHR_shader_untyped_pointers`
- **THEN** `VK_KHR_shader_untyped_pointers` is listed and can be enabled with its `shaderUntypedPointers` feature

### Requirement: Heap-using applications run unchanged under capture
An application that enables `VK_EXT_descriptor_heap`, writes sampler and image descriptors into heap memory, binds resource and sampler heaps, pushes data and draws with heap-enabled shader objects SHALL produce the same output under capture as without RenderDoc.

#### Scenario: Halcyon runs under capture
- **WHEN** Halcyon is launched through RenderDoc and renders its textured quads
- **THEN** the presented frames match those rendered without RenderDoc
- **AND** RenderDoc introduces no Vulkan validation errors in the application's own commands (errors raised by RenderDoc's in-app overlay are excluded)

### Requirement: Captured frames replay identically
Replaying a capture of a heap-using application SHALL reproduce the captured rendering at every event, with heap descriptors resolving to the same resources and push data holding the same values as at capture time.

#### Scenario: Textured draw replays
- **WHEN** a Halcyon capture is opened and the last draw is selected
- **THEN** the texture viewer shows the render target with the same textured quads as the captured frame

#### Scenario: Moving between events
- **WHEN** the user selects one event and then a later event in the same capture
- **THEN** the later event's render target matches the captured rendering at that event

### Requirement: Heap-only resources are included in captures
Images that are referenced only through descriptors in heap memory SHALL be included in the capture with their contents at the start of the frame.

#### Scenario: Texture reachable only through the heap
- **WHEN** a frame samples a texture whose only reference is a descriptor in the resource heap
- **THEN** the texture appears in the capture's resource list with its contents intact

### Requirement: Heap shaders load without error
Shaders that access descriptor heaps through untyped pointers SHALL load during replay without errors, with heap variables omitted from their reflected resources.

#### Scenario: Viewing a heap-using shader
- **WHEN** the user opens the fragment shader of a heap-using draw in the pipeline state view
- **THEN** the shader loads and shows its push constant block
- **AND** no error is reported for the heap variables, which are not listed as resources

### Requirement: Replay fails clearly when descriptors cannot be reproduced
When the replaying device cannot recreate an image so that its descriptors match the captured ones, replay SHALL stop with an error rather than render with mismatched descriptors.

#### Scenario: Replay on a different driver
- **WHEN** a heap-using capture is opened on a device or driver that rejects the captured image opaque data
- **THEN** loading the capture fails with an image creation error instead of replaying
