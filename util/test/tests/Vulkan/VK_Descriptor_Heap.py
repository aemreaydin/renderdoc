import renderdoc as rd
import rdtest

# VkImageLayout values, reported in byteOffset for images
LAYOUT_GENERAL = 1
LAYOUT_SHADER_READ_ONLY_OPTIMAL = 5


class VK_Descriptor_Heap(rdtest.TestCase):
    demos_test_name = 'VK_Descriptor_Heap'

    def fetch_heap(self, name: str):
        heap = self.get_resource_by_name(name).resourceId
        store = [s for s in self.controller.GetDescriptorStores() if s.resourceId == heap][0]

        r = rd.DescriptorRange()
        r.offset = store.firstDescriptorOffset
        r.descriptorSize = store.descriptorByteSize
        r.count = store.descriptorCount
        r.type = rd.DescriptorType.Unknown

        return (self.controller.GetDescriptors(heap, [r]), self.controller.GetSamplerDescriptors(heap, [r]),
                self.controller.GetDescriptorLocations(heap, [r]))

    def check_resource(self, desc: rd.Descriptor, type: rd.DescriptorType, name: str):
        self.check_eq(desc.type, type)
        self.check_eq(self.get_resource(desc.resource).name, name)

    def check_resource_heap(self, uniform_buffer: str):
        descs, _, locs = self.fetch_heap("ResourceHeap")

        self.check_eq([l.logicalBindName for l in locs[0:3]], ["0", "1", "2"])

        d = descs[0]
        self.check_resource(d, rd.DescriptorType.Image, "SampledImage")
        self.check_eq(d.view, rd.ResourceId())
        self.check_eq(d.format.Name(), "R8G8B8A8_UNORM")
        self.check_eq((d.firstMip, d.numMips, d.firstSlice, d.numSlices), (1, 2, 1, 1))
        self.check_eq((d.swizzle.red, d.swizzle.green, d.swizzle.blue, d.swizzle.alpha),
                      (rd.TextureSwizzle.Blue, rd.TextureSwizzle.Green, rd.TextureSwizzle.Red,
                       rd.TextureSwizzle.Alpha))
        self.check_eq(d.textureType, rd.TextureType.Texture2D)
        self.check_eq(d.byteOffset, LAYOUT_SHADER_READ_ONLY_OPTIMAL)

        d = descs[1]
        self.check_resource(d, rd.DescriptorType.ReadWriteImage, "StorageImage")
        self.check_eq((d.firstMip, d.numMips, d.firstSlice, d.numSlices), (0, 1, 0, 1))
        self.check_eq(d.byteOffset, LAYOUT_GENERAL)

        d = descs[2]
        self.check_resource(d, rd.DescriptorType.ConstantBuffer, uniform_buffer)
        self.check_eq((d.byteOffset, d.byteSize), (0, 16))

        d = descs[3]
        self.check_resource(d, rd.DescriptorType.ReadWriteBuffer, "StorageBuffer")
        self.check_eq((d.byteOffset, d.byteSize), (256, 64))

        d = descs[4]
        self.check_resource(d, rd.DescriptorType.TypedBuffer, "UniformTexels")
        self.check_eq((d.byteOffset, d.byteSize), (256, 64))
        self.check_eq(d.format.Name(), "R32G32B32A32_FLOAT")

        d = descs[5]
        self.check_resource(d, rd.DescriptorType.ReadWriteTypedBuffer, "StorageTexels")
        self.check_eq((d.byteOffset, d.byteSize), (0, 64))
        self.check_eq(d.format.Name(), "R32G32B32A32_FLOAT")

        self.check_resource(descs[6], rd.DescriptorType.AccelerationStructure, "TLAS")

        self.check_eq(descs[7].type, rd.DescriptorType.Image)
        self.check_eq(descs[7].resource, rd.ResourceId())

        self.check_eq(descs[8].type, rd.DescriptorType.Unknown)
        self.check_eq(descs[8].resource, rd.ResourceId())

    def check_sampler_heap(self):
        descs, samps, _ = self.fetch_heap("SamplerHeap")

        self.check_eq([d.type for d in descs[0:2]], [rd.DescriptorType.Sampler] * 2)

        s = samps[0]
        self.check_eq((s.filter.minify, s.filter.magnify, s.filter.mip, s.filter.filter),
                      (rd.FilterMode.Point, rd.FilterMode.Point, rd.FilterMode.Point,
                       rd.FilterFunction.Normal))
        self.check_eq((s.addressU, s.addressV, s.addressW),
                      (rd.AddressMode.Wrap, rd.AddressMode.Wrap, rd.AddressMode.Wrap))

        s = samps[1]
        self.check_eq((s.filter.minify, s.filter.magnify, s.filter.mip, s.filter.filter),
                      (rd.FilterMode.Linear, rd.FilterMode.Linear, rd.FilterMode.Linear,
                       rd.FilterFunction.Comparison))
        self.check_eq((s.addressU, s.addressV, s.addressW),
                      (rd.AddressMode.ClampEdge, rd.AddressMode.Mirror, rd.AddressMode.ClampBorder))
        self.check_eq(s.compareFunction, rd.CompareFunction.Less)
        self.check_eq((s.minLOD, s.maxLOD), (1.0, 4.0))

    def check_bound_heap(self, heap: rd.VKDescriptorHeap, name: str):
        self.check_eq(self.get_resource(heap.buffer).name, name)

        store = [s for s in self.controller.GetDescriptorStores() if s.resourceId == heap.buffer][0]
        self.check_eq(heap.byteOffset, store.firstDescriptorOffset)
        self.check_eq(heap.byteSize // store.descriptorByteSize, store.descriptorCount)
        # the demo places the reserved range at the end of the heap
        self.check_eq(heap.reservedRangeOffset + heap.reservedRangeSize, heap.byteSize)

    def check_capture(self):
        self.set_event(self.find_action("vkCmdUpdateBuffer").eventId, False)
        vkpipe = self.controller.GetVulkanPipelineState()
        self.check_eq(vkpipe.resourceHeap.buffer, rd.ResourceId())
        self.check_eq(vkpipe.samplerHeap.buffer, rd.ResourceId())

        rdtest.log.success("No heaps are reported before the first heap bind")

        for name, x, uniform_buffer in [("First Draw", 0.25, "UniformBuffer"),
                                        ("Second Draw", 0.75, "UniformBuffer2")]:
            action = self.find_action(name).nextAction
            self.set_event(action.eventId, False)

            pipe = self.controller.GetPipelineState()
            self.check_pixel_value(pipe.GetOutputTargets()[0].resource, x, 0.5, [0.0, 1.0, 0.0, 1.0])

            rdtest.log.success(f"{name} reads every heap descriptor correctly")

            vkpipe = self.controller.GetVulkanPipelineState()
            self.check_bound_heap(vkpipe.resourceHeap, "ResourceHeap")
            self.check_bound_heap(vkpipe.samplerHeap, "SamplerHeap")

            rdtest.log.success(f"Bound heaps at {name} are as expected")

            self.check_resource_heap(uniform_buffer)
            self.check_sampler_heap()

            rdtest.log.success(f"Heap contents at {name} are as expected")
