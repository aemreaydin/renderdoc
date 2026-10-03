/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2026 Baldur Karlsson
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

#include "vk_test.h"

RD_TEST(VK_Descriptor_Heap, VulkanGraphicsTest)
{
  static constexpr const char *Description =
      "Writes every descriptor type into EXT_descriptor_heap heaps and reads them all from "
      "heap-enabled shader objects, replacing one descriptor between two draws.";

  std::string vertex = R"EOSHADER(
#version 460 core

void main()
{
  vec2 pos = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
  gl_Position = vec4(pos * 2.0f - 1.0f, 0.0f, 1.0f);
}

)EOSHADER";

  std::string pixel = R"EOSHADER(
#version 460 core

#extension GL_EXT_descriptor_heap : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_ray_query : require
#extension GL_EXT_samplerless_texture_functions : require

layout(descriptor_heap) uniform texture2D sampledImages[];
layout(descriptor_heap, rgba8) readonly uniform image2D storageImages[];
layout(descriptor_heap) uniform ubo_block { vec4 data; } ubos[];
layout(descriptor_heap) readonly buffer ssbo_block { vec4 data; } ssbos[];
layout(descriptor_heap) uniform textureBuffer uniformTexels[];
layout(descriptor_heap, rgba32f) readonly uniform imageBuffer storageTexels[];
layout(descriptor_heap) uniform accelerationStructureEXT accelStructs[];
layout(descriptor_heap) uniform sampler samplers[];

layout(push_constant) uniform PushData
{
  uint sampledImage;
  uint storageImage;
  uint ubo;
  uint ssbo;
  uint uniformTexel;
  uint storageTexel;
  uint accelStruct;
  uint samp;
  vec4 expectedUBO;
} push;

layout(location = 0, index = 0) out vec4 Color;

bool near(vec4 a, vec4 b)
{
  return all(lessThan(abs(a - b), vec4(0.01f)));
}

void main()
{
  bool ok = true;

  ok = ok && near(texture(sampler2D(sampledImages[push.sampledImage], samplers[push.samp]),
                          vec2(0.5f)), vec4(0.25f, 0.5f, 0.75f, 1.0f));
  ok = ok && near(imageLoad(storageImages[push.storageImage], ivec2(1, 1)),
                  vec4(0.0f, 1.0f, 0.0f, 1.0f));
  ok = ok && near(ubos[push.ubo].data, push.expectedUBO);
  ok = ok && near(ssbos[push.ssbo].data, vec4(9.0f, 10.0f, 11.0f, 12.0f));
  ok = ok && near(texelFetch(uniformTexels[push.uniformTexel], 1), vec4(13.0f, 14.0f, 15.0f, 16.0f));
  ok = ok && near(imageLoad(storageTexels[push.storageTexel], 1), vec4(17.0f, 18.0f, 19.0f, 20.0f));

  rayQueryEXT query;
  rayQueryInitializeEXT(query, accelStructs[push.accelStruct], gl_RayFlagsTerminateOnFirstHitEXT,
                        0xFF, vec3(0.0f, 0.0f, 5.0f), 0.01f, vec3(0.0f, 0.0f, -1.0f), 100.0f);
  rayQueryProceedEXT(query);
  ok = ok && rayQueryGetIntersectionTypeEXT(query, true) != gl_RayQueryCommittedIntersectionNoneEXT;

  Color = ok ? vec4(0.0f, 1.0f, 0.0f, 1.0f) : vec4(1.0f, 0.0f, 0.0f, 1.0f);
}

)EOSHADER";

  struct PushData
  {
    uint32_t sampledImage, storageImage, ubo, ssbo, uniformTexel, storageTexel, accelStruct, samp;
    Vec4f expectedUBO;
  };

  // resource heap slots, each resourceStride bytes apart
  enum
  {
    SampledImageSlot,
    StorageImageSlot,
    UniformBufferSlot,
    StorageBufferSlot,
    UniformTexelSlot,
    StorageTexelSlot,
    AccelStructSlot,
    NullSlot,
    UnknownSlot,
    ResourceSlotCount,
  };

  VkPhysicalDeviceDescriptorHeapPropertiesEXT heapProps = {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_HEAP_PROPERTIES_EXT,
  };

  void Prepare(int argc, char **argv)
  {
    devExts.push_back(VK_EXT_DESCRIPTOR_HEAP_EXTENSION_NAME);
    devExts.push_back(VK_KHR_SHADER_UNTYPED_POINTERS_EXTENSION_NAME);
    devExts.push_back(VK_EXT_SHADER_OBJECT_EXTENSION_NAME);
    devExts.push_back(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);
    devExts.push_back(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME);
    devExts.push_back(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
    devExts.push_back(VK_EXT_ROBUSTNESS_2_EXTENSION_NAME);
    devExts.push_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
    devExts.push_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
    devExts.push_back(VK_KHR_RAY_QUERY_EXTENSION_NAME);
    devExts.push_back(VK_KHR_SPIRV_1_4_EXTENSION_NAME);
    devExts.push_back(VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME);

    VulkanGraphicsTest::Prepare(argc, argv);

    if(!Avail.empty())
      return;

    static VkPhysicalDeviceDescriptorHeapFeaturesEXT heapFeats = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_HEAP_FEATURES_EXT,
    };
    getPhysFeatures2(&heapFeats);
    if(!heapFeats.descriptorHeap)
      Avail = "feature 'descriptorHeap' not available";
    heapFeats = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_HEAP_FEATURES_EXT};
    heapFeats.descriptorHeap = VK_TRUE;
    heapFeats.pNext = (void *)devInfoNext;
    devInfoNext = &heapFeats;

    static VkPhysicalDeviceShaderUntypedPointersFeaturesKHR untypedFeats = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_UNTYPED_POINTERS_FEATURES_KHR,
    };
    getPhysFeatures2(&untypedFeats);
    if(!untypedFeats.shaderUntypedPointers)
      Avail = "feature 'shaderUntypedPointers' not available";
    untypedFeats.pNext = (void *)devInfoNext;
    devInfoNext = &untypedFeats;

    static VkPhysicalDeviceShaderObjectFeaturesEXT shaderObjFeats = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_OBJECT_FEATURES_EXT,
    };
    getPhysFeatures2(&shaderObjFeats);
    if(!shaderObjFeats.shaderObject)
      Avail = "feature 'shaderObject' not available";
    shaderObjFeats.pNext = (void *)devInfoNext;
    devInfoNext = &shaderObjFeats;

    static VkPhysicalDeviceDynamicRenderingFeaturesKHR dynRenderFeats = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR,
    };
    getPhysFeatures2(&dynRenderFeats);
    if(!dynRenderFeats.dynamicRendering)
      Avail = "feature 'dynamicRendering' not available";
    dynRenderFeats.pNext = (void *)devInfoNext;
    devInfoNext = &dynRenderFeats;

    static VkPhysicalDeviceSynchronization2FeaturesKHR sync2Feats = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES_KHR,
    };
    getPhysFeatures2(&sync2Feats);
    if(!sync2Feats.synchronization2)
      Avail = "feature 'synchronization2' not available";
    sync2Feats.pNext = (void *)devInfoNext;
    devInfoNext = &sync2Feats;

    static VkPhysicalDeviceBufferDeviceAddressFeaturesKHR bdaFeats = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES_KHR,
    };
    getPhysFeatures2(&bdaFeats);
    if(!bdaFeats.bufferDeviceAddress)
      Avail = "feature 'bufferDeviceAddress' not available";
    bdaFeats = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES_KHR};
    bdaFeats.bufferDeviceAddress = VK_TRUE;
    bdaFeats.pNext = (void *)devInfoNext;
    devInfoNext = &bdaFeats;

    static VkPhysicalDeviceRobustness2FeaturesEXT robustFeats = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT,
    };
    getPhysFeatures2(&robustFeats);
    if(!robustFeats.nullDescriptor)
      Avail = "feature 'nullDescriptor' not available";
    robustFeats = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT};
    robustFeats.nullDescriptor = VK_TRUE;
    robustFeats.pNext = (void *)devInfoNext;
    devInfoNext = &robustFeats;

    static VkPhysicalDeviceAccelerationStructureFeaturesKHR accelFeats = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
    };
    getPhysFeatures2(&accelFeats);
    if(!accelFeats.accelerationStructure)
      Avail = "feature 'accelerationStructure' not available";
    accelFeats = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR};
    accelFeats.accelerationStructure = VK_TRUE;
    accelFeats.pNext = (void *)devInfoNext;
    devInfoNext = &accelFeats;

    static VkPhysicalDeviceRayQueryFeaturesKHR rqFeats = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR,
    };
    getPhysFeatures2(&rqFeats);
    if(!rqFeats.rayQuery)
      Avail = "feature 'rayQuery' not available";
    rqFeats.pNext = (void *)devInfoNext;
    devInfoNext = &rqFeats;

    static VkPhysicalDeviceDescriptorIndexingFeaturesEXT indexingFeats = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT,
    };
    getPhysFeatures2(&indexingFeats);
    if(!indexingFeats.shaderUniformTexelBufferArrayDynamicIndexing ||
       !indexingFeats.shaderStorageTexelBufferArrayDynamicIndexing)
      Avail = "texel buffer array dynamic indexing not available";
    indexingFeats = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT};
    indexingFeats.shaderUniformTexelBufferArrayDynamicIndexing = VK_TRUE;
    indexingFeats.shaderStorageTexelBufferArrayDynamicIndexing = VK_TRUE;
    indexingFeats.pNext = (void *)devInfoNext;
    devInfoNext = &indexingFeats;

    getPhysProperties2(&heapProps);
  }

  static VkDeviceSize AlignUp(VkDeviceSize x, VkDeviceSize a)
  {
    return (x + a - 1) / a * a;
  }

  VkShaderEXT CreateShader(const std::string &source, ShaderStage stage)
  {
    std::vector<uint32_t> spirv = ::CompileShaderToSpv(demoName, source, SPIRVTarget::vulkan12,
                                                       ShaderLang::glsl, stage, "main");

    VkShaderCreateInfoEXT info = {VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT};
    info.flags = VK_SHADER_CREATE_DESCRIPTOR_HEAP_BIT_EXT;
    info.stage =
        stage == ShaderStage::vert ? VK_SHADER_STAGE_VERTEX_BIT : VK_SHADER_STAGE_FRAGMENT_BIT;
    info.nextStage = stage == ShaderStage::vert ? VK_SHADER_STAGE_FRAGMENT_BIT : 0;
    info.codeType = VK_SHADER_CODE_TYPE_SPIRV_EXT;
    info.codeSize = spirv.size() * sizeof(uint32_t);
    info.pCode = spirv.data();
    info.pName = "main";

    VkShaderEXT ret = VK_NULL_HANDLE;
    CHECK_VKR(vkCreateShadersEXT(device, 1, &info, NULL, &ret));
    return ret;
  }

  void HeapBarrier(VkCommandBuffer cmd, VkPipelineStageFlags2 srcStage, VkAccessFlags2 srcAccess,
                   VkPipelineStageFlags2 dstStage, VkAccessFlags2 dstAccess)
  {
    VkMemoryBarrier2 barrier = {VK_STRUCTURE_TYPE_MEMORY_BARRIER_2};
    barrier.srcStageMask = srcStage;
    barrier.srcAccessMask = srcAccess;
    barrier.dstStageMask = dstStage;
    barrier.dstAccessMask = dstAccess;

    VkDependencyInfo dep = {VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    dep.memoryBarrierCount = 1;
    dep.pMemoryBarriers = &barrier;
    vkCmdPipelineBarrier2KHR(cmd, &dep);
  }

  int main()
  {
    vmaBDA = true;

    // initialise, create window, create context, etc
    if(!Init())
      return 3;

    VkShaderEXT vs = CreateShader(vertex, ShaderStage::vert);
    VkShaderEXT fs = CreateShader(pixel, ShaderStage::frag);

    const VkBufferUsageFlags addressable = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    const VmaAllocationCreateInfo cpuToGpu = {0, VMA_MEMORY_USAGE_CPU_TO_GPU};
    const VmaAllocationCreateInfo gpuOnly = {0, VMA_MEMORY_USAGE_GPU_ONLY};

    // sampled image with mips and layers, viewed through a sub-range with red and blue swapped
    AllocatedImage sampledImage(
        this,
        vkh::ImageCreateInfo(4, 4, 0, VK_FORMAT_R8G8B8A8_UNORM,
                             VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, 3, 2),
        gpuOnly);
    setName(sampledImage.image, "SampledImage");

    AllocatedImage storageImage(
        this,
        vkh::ImageCreateInfo(4, 4, 0, VK_FORMAT_R8G8B8A8_UNORM,
                             VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT),
        gpuOnly);
    setName(storageImage.image, "StorageImage");

    {
      VkCommandBuffer cmd = GetCommandBuffer();
      CHECK_VKR(vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo()));

      vkh::cmdPipelineBarrier(
          cmd,
          {
              vkh::ImageMemoryBarrier(0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, sampledImage.image),
              vkh::ImageMemoryBarrier(0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, storageImage.image),
          });

      vkCmdClearColorImage(cmd, sampledImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           vkh::ClearColorValue(0.75f, 0.5f, 0.25f, 1.0f), 1,
                           vkh::ImageSubresourceRange());
      vkCmdClearColorImage(cmd, storageImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           vkh::ClearColorValue(0.0f, 1.0f, 0.0f, 1.0f), 1,
                           vkh::ImageSubresourceRange());

      vkh::cmdPipelineBarrier(
          cmd,
          {
              vkh::ImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                                      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, sampledImage.image),
              vkh::ImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                                      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
                                      storageImage.image),
          });

      CHECK_VKR(vkEndCommandBuffer(cmd));
      Submit(99, 99, {cmd});
    }

    AllocatedBuffer uniformBuffer(
        this, vkh::BufferCreateInfo(256, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | addressable),
        cpuToGpu, 256);
    setName(uniformBuffer.buffer, "UniformBuffer");
    Vec4f uboData = Vec4f(1.0f, 2.0f, 3.0f, 4.0f);
    uniformBuffer.upload(&uboData, sizeof(uboData));

    AllocatedBuffer uniformBuffer2(
        this, vkh::BufferCreateInfo(256, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | addressable),
        cpuToGpu, 256);
    setName(uniformBuffer2.buffer, "UniformBuffer2");
    Vec4f ubo2Data = Vec4f(5.0f, 6.0f, 7.0f, 8.0f);
    uniformBuffer2.upload(&ubo2Data, sizeof(ubo2Data));

    // buffers whose descriptors start partway in, so offsets are visible
    AllocatedBuffer storageBuffer(
        this, vkh::BufferCreateInfo(512, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | addressable),
        cpuToGpu, 256);
    setName(storageBuffer.buffer, "StorageBuffer");
    {
      Vec4f data[32] = {};
      data[16] = Vec4f(9.0f, 10.0f, 11.0f, 12.0f);
      storageBuffer.upload(data);
    }

    AllocatedBuffer uniformTexels(
        this, vkh::BufferCreateInfo(512, VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT | addressable),
        cpuToGpu, 256);
    setName(uniformTexels.buffer, "UniformTexels");
    {
      Vec4f data[32] = {};
      data[17] = Vec4f(13.0f, 14.0f, 15.0f, 16.0f);
      uniformTexels.upload(data);
    }

    AllocatedBuffer storageTexels(
        this, vkh::BufferCreateInfo(512, VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT | addressable),
        cpuToGpu, 256);
    setName(storageTexels.buffer, "StorageTexels");
    {
      Vec4f data[32] = {};
      data[1] = Vec4f(17.0f, 18.0f, 19.0f, 20.0f);
      storageTexels.upload(data);
    }

    AllocatedBuffer blasBuffer, tlasBuffer;
    VkAccelerationStructureKHR blas = VK_NULL_HANDLE, tlas = VK_NULL_HANDLE;
    VkDeviceAddress tlasAddress = 0;
    VkDeviceSize tlasSize = 0;
    {
      Vec3f vertices[] = {
          {0.0f, 0.3f, 0.5f},
          {-0.3f, -0.3f, 0.5f},
          {0.3f, -0.3f, 0.5f},
      };
      uint32_t indices[] = {0, 1, 2};

      const VkBufferUsageFlags inputUsage =
          VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | addressable;

      AllocatedBuffer vb(this, vkh::BufferCreateInfo(sizeof(vertices), inputUsage), cpuToGpu, 4);
      AllocatedBuffer ib(this, vkh::BufferCreateInfo(sizeof(indices), inputUsage), cpuToGpu, 4);
      vb.upload(vertices);
      ib.upload(indices);

      VkAccelerationStructureGeometryKHR blasGeom = {
          VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
      blasGeom.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
      blasGeom.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
      blasGeom.geometry.triangles.sType =
          VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
      blasGeom.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
      blasGeom.geometry.triangles.maxVertex = 2;
      blasGeom.geometry.triangles.vertexStride = sizeof(Vec3f);
      blasGeom.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
      blasGeom.geometry.triangles.vertexData.deviceAddress = vb.address;
      blasGeom.geometry.triangles.indexData.deviceAddress = ib.address;

      VkAccelerationStructureBuildGeometryInfoKHR blasBuild = {
          VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
      blasBuild.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
      blasBuild.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
      blasBuild.geometryCount = 1;
      blasBuild.pGeometries = &blasGeom;

      uint32_t primCount = 1;
      VkAccelerationStructureBuildSizesInfoKHR blasSizes = {
          VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
      vkGetAccelerationStructureBuildSizesKHR(device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                              &blasBuild, &primCount, &blasSizes);

      blasBuffer = AllocatedBuffer(
          this,
          vkh::BufferCreateInfo(blasSizes.accelerationStructureSize,
                                VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | addressable),
          gpuOnly);

      VkAccelerationStructureCreateInfoKHR blasInfo = {
          VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
      blasInfo.buffer = blasBuffer.buffer;
      blasInfo.size = blasSizes.accelerationStructureSize;
      blasInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
      CHECK_VKR(vkCreateAccelerationStructureKHR(device, &blasInfo, NULL, &blas));
      setName(blas, "BLAS");

      VkAccelerationStructureDeviceAddressInfoKHR blasAddrInfo = {
          VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR, NULL, blas};
      VkDeviceAddress blasAddress = vkGetAccelerationStructureDeviceAddressKHR(device, &blasAddrInfo);

      AllocatedBuffer blasScratch(
          this,
          vkh::BufferCreateInfo(blasSizes.buildScratchSize,
                                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | addressable),
          gpuOnly, 256);
      blasBuild.scratchData.deviceAddress = blasScratch.address;
      blasBuild.dstAccelerationStructure = blas;

      VkAccelerationStructureInstanceKHR instance = {
          {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f},
          0,
          0xFF,
          0,
          VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR,
          blasAddress,
      };
      AllocatedBuffer instances(this, vkh::BufferCreateInfo(sizeof(instance), inputUsage), cpuToGpu,
                                16);
      instances.upload(&instance, sizeof(instance));

      VkAccelerationStructureGeometryKHR tlasGeom = {
          VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
      tlasGeom.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
      tlasGeom.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
      tlasGeom.geometry.instances.sType =
          VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
      tlasGeom.geometry.instances.data.deviceAddress = instances.address;

      VkAccelerationStructureBuildGeometryInfoKHR tlasBuild = {
          VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
      tlasBuild.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
      tlasBuild.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
      tlasBuild.geometryCount = 1;
      tlasBuild.pGeometries = &tlasGeom;

      VkAccelerationStructureBuildSizesInfoKHR tlasSizes = {
          VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
      vkGetAccelerationStructureBuildSizesKHR(device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                              &tlasBuild, &primCount, &tlasSizes);

      tlasBuffer = AllocatedBuffer(
          this,
          vkh::BufferCreateInfo(tlasSizes.accelerationStructureSize,
                                VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | addressable),
          gpuOnly);

      VkAccelerationStructureCreateInfoKHR tlasInfo = {
          VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
      tlasInfo.buffer = tlasBuffer.buffer;
      tlasInfo.size = tlasSizes.accelerationStructureSize;
      tlasInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
      CHECK_VKR(vkCreateAccelerationStructureKHR(device, &tlasInfo, NULL, &tlas));
      setName(tlas, "TLAS");

      VkAccelerationStructureDeviceAddressInfoKHR tlasAddrInfo = {
          VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR, NULL, tlas};
      tlasAddress = vkGetAccelerationStructureDeviceAddressKHR(device, &tlasAddrInfo);
      tlasSize = tlasSizes.accelerationStructureSize;

      AllocatedBuffer tlasScratch(
          this,
          vkh::BufferCreateInfo(tlasSizes.buildScratchSize,
                                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | addressable),
          gpuOnly, 256);
      tlasBuild.scratchData.deviceAddress = tlasScratch.address;
      tlasBuild.dstAccelerationStructure = tlas;

      VkAccelerationStructureBuildRangeInfoKHR range = {1, 0, 0, 0};
      VkAccelerationStructureBuildRangeInfoKHR *ranges = &range;

      VkCommandBuffer cmd = GetCommandBuffer();
      CHECK_VKR(vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo()));
      vkCmdBuildAccelerationStructuresKHR(cmd, 1, &blasBuild, &ranges);
      VkMemoryBarrier asBarrier = {VK_STRUCTURE_TYPE_MEMORY_BARRIER};
      asBarrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
      asBarrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
      vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                           VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1, &asBarrier,
                           0, NULL, 0, NULL);
      vkCmdBuildAccelerationStructuresKHR(cmd, 1, &tlasBuild, &ranges);
      CHECK_VKR(vkEndCommandBuffer(cmd));
      Submit(99, 99, {cmd});
      CHECK_VKR(vkQueueWaitIdle(queue));
    }

    const VkDeviceSize resourceStride =
        std::max(AlignUp(heapProps.imageDescriptorSize, heapProps.imageDescriptorAlignment),
                 AlignUp(heapProps.bufferDescriptorSize, heapProps.bufferDescriptorAlignment));
    const VkDeviceSize samplerStride =
        AlignUp(heapProps.samplerDescriptorSize, heapProps.samplerDescriptorAlignment);

    const VkDeviceSize resourceReservedOffset =
        AlignUp(resourceStride * ResourceSlotCount,
                std::max(heapProps.imageDescriptorAlignment, heapProps.bufferDescriptorAlignment));
    const VkDeviceSize samplerReservedOffset = samplerStride * 2;

    AllocatedBuffer resourceHeap(
        this,
        vkh::BufferCreateInfo(resourceReservedOffset + heapProps.minResourceHeapReservedRange,
                              VK_BUFFER_USAGE_DESCRIPTOR_HEAP_BIT_EXT |
                                  VK_BUFFER_USAGE_TRANSFER_DST_BIT | addressable),
        cpuToGpu, (uint32_t)heapProps.resourceHeapAlignment);
    setName(resourceHeap.buffer, "ResourceHeap");

    AllocatedBuffer samplerHeap(
        this,
        vkh::BufferCreateInfo(samplerReservedOffset + heapProps.minSamplerHeapReservedRange,
                              VK_BUFFER_USAGE_DESCRIPTOR_HEAP_BIT_EXT | addressable),
        cpuToGpu, (uint32_t)heapProps.samplerHeapAlignment);
    setName(samplerHeap.buffer, "SamplerHeap");

    // write every resource descriptor in one call, including the replacement uniform buffer
    // descriptor that is copied over UniformBufferSlot between the draws
    VkImageViewCreateInfo sampledView =
        vkh::ImageViewCreateInfo(sampledImage.image, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R8G8B8A8_UNORM,
                                 {VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_G,
                                  VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_A},
                                 vkh::ImageSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 1, 2, 1, 1));
    VkImageViewCreateInfo storageView = vkh::ImageViewCreateInfo(
        storageImage.image, VK_IMAGE_VIEW_TYPE_2D, VK_FORMAT_R8G8B8A8_UNORM);

    VkImageDescriptorInfoEXT sampledInfo = {VK_STRUCTURE_TYPE_IMAGE_DESCRIPTOR_INFO_EXT, NULL,
                                            &sampledView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkImageDescriptorInfoEXT storageInfo = {VK_STRUCTURE_TYPE_IMAGE_DESCRIPTOR_INFO_EXT, NULL,
                                            &storageView, VK_IMAGE_LAYOUT_GENERAL};

    VkDeviceAddressRangeEXT uboRange = {uniformBuffer.address, sizeof(Vec4f)};
    VkDeviceAddressRangeEXT ubo2Range = {uniformBuffer2.address, sizeof(Vec4f)};
    VkDeviceAddressRangeEXT ssboRange = {storageBuffer.address + 256, 64};
    VkDeviceAddressRangeEXT asRange = {tlasAddress, tlasSize};

    VkTexelBufferDescriptorInfoEXT uniformTexelInfo = {
        VK_STRUCTURE_TYPE_TEXEL_BUFFER_DESCRIPTOR_INFO_EXT,
        NULL,
        VK_FORMAT_R32G32B32A32_SFLOAT,
        {uniformTexels.address + 256, 64},
    };
    VkTexelBufferDescriptorInfoEXT storageTexelInfo = {
        VK_STRUCTURE_TYPE_TEXEL_BUFFER_DESCRIPTOR_INFO_EXT,
        NULL,
        VK_FORMAT_R32G32B32A32_SFLOAT,
        {storageTexels.address, 64},
    };

    struct ResourceWrite
    {
      VkDescriptorType type;
      const void *data;
    };

    // indices into the writes below
    const uint32_t replacementUBOWrite = ResourceSlotCount - 1;
    const ResourceWrite writes[ResourceSlotCount] = {
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, &sampledInfo},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &storageInfo},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &uboRange},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &ssboRange},
        {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, &uniformTexelInfo},
        {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, &storageTexelInfo},
        {VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, &asRange},
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, NULL},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &ubo2Range},
    };

    std::vector<byte> resourceBytes(size_t(resourceStride * ResourceSlotCount), 0);
    VkResourceDescriptorInfoEXT resourceInfos[ResourceSlotCount] = {};
    VkHostAddressRangeEXT resourceDests[ResourceSlotCount] = {};
    VkDeviceSize descSize[ResourceSlotCount] = {};

    for(uint32_t i = 0; i < ResourceSlotCount; i++)
    {
      resourceInfos[i].sType = VK_STRUCTURE_TYPE_RESOURCE_DESCRIPTOR_INFO_EXT;
      resourceInfos[i].type = writes[i].type;
      resourceInfos[i].data.pImage = (const VkImageDescriptorInfoEXT *)writes[i].data;

      descSize[i] = vkGetPhysicalDeviceDescriptorSizeEXT(phys, writes[i].type);
      resourceDests[i].address = resourceBytes.data() + resourceStride * i;
      resourceDests[i].size = (size_t)descSize[i];
    }

    CHECK_VKR(vkWriteResourceDescriptorsEXT(device, ResourceSlotCount, resourceInfos, resourceDests));

    std::vector<byte> replacementUBO(resourceBytes.begin() + resourceStride * replacementUBOWrite,
                                     resourceBytes.begin() + resourceStride * replacementUBOWrite +
                                         descSize[replacementUBOWrite]);
    std::vector<byte> originalUBO(
        resourceBytes.begin() + resourceStride * UniformBufferSlot,
        resourceBytes.begin() + resourceStride * UniformBufferSlot + descSize[UniformBufferSlot]);

    // no descriptor write produces this
    memset(resourceBytes.data() + resourceStride * UnknownSlot, 0xab, (size_t)resourceStride);

    resourceHeap.upload(resourceBytes.data(), resourceBytes.size());

    VkSamplerCreateInfo samplerInfos[2] = {};
    samplerInfos[0].sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfos[0].magFilter = VK_FILTER_NEAREST;
    samplerInfos[0].minFilter = VK_FILTER_NEAREST;
    samplerInfos[0].mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    samplerInfos[0].addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfos[0].addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfos[0].addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfos[0].maxLod = VK_LOD_CLAMP_NONE;

    samplerInfos[1].sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfos[1].magFilter = VK_FILTER_LINEAR;
    samplerInfos[1].minFilter = VK_FILTER_LINEAR;
    samplerInfos[1].mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfos[1].addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfos[1].addressModeV = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    samplerInfos[1].addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfos[1].borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    samplerInfos[1].compareEnable = VK_TRUE;
    samplerInfos[1].compareOp = VK_COMPARE_OP_LESS;
    samplerInfos[1].minLod = 1.0f;
    samplerInfos[1].maxLod = 4.0f;

    {
      std::vector<byte> samplerBytes(size_t(samplerReservedOffset), 0);
      VkHostAddressRangeEXT samplerDests[2] = {
          {samplerBytes.data(), (size_t)heapProps.samplerDescriptorSize},
          {samplerBytes.data() + samplerStride, (size_t)heapProps.samplerDescriptorSize},
      };
      CHECK_VKR(vkWriteSamplerDescriptorsEXT(device, 2, samplerInfos, samplerDests));
      samplerHeap.upload(samplerBytes.data(), samplerBytes.size());
    }

    // shaders index heap arrays of images and texel buffers in units of imageDescriptorSize, and
    // arrays of buffers and acceleration structures in units of bufferDescriptorSize
    auto shaderIndex = [&](uint32_t slot, VkDeviceSize arrayStride) {
      if((slot * resourceStride) % arrayStride != 0)
        TEST_ERROR("Slot %u isn't a multiple of array stride %llu", slot,
                   (unsigned long long)arrayStride);
      return uint32_t(slot * resourceStride / arrayStride);
    };

    PushData push = {};
    push.sampledImage = shaderIndex(SampledImageSlot, heapProps.imageDescriptorSize);
    push.storageImage = shaderIndex(StorageImageSlot, heapProps.imageDescriptorSize);
    push.ubo = shaderIndex(UniformBufferSlot, heapProps.bufferDescriptorSize);
    push.ssbo = shaderIndex(StorageBufferSlot, heapProps.bufferDescriptorSize);
    push.uniformTexel = shaderIndex(UniformTexelSlot, heapProps.imageDescriptorSize);
    push.storageTexel = shaderIndex(StorageTexelSlot, heapProps.imageDescriptorSize);
    push.accelStruct = shaderIndex(AccelStructSlot, heapProps.bufferDescriptorSize);
    push.samp = 0;

    VkBindHeapInfoEXT resourceBind = {VK_STRUCTURE_TYPE_BIND_HEAP_INFO_EXT};
    resourceBind.heapRange = {resourceHeap.address,
                              resourceReservedOffset + heapProps.minResourceHeapReservedRange};
    resourceBind.reservedRangeOffset = resourceReservedOffset;
    resourceBind.reservedRangeSize = heapProps.minResourceHeapReservedRange;

    VkBindHeapInfoEXT samplerBind = {VK_STRUCTURE_TYPE_BIND_HEAP_INFO_EXT};
    samplerBind.heapRange = {samplerHeap.address,
                             samplerReservedOffset + heapProps.minSamplerHeapReservedRange};
    samplerBind.reservedRangeOffset = samplerReservedOffset;
    samplerBind.reservedRangeSize = heapProps.minSamplerHeapReservedRange;

    const VkDeviceSize uboSlotOffset = resourceStride * UniformBufferSlot;

    while(Running())
    {
      VkCommandBuffer cmd = GetCommandBuffer();

      vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());

      StartUsingBackbuffer(cmd, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

      // undo the previous frame's replacement
      vkCmdUpdateBuffer(cmd, resourceHeap.buffer, uboSlotOffset, originalUBO.size(),
                        originalUBO.data());
      HeapBarrier(cmd, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
                  VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_RESOURCE_HEAP_READ_BIT_EXT);

      vkCmdBindResourceHeapEXT(cmd, &resourceBind);
      vkCmdBindSamplerHeapEXT(cmd, &samplerBind);

      VkShaderStageFlagBits stages[2] = {VK_SHADER_STAGE_VERTEX_BIT, VK_SHADER_STAGE_FRAGMENT_BIT};
      VkShaderEXT shaderObjs[2] = {vs, fs};
      vkCmdBindShadersEXT(cmd, 2, stages, shaderObjs);

      vkCmdSetScissorWithCountEXT(cmd, 1, &mainWindow->scissor);
      vkCmdSetRasterizerDiscardEnableEXT(cmd, VK_FALSE);
      vkCmdSetVertexInputEXT(cmd, 0, NULL, 0, NULL);
      vkCmdSetPrimitiveTopologyEXT(cmd, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
      vkCmdSetPrimitiveRestartEnableEXT(cmd, VK_FALSE);
      vkCmdSetPolygonModeEXT(cmd, VK_POLYGON_MODE_FILL);
      vkCmdSetCullModeEXT(cmd, VK_CULL_MODE_NONE);
      vkCmdSetFrontFaceEXT(cmd, VK_FRONT_FACE_CLOCKWISE);
      vkCmdSetRasterizationSamplesEXT(cmd, VK_SAMPLE_COUNT_1_BIT);
      VkSampleMask sampleMask = ~0U;
      vkCmdSetSampleMaskEXT(cmd, VK_SAMPLE_COUNT_1_BIT, &sampleMask);
      vkCmdSetAlphaToCoverageEnableEXT(cmd, VK_FALSE);
      vkCmdSetDepthTestEnableEXT(cmd, VK_FALSE);
      vkCmdSetDepthWriteEnableEXT(cmd, VK_FALSE);
      vkCmdSetDepthBiasEnableEXT(cmd, VK_FALSE);
      vkCmdSetStencilTestEnableEXT(cmd, VK_FALSE);
      VkBool32 blendEnable = VK_FALSE;
      vkCmdSetColorBlendEnableEXT(cmd, 0, 1, &blendEnable);
      VkColorComponentFlags writeMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
      vkCmdSetColorWriteMaskEXT(cmd, 0, 1, &writeMask);

      VkRenderingAttachmentInfoKHR colAtt = {VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR};
      colAtt.imageView = mainWindow->GetView();
      colAtt.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
      colAtt.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
      colAtt.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
      colAtt.clearValue = vkh::ClearValue(0.2f, 0.2f, 0.2f, 1.0f);

      VkRenderingInfoKHR rendInfo = {VK_STRUCTURE_TYPE_RENDERING_INFO_KHR};
      rendInfo.renderArea = mainWindow->scissor;
      rendInfo.layerCount = 1;
      rendInfo.colorAttachmentCount = 1;
      rendInfo.pColorAttachments = &colAtt;

      VkViewport view = mainWindow->viewport;
      view.width /= 2.0f;

      vkCmdBeginRenderingKHR(cmd, &rendInfo);

      push.expectedUBO = uboData;
      VkPushDataInfoEXT pushInfo = {VK_STRUCTURE_TYPE_PUSH_DATA_INFO_EXT};
      pushInfo.data.address = &push;
      pushInfo.data.size = sizeof(push);
      vkCmdPushDataEXT(cmd, &pushInfo);

      vkCmdSetViewportWithCountEXT(cmd, 1, &view);
      setMarker(cmd, "First Draw");
      vkCmdDraw(cmd, 3, 1, 0, 0);

      vkCmdEndRenderingKHR(cmd);

      HeapBarrier(cmd, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_NONE,
                  VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_NONE);
      vkCmdUpdateBuffer(cmd, resourceHeap.buffer, uboSlotOffset, replacementUBO.size(),
                        replacementUBO.data());
      HeapBarrier(cmd, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
                  VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_RESOURCE_HEAP_READ_BIT_EXT);

      colAtt.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
      vkCmdBeginRenderingKHR(cmd, &rendInfo);

      push.expectedUBO = ubo2Data;
      vkCmdPushDataEXT(cmd, &pushInfo);

      view.x += view.width;
      vkCmdSetViewportWithCountEXT(cmd, 1, &view);
      setMarker(cmd, "Second Draw");
      vkCmdDraw(cmd, 3, 1, 0, 0);

      vkCmdEndRenderingKHR(cmd);

      FinishUsingBackbuffer(cmd, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL);

      vkEndCommandBuffer(cmd);

      Submit(0, 1, {cmd});

      Present();
    }

    CHECK_VKR(vkDeviceWaitIdle(device));

    vkDestroyShaderEXT(device, vs, NULL);
    vkDestroyShaderEXT(device, fs, NULL);
    vkDestroyAccelerationStructureKHR(device, tlas, NULL);
    vkDestroyAccelerationStructureKHR(device, blas, NULL);

    return 0;
  }
};

REGISTER_TEST();
