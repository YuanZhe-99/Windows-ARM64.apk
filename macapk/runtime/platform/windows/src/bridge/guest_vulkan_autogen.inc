// Auto-generated from locked Vulkan-Headers 1.4.363 <vulkan/vulkan_core.h>.
// Covers Vulkan 1.0, 1.1, 1.2, 1.3 core + promoted/KHR/EXT instance, physical-device,
// device, queue and command-buffer entry points with allocator & pNext boundary checks.

#ifdef GUEST_VULKAN_STAGE_TABLE
struct DeviceDispatchTable {
    PFN_vkQueueSubmit vkQueueSubmit = nullptr;
    PFN_vkQueueWaitIdle vkQueueWaitIdle = nullptr;
    PFN_vkDeviceWaitIdle vkDeviceWaitIdle = nullptr;
    PFN_vkAllocateMemory vkAllocateMemory = nullptr;
    PFN_vkFreeMemory vkFreeMemory = nullptr;
    PFN_vkMapMemory vkMapMemory = nullptr;
    PFN_vkUnmapMemory vkUnmapMemory = nullptr;
    PFN_vkFlushMappedMemoryRanges vkFlushMappedMemoryRanges = nullptr;
    PFN_vkInvalidateMappedMemoryRanges vkInvalidateMappedMemoryRanges = nullptr;
    PFN_vkGetDeviceMemoryCommitment vkGetDeviceMemoryCommitment = nullptr;
    PFN_vkBindBufferMemory vkBindBufferMemory = nullptr;
    PFN_vkBindImageMemory vkBindImageMemory = nullptr;
    PFN_vkGetBufferMemoryRequirements vkGetBufferMemoryRequirements = nullptr;
    PFN_vkGetImageMemoryRequirements vkGetImageMemoryRequirements = nullptr;
    PFN_vkGetImageSparseMemoryRequirements vkGetImageSparseMemoryRequirements = nullptr;
    PFN_vkQueueBindSparse vkQueueBindSparse = nullptr;
    PFN_vkCreateFence vkCreateFence = nullptr;
    PFN_vkDestroyFence vkDestroyFence = nullptr;
    PFN_vkResetFences vkResetFences = nullptr;
    PFN_vkGetFenceStatus vkGetFenceStatus = nullptr;
    PFN_vkWaitForFences vkWaitForFences = nullptr;
    PFN_vkCreateSemaphore vkCreateSemaphore = nullptr;
    PFN_vkDestroySemaphore vkDestroySemaphore = nullptr;
    PFN_vkCreateQueryPool vkCreateQueryPool = nullptr;
    PFN_vkDestroyQueryPool vkDestroyQueryPool = nullptr;
    PFN_vkGetQueryPoolResults vkGetQueryPoolResults = nullptr;
    PFN_vkCreateBuffer vkCreateBuffer = nullptr;
    PFN_vkDestroyBuffer vkDestroyBuffer = nullptr;
    PFN_vkCreateImage vkCreateImage = nullptr;
    PFN_vkDestroyImage vkDestroyImage = nullptr;
    PFN_vkGetImageSubresourceLayout vkGetImageSubresourceLayout = nullptr;
    PFN_vkCreateImageView vkCreateImageView = nullptr;
    PFN_vkDestroyImageView vkDestroyImageView = nullptr;
    PFN_vkCreateCommandPool vkCreateCommandPool = nullptr;
    PFN_vkResetCommandPool vkResetCommandPool = nullptr;
    PFN_vkBeginCommandBuffer vkBeginCommandBuffer = nullptr;
    PFN_vkEndCommandBuffer vkEndCommandBuffer = nullptr;
    PFN_vkResetCommandBuffer vkResetCommandBuffer = nullptr;
    PFN_vkCmdCopyBuffer vkCmdCopyBuffer = nullptr;
    PFN_vkCmdCopyImage vkCmdCopyImage = nullptr;
    PFN_vkCmdCopyBufferToImage vkCmdCopyBufferToImage = nullptr;
    PFN_vkCmdCopyImageToBuffer vkCmdCopyImageToBuffer = nullptr;
    PFN_vkCmdUpdateBuffer vkCmdUpdateBuffer = nullptr;
    PFN_vkCmdFillBuffer vkCmdFillBuffer = nullptr;
    PFN_vkCmdPipelineBarrier vkCmdPipelineBarrier = nullptr;
    PFN_vkCmdBeginQuery vkCmdBeginQuery = nullptr;
    PFN_vkCmdEndQuery vkCmdEndQuery = nullptr;
    PFN_vkCmdResetQueryPool vkCmdResetQueryPool = nullptr;
    PFN_vkCmdWriteTimestamp vkCmdWriteTimestamp = nullptr;
    PFN_vkCmdCopyQueryPoolResults vkCmdCopyQueryPoolResults = nullptr;
    PFN_vkCmdExecuteCommands vkCmdExecuteCommands = nullptr;
    PFN_vkCreateEvent vkCreateEvent = nullptr;
    PFN_vkDestroyEvent vkDestroyEvent = nullptr;
    PFN_vkGetEventStatus vkGetEventStatus = nullptr;
    PFN_vkSetEvent vkSetEvent = nullptr;
    PFN_vkResetEvent vkResetEvent = nullptr;
    PFN_vkCreateBufferView vkCreateBufferView = nullptr;
    PFN_vkDestroyBufferView vkDestroyBufferView = nullptr;
    PFN_vkCreateShaderModule vkCreateShaderModule = nullptr;
    PFN_vkDestroyShaderModule vkDestroyShaderModule = nullptr;
    PFN_vkCreatePipelineCache vkCreatePipelineCache = nullptr;
    PFN_vkDestroyPipelineCache vkDestroyPipelineCache = nullptr;
    PFN_vkGetPipelineCacheData vkGetPipelineCacheData = nullptr;
    PFN_vkMergePipelineCaches vkMergePipelineCaches = nullptr;
    PFN_vkCreateComputePipelines vkCreateComputePipelines = nullptr;
    PFN_vkDestroyPipeline vkDestroyPipeline = nullptr;
    PFN_vkCreatePipelineLayout vkCreatePipelineLayout = nullptr;
    PFN_vkDestroyPipelineLayout vkDestroyPipelineLayout = nullptr;
    PFN_vkCreateSampler vkCreateSampler = nullptr;
    PFN_vkDestroySampler vkDestroySampler = nullptr;
    PFN_vkCreateDescriptorSetLayout vkCreateDescriptorSetLayout = nullptr;
    PFN_vkDestroyDescriptorSetLayout vkDestroyDescriptorSetLayout = nullptr;
    PFN_vkCreateDescriptorPool vkCreateDescriptorPool = nullptr;
    PFN_vkDestroyDescriptorPool vkDestroyDescriptorPool = nullptr;
    PFN_vkResetDescriptorPool vkResetDescriptorPool = nullptr;
    PFN_vkAllocateDescriptorSets vkAllocateDescriptorSets = nullptr;
    PFN_vkFreeDescriptorSets vkFreeDescriptorSets = nullptr;
    PFN_vkUpdateDescriptorSets vkUpdateDescriptorSets = nullptr;
    PFN_vkCmdBindPipeline vkCmdBindPipeline = nullptr;
    PFN_vkCmdBindDescriptorSets vkCmdBindDescriptorSets = nullptr;
    PFN_vkCmdClearColorImage vkCmdClearColorImage = nullptr;
    PFN_vkCmdDispatch vkCmdDispatch = nullptr;
    PFN_vkCmdDispatchIndirect vkCmdDispatchIndirect = nullptr;
    PFN_vkCmdSetEvent vkCmdSetEvent = nullptr;
    PFN_vkCmdResetEvent vkCmdResetEvent = nullptr;
    PFN_vkCmdWaitEvents vkCmdWaitEvents = nullptr;
    PFN_vkCmdPushConstants vkCmdPushConstants = nullptr;
    PFN_vkCreateGraphicsPipelines vkCreateGraphicsPipelines = nullptr;
    PFN_vkCreateFramebuffer vkCreateFramebuffer = nullptr;
    PFN_vkDestroyFramebuffer vkDestroyFramebuffer = nullptr;
    PFN_vkCreateRenderPass vkCreateRenderPass = nullptr;
    PFN_vkDestroyRenderPass vkDestroyRenderPass = nullptr;
    PFN_vkGetRenderAreaGranularity vkGetRenderAreaGranularity = nullptr;
    PFN_vkCmdSetViewport vkCmdSetViewport = nullptr;
    PFN_vkCmdSetScissor vkCmdSetScissor = nullptr;
    PFN_vkCmdSetLineWidth vkCmdSetLineWidth = nullptr;
    PFN_vkCmdSetDepthBias vkCmdSetDepthBias = nullptr;
    PFN_vkCmdSetBlendConstants vkCmdSetBlendConstants = nullptr;
    PFN_vkCmdSetDepthBounds vkCmdSetDepthBounds = nullptr;
    PFN_vkCmdSetStencilCompareMask vkCmdSetStencilCompareMask = nullptr;
    PFN_vkCmdSetStencilWriteMask vkCmdSetStencilWriteMask = nullptr;
    PFN_vkCmdSetStencilReference vkCmdSetStencilReference = nullptr;
    PFN_vkCmdBindIndexBuffer vkCmdBindIndexBuffer = nullptr;
    PFN_vkCmdBindVertexBuffers vkCmdBindVertexBuffers = nullptr;
    PFN_vkCmdDraw vkCmdDraw = nullptr;
    PFN_vkCmdDrawIndexed vkCmdDrawIndexed = nullptr;
    PFN_vkCmdDrawIndirect vkCmdDrawIndirect = nullptr;
    PFN_vkCmdDrawIndexedIndirect vkCmdDrawIndexedIndirect = nullptr;
    PFN_vkCmdBlitImage vkCmdBlitImage = nullptr;
    PFN_vkCmdClearDepthStencilImage vkCmdClearDepthStencilImage = nullptr;
    PFN_vkCmdClearAttachments vkCmdClearAttachments = nullptr;
    PFN_vkCmdResolveImage vkCmdResolveImage = nullptr;
    PFN_vkCmdBeginRenderPass vkCmdBeginRenderPass = nullptr;
    PFN_vkCmdNextSubpass vkCmdNextSubpass = nullptr;
    PFN_vkCmdEndRenderPass vkCmdEndRenderPass = nullptr;
    PFN_vkBindBufferMemory2 vkBindBufferMemory2 = nullptr;
    PFN_vkBindImageMemory2 vkBindImageMemory2 = nullptr;
    PFN_vkGetDeviceGroupPeerMemoryFeatures vkGetDeviceGroupPeerMemoryFeatures = nullptr;
    PFN_vkCmdSetDeviceMask vkCmdSetDeviceMask = nullptr;
    PFN_vkGetImageMemoryRequirements2 vkGetImageMemoryRequirements2 = nullptr;
    PFN_vkGetBufferMemoryRequirements2 vkGetBufferMemoryRequirements2 = nullptr;
    PFN_vkGetImageSparseMemoryRequirements2 vkGetImageSparseMemoryRequirements2 = nullptr;
    PFN_vkTrimCommandPool vkTrimCommandPool = nullptr;
    PFN_vkCmdDispatchBase vkCmdDispatchBase = nullptr;
    PFN_vkCreateDescriptorUpdateTemplate vkCreateDescriptorUpdateTemplate = nullptr;
    PFN_vkDestroyDescriptorUpdateTemplate vkDestroyDescriptorUpdateTemplate = nullptr;
    PFN_vkUpdateDescriptorSetWithTemplate vkUpdateDescriptorSetWithTemplate = nullptr;
    PFN_vkGetDescriptorSetLayoutSupport vkGetDescriptorSetLayoutSupport = nullptr;
    PFN_vkCreateSamplerYcbcrConversion vkCreateSamplerYcbcrConversion = nullptr;
    PFN_vkDestroySamplerYcbcrConversion vkDestroySamplerYcbcrConversion = nullptr;
    PFN_vkResetQueryPool vkResetQueryPool = nullptr;
    PFN_vkGetSemaphoreCounterValue vkGetSemaphoreCounterValue = nullptr;
    PFN_vkWaitSemaphores vkWaitSemaphores = nullptr;
    PFN_vkSignalSemaphore vkSignalSemaphore = nullptr;
    PFN_vkGetBufferDeviceAddress vkGetBufferDeviceAddress = nullptr;
    PFN_vkGetBufferOpaqueCaptureAddress vkGetBufferOpaqueCaptureAddress = nullptr;
    PFN_vkGetDeviceMemoryOpaqueCaptureAddress vkGetDeviceMemoryOpaqueCaptureAddress = nullptr;
    PFN_vkCmdDrawIndirectCount vkCmdDrawIndirectCount = nullptr;
    PFN_vkCmdDrawIndexedIndirectCount vkCmdDrawIndexedIndirectCount = nullptr;
    PFN_vkCreateRenderPass2 vkCreateRenderPass2 = nullptr;
    PFN_vkCmdBeginRenderPass2 vkCmdBeginRenderPass2 = nullptr;
    PFN_vkCmdNextSubpass2 vkCmdNextSubpass2 = nullptr;
    PFN_vkCmdEndRenderPass2 vkCmdEndRenderPass2 = nullptr;
    PFN_vkCreatePrivateDataSlot vkCreatePrivateDataSlot = nullptr;
    PFN_vkDestroyPrivateDataSlot vkDestroyPrivateDataSlot = nullptr;
    PFN_vkSetPrivateData vkSetPrivateData = nullptr;
    PFN_vkGetPrivateData vkGetPrivateData = nullptr;
    PFN_vkCmdPipelineBarrier2 vkCmdPipelineBarrier2 = nullptr;
    PFN_vkCmdWriteTimestamp2 vkCmdWriteTimestamp2 = nullptr;
    PFN_vkQueueSubmit2 vkQueueSubmit2 = nullptr;
    PFN_vkCmdCopyBuffer2 vkCmdCopyBuffer2 = nullptr;
    PFN_vkCmdCopyImage2 vkCmdCopyImage2 = nullptr;
    PFN_vkCmdCopyBufferToImage2 vkCmdCopyBufferToImage2 = nullptr;
    PFN_vkCmdCopyImageToBuffer2 vkCmdCopyImageToBuffer2 = nullptr;
    PFN_vkGetDeviceBufferMemoryRequirements vkGetDeviceBufferMemoryRequirements = nullptr;
    PFN_vkGetDeviceImageMemoryRequirements vkGetDeviceImageMemoryRequirements = nullptr;
    PFN_vkGetDeviceImageSparseMemoryRequirements vkGetDeviceImageSparseMemoryRequirements = nullptr;
    PFN_vkCmdSetEvent2 vkCmdSetEvent2 = nullptr;
    PFN_vkCmdResetEvent2 vkCmdResetEvent2 = nullptr;
    PFN_vkCmdWaitEvents2 vkCmdWaitEvents2 = nullptr;
    PFN_vkCmdBlitImage2 vkCmdBlitImage2 = nullptr;
    PFN_vkCmdResolveImage2 vkCmdResolveImage2 = nullptr;
    PFN_vkCmdBeginRendering vkCmdBeginRendering = nullptr;
    PFN_vkCmdEndRendering vkCmdEndRendering = nullptr;
    PFN_vkCmdSetCullMode vkCmdSetCullMode = nullptr;
    PFN_vkCmdSetFrontFace vkCmdSetFrontFace = nullptr;
    PFN_vkCmdSetPrimitiveTopology vkCmdSetPrimitiveTopology = nullptr;
    PFN_vkCmdSetViewportWithCount vkCmdSetViewportWithCount = nullptr;
    PFN_vkCmdSetScissorWithCount vkCmdSetScissorWithCount = nullptr;
    PFN_vkCmdBindVertexBuffers2 vkCmdBindVertexBuffers2 = nullptr;
    PFN_vkCmdSetDepthTestEnable vkCmdSetDepthTestEnable = nullptr;
    PFN_vkCmdSetDepthWriteEnable vkCmdSetDepthWriteEnable = nullptr;
    PFN_vkCmdSetDepthCompareOp vkCmdSetDepthCompareOp = nullptr;
    PFN_vkCmdSetDepthBoundsTestEnable vkCmdSetDepthBoundsTestEnable = nullptr;
    PFN_vkCmdSetStencilTestEnable vkCmdSetStencilTestEnable = nullptr;
    PFN_vkCmdSetStencilOp vkCmdSetStencilOp = nullptr;
    PFN_vkCmdSetRasterizerDiscardEnable vkCmdSetRasterizerDiscardEnable = nullptr;
    PFN_vkCmdSetDepthBiasEnable vkCmdSetDepthBiasEnable = nullptr;
    PFN_vkCmdSetPrimitiveRestartEnable vkCmdSetPrimitiveRestartEnable = nullptr;
    PFN_vkCmdBeginRenderingKHR vkCmdBeginRenderingKHR = nullptr;
    PFN_vkCmdEndRenderingKHR vkCmdEndRenderingKHR = nullptr;
    PFN_vkTrimCommandPoolKHR vkTrimCommandPoolKHR = nullptr;
    PFN_vkCmdPushDescriptorSetKHR vkCmdPushDescriptorSetKHR = nullptr;
    PFN_vkCmdPushDescriptorSetWithTemplateKHR vkCmdPushDescriptorSetWithTemplateKHR = nullptr;
    PFN_vkCreateDescriptorUpdateTemplateKHR vkCreateDescriptorUpdateTemplateKHR = nullptr;
    PFN_vkDestroyDescriptorUpdateTemplateKHR vkDestroyDescriptorUpdateTemplateKHR = nullptr;
    PFN_vkUpdateDescriptorSetWithTemplateKHR vkUpdateDescriptorSetWithTemplateKHR = nullptr;
    PFN_vkCreateRenderPass2KHR vkCreateRenderPass2KHR = nullptr;
    PFN_vkCmdBeginRenderPass2KHR vkCmdBeginRenderPass2KHR = nullptr;
    PFN_vkCmdNextSubpass2KHR vkCmdNextSubpass2KHR = nullptr;
    PFN_vkCmdEndRenderPass2KHR vkCmdEndRenderPass2KHR = nullptr;
    PFN_vkGetImageMemoryRequirements2KHR vkGetImageMemoryRequirements2KHR = nullptr;
    PFN_vkGetBufferMemoryRequirements2KHR vkGetBufferMemoryRequirements2KHR = nullptr;
    PFN_vkGetImageSparseMemoryRequirements2KHR vkGetImageSparseMemoryRequirements2KHR = nullptr;
    PFN_vkCreateSamplerYcbcrConversionKHR vkCreateSamplerYcbcrConversionKHR = nullptr;
    PFN_vkDestroySamplerYcbcrConversionKHR vkDestroySamplerYcbcrConversionKHR = nullptr;
    PFN_vkBindBufferMemory2KHR vkBindBufferMemory2KHR = nullptr;
    PFN_vkBindImageMemory2KHR vkBindImageMemory2KHR = nullptr;
    PFN_vkGetDescriptorSetLayoutSupportKHR vkGetDescriptorSetLayoutSupportKHR = nullptr;
    PFN_vkCmdDrawIndirectCountKHR vkCmdDrawIndirectCountKHR = nullptr;
    PFN_vkCmdDrawIndexedIndirectCountKHR vkCmdDrawIndexedIndirectCountKHR = nullptr;
    PFN_vkGetSemaphoreCounterValueKHR vkGetSemaphoreCounterValueKHR = nullptr;
    PFN_vkWaitSemaphoresKHR vkWaitSemaphoresKHR = nullptr;
    PFN_vkSignalSemaphoreKHR vkSignalSemaphoreKHR = nullptr;
    PFN_vkGetBufferDeviceAddressKHR vkGetBufferDeviceAddressKHR = nullptr;
    PFN_vkGetBufferOpaqueCaptureAddressKHR vkGetBufferOpaqueCaptureAddressKHR = nullptr;
    PFN_vkGetDeviceMemoryOpaqueCaptureAddressKHR vkGetDeviceMemoryOpaqueCaptureAddressKHR = nullptr;
    PFN_vkCmdSetEvent2KHR vkCmdSetEvent2KHR = nullptr;
    PFN_vkCmdResetEvent2KHR vkCmdResetEvent2KHR = nullptr;
    PFN_vkCmdWaitEvents2KHR vkCmdWaitEvents2KHR = nullptr;
    PFN_vkCmdPipelineBarrier2KHR vkCmdPipelineBarrier2KHR = nullptr;
    PFN_vkCmdWriteTimestamp2KHR vkCmdWriteTimestamp2KHR = nullptr;
    PFN_vkQueueSubmit2KHR vkQueueSubmit2KHR = nullptr;
    PFN_vkCmdCopyBuffer2KHR vkCmdCopyBuffer2KHR = nullptr;
    PFN_vkCmdCopyImage2KHR vkCmdCopyImage2KHR = nullptr;
    PFN_vkCmdCopyBufferToImage2KHR vkCmdCopyBufferToImage2KHR = nullptr;
    PFN_vkCmdCopyImageToBuffer2KHR vkCmdCopyImageToBuffer2KHR = nullptr;
    PFN_vkCmdBlitImage2KHR vkCmdBlitImage2KHR = nullptr;
    PFN_vkCmdResolveImage2KHR vkCmdResolveImage2KHR = nullptr;
    PFN_vkGetDeviceBufferMemoryRequirementsKHR vkGetDeviceBufferMemoryRequirementsKHR = nullptr;
    PFN_vkGetDeviceImageMemoryRequirementsKHR vkGetDeviceImageMemoryRequirementsKHR = nullptr;
    PFN_vkGetDeviceImageSparseMemoryRequirementsKHR vkGetDeviceImageSparseMemoryRequirementsKHR = nullptr;
    PFN_vkCmdSetCullModeEXT vkCmdSetCullModeEXT = nullptr;
    PFN_vkCmdSetFrontFaceEXT vkCmdSetFrontFaceEXT = nullptr;
    PFN_vkCmdSetPrimitiveTopologyEXT vkCmdSetPrimitiveTopologyEXT = nullptr;
    PFN_vkCmdSetViewportWithCountEXT vkCmdSetViewportWithCountEXT = nullptr;
    PFN_vkCmdSetScissorWithCountEXT vkCmdSetScissorWithCountEXT = nullptr;
    PFN_vkCmdBindVertexBuffers2EXT vkCmdBindVertexBuffers2EXT = nullptr;
    PFN_vkCmdSetDepthTestEnableEXT vkCmdSetDepthTestEnableEXT = nullptr;
    PFN_vkCmdSetDepthWriteEnableEXT vkCmdSetDepthWriteEnableEXT = nullptr;
    PFN_vkCmdSetDepthCompareOpEXT vkCmdSetDepthCompareOpEXT = nullptr;
    PFN_vkCmdSetDepthBoundsTestEnableEXT vkCmdSetDepthBoundsTestEnableEXT = nullptr;
    PFN_vkCmdSetStencilTestEnableEXT vkCmdSetStencilTestEnableEXT = nullptr;
    PFN_vkCmdSetStencilOpEXT vkCmdSetStencilOpEXT = nullptr;
    PFN_vkCmdSetPatchControlPointsEXT vkCmdSetPatchControlPointsEXT = nullptr;
    PFN_vkCmdSetRasterizerDiscardEnableEXT vkCmdSetRasterizerDiscardEnableEXT = nullptr;
    PFN_vkCmdSetDepthBiasEnableEXT vkCmdSetDepthBiasEnableEXT = nullptr;
    PFN_vkCmdSetLogicOpEXT vkCmdSetLogicOpEXT = nullptr;
    PFN_vkCmdSetPrimitiveRestartEnableEXT vkCmdSetPrimitiveRestartEnableEXT = nullptr;
};

void PopulateDeviceDispatchTable(VkDevice dev, PFN_vkGetDeviceProcAddr gdpa, DeviceDispatchTable& dt) {
    dt.vkQueueSubmit = reinterpret_cast<PFN_vkQueueSubmit>(gdpa(dev, "vkQueueSubmit"));
    dt.vkQueueWaitIdle = reinterpret_cast<PFN_vkQueueWaitIdle>(gdpa(dev, "vkQueueWaitIdle"));
    dt.vkDeviceWaitIdle = reinterpret_cast<PFN_vkDeviceWaitIdle>(gdpa(dev, "vkDeviceWaitIdle"));
    dt.vkAllocateMemory = reinterpret_cast<PFN_vkAllocateMemory>(gdpa(dev, "vkAllocateMemory"));
    dt.vkFreeMemory = reinterpret_cast<PFN_vkFreeMemory>(gdpa(dev, "vkFreeMemory"));
    dt.vkMapMemory = reinterpret_cast<PFN_vkMapMemory>(gdpa(dev, "vkMapMemory"));
    dt.vkUnmapMemory = reinterpret_cast<PFN_vkUnmapMemory>(gdpa(dev, "vkUnmapMemory"));
    dt.vkFlushMappedMemoryRanges = reinterpret_cast<PFN_vkFlushMappedMemoryRanges>(gdpa(dev, "vkFlushMappedMemoryRanges"));
    dt.vkInvalidateMappedMemoryRanges = reinterpret_cast<PFN_vkInvalidateMappedMemoryRanges>(gdpa(dev, "vkInvalidateMappedMemoryRanges"));
    dt.vkGetDeviceMemoryCommitment = reinterpret_cast<PFN_vkGetDeviceMemoryCommitment>(gdpa(dev, "vkGetDeviceMemoryCommitment"));
    dt.vkBindBufferMemory = reinterpret_cast<PFN_vkBindBufferMemory>(gdpa(dev, "vkBindBufferMemory"));
    dt.vkBindImageMemory = reinterpret_cast<PFN_vkBindImageMemory>(gdpa(dev, "vkBindImageMemory"));
    dt.vkGetBufferMemoryRequirements = reinterpret_cast<PFN_vkGetBufferMemoryRequirements>(gdpa(dev, "vkGetBufferMemoryRequirements"));
    dt.vkGetImageMemoryRequirements = reinterpret_cast<PFN_vkGetImageMemoryRequirements>(gdpa(dev, "vkGetImageMemoryRequirements"));
    dt.vkGetImageSparseMemoryRequirements = reinterpret_cast<PFN_vkGetImageSparseMemoryRequirements>(gdpa(dev, "vkGetImageSparseMemoryRequirements"));
    dt.vkQueueBindSparse = reinterpret_cast<PFN_vkQueueBindSparse>(gdpa(dev, "vkQueueBindSparse"));
    dt.vkCreateFence = reinterpret_cast<PFN_vkCreateFence>(gdpa(dev, "vkCreateFence"));
    dt.vkDestroyFence = reinterpret_cast<PFN_vkDestroyFence>(gdpa(dev, "vkDestroyFence"));
    dt.vkResetFences = reinterpret_cast<PFN_vkResetFences>(gdpa(dev, "vkResetFences"));
    dt.vkGetFenceStatus = reinterpret_cast<PFN_vkGetFenceStatus>(gdpa(dev, "vkGetFenceStatus"));
    dt.vkWaitForFences = reinterpret_cast<PFN_vkWaitForFences>(gdpa(dev, "vkWaitForFences"));
    dt.vkCreateSemaphore = reinterpret_cast<PFN_vkCreateSemaphore>(gdpa(dev, "vkCreateSemaphore"));
    dt.vkDestroySemaphore = reinterpret_cast<PFN_vkDestroySemaphore>(gdpa(dev, "vkDestroySemaphore"));
    dt.vkCreateQueryPool = reinterpret_cast<PFN_vkCreateQueryPool>(gdpa(dev, "vkCreateQueryPool"));
    dt.vkDestroyQueryPool = reinterpret_cast<PFN_vkDestroyQueryPool>(gdpa(dev, "vkDestroyQueryPool"));
    dt.vkGetQueryPoolResults = reinterpret_cast<PFN_vkGetQueryPoolResults>(gdpa(dev, "vkGetQueryPoolResults"));
    dt.vkCreateBuffer = reinterpret_cast<PFN_vkCreateBuffer>(gdpa(dev, "vkCreateBuffer"));
    dt.vkDestroyBuffer = reinterpret_cast<PFN_vkDestroyBuffer>(gdpa(dev, "vkDestroyBuffer"));
    dt.vkCreateImage = reinterpret_cast<PFN_vkCreateImage>(gdpa(dev, "vkCreateImage"));
    dt.vkDestroyImage = reinterpret_cast<PFN_vkDestroyImage>(gdpa(dev, "vkDestroyImage"));
    dt.vkGetImageSubresourceLayout = reinterpret_cast<PFN_vkGetImageSubresourceLayout>(gdpa(dev, "vkGetImageSubresourceLayout"));
    dt.vkCreateImageView = reinterpret_cast<PFN_vkCreateImageView>(gdpa(dev, "vkCreateImageView"));
    dt.vkDestroyImageView = reinterpret_cast<PFN_vkDestroyImageView>(gdpa(dev, "vkDestroyImageView"));
    dt.vkCreateCommandPool = reinterpret_cast<PFN_vkCreateCommandPool>(gdpa(dev, "vkCreateCommandPool"));
    dt.vkResetCommandPool = reinterpret_cast<PFN_vkResetCommandPool>(gdpa(dev, "vkResetCommandPool"));
    dt.vkBeginCommandBuffer = reinterpret_cast<PFN_vkBeginCommandBuffer>(gdpa(dev, "vkBeginCommandBuffer"));
    dt.vkEndCommandBuffer = reinterpret_cast<PFN_vkEndCommandBuffer>(gdpa(dev, "vkEndCommandBuffer"));
    dt.vkResetCommandBuffer = reinterpret_cast<PFN_vkResetCommandBuffer>(gdpa(dev, "vkResetCommandBuffer"));
    dt.vkCmdCopyBuffer = reinterpret_cast<PFN_vkCmdCopyBuffer>(gdpa(dev, "vkCmdCopyBuffer"));
    dt.vkCmdCopyImage = reinterpret_cast<PFN_vkCmdCopyImage>(gdpa(dev, "vkCmdCopyImage"));
    dt.vkCmdCopyBufferToImage = reinterpret_cast<PFN_vkCmdCopyBufferToImage>(gdpa(dev, "vkCmdCopyBufferToImage"));
    dt.vkCmdCopyImageToBuffer = reinterpret_cast<PFN_vkCmdCopyImageToBuffer>(gdpa(dev, "vkCmdCopyImageToBuffer"));
    dt.vkCmdUpdateBuffer = reinterpret_cast<PFN_vkCmdUpdateBuffer>(gdpa(dev, "vkCmdUpdateBuffer"));
    dt.vkCmdFillBuffer = reinterpret_cast<PFN_vkCmdFillBuffer>(gdpa(dev, "vkCmdFillBuffer"));
    dt.vkCmdPipelineBarrier = reinterpret_cast<PFN_vkCmdPipelineBarrier>(gdpa(dev, "vkCmdPipelineBarrier"));
    dt.vkCmdBeginQuery = reinterpret_cast<PFN_vkCmdBeginQuery>(gdpa(dev, "vkCmdBeginQuery"));
    dt.vkCmdEndQuery = reinterpret_cast<PFN_vkCmdEndQuery>(gdpa(dev, "vkCmdEndQuery"));
    dt.vkCmdResetQueryPool = reinterpret_cast<PFN_vkCmdResetQueryPool>(gdpa(dev, "vkCmdResetQueryPool"));
    dt.vkCmdWriteTimestamp = reinterpret_cast<PFN_vkCmdWriteTimestamp>(gdpa(dev, "vkCmdWriteTimestamp"));
    dt.vkCmdCopyQueryPoolResults = reinterpret_cast<PFN_vkCmdCopyQueryPoolResults>(gdpa(dev, "vkCmdCopyQueryPoolResults"));
    dt.vkCmdExecuteCommands = reinterpret_cast<PFN_vkCmdExecuteCommands>(gdpa(dev, "vkCmdExecuteCommands"));
    dt.vkCreateEvent = reinterpret_cast<PFN_vkCreateEvent>(gdpa(dev, "vkCreateEvent"));
    dt.vkDestroyEvent = reinterpret_cast<PFN_vkDestroyEvent>(gdpa(dev, "vkDestroyEvent"));
    dt.vkGetEventStatus = reinterpret_cast<PFN_vkGetEventStatus>(gdpa(dev, "vkGetEventStatus"));
    dt.vkSetEvent = reinterpret_cast<PFN_vkSetEvent>(gdpa(dev, "vkSetEvent"));
    dt.vkResetEvent = reinterpret_cast<PFN_vkResetEvent>(gdpa(dev, "vkResetEvent"));
    dt.vkCreateBufferView = reinterpret_cast<PFN_vkCreateBufferView>(gdpa(dev, "vkCreateBufferView"));
    dt.vkDestroyBufferView = reinterpret_cast<PFN_vkDestroyBufferView>(gdpa(dev, "vkDestroyBufferView"));
    dt.vkCreateShaderModule = reinterpret_cast<PFN_vkCreateShaderModule>(gdpa(dev, "vkCreateShaderModule"));
    dt.vkDestroyShaderModule = reinterpret_cast<PFN_vkDestroyShaderModule>(gdpa(dev, "vkDestroyShaderModule"));
    dt.vkCreatePipelineCache = reinterpret_cast<PFN_vkCreatePipelineCache>(gdpa(dev, "vkCreatePipelineCache"));
    dt.vkDestroyPipelineCache = reinterpret_cast<PFN_vkDestroyPipelineCache>(gdpa(dev, "vkDestroyPipelineCache"));
    dt.vkGetPipelineCacheData = reinterpret_cast<PFN_vkGetPipelineCacheData>(gdpa(dev, "vkGetPipelineCacheData"));
    dt.vkMergePipelineCaches = reinterpret_cast<PFN_vkMergePipelineCaches>(gdpa(dev, "vkMergePipelineCaches"));
    dt.vkCreateComputePipelines = reinterpret_cast<PFN_vkCreateComputePipelines>(gdpa(dev, "vkCreateComputePipelines"));
    dt.vkDestroyPipeline = reinterpret_cast<PFN_vkDestroyPipeline>(gdpa(dev, "vkDestroyPipeline"));
    dt.vkCreatePipelineLayout = reinterpret_cast<PFN_vkCreatePipelineLayout>(gdpa(dev, "vkCreatePipelineLayout"));
    dt.vkDestroyPipelineLayout = reinterpret_cast<PFN_vkDestroyPipelineLayout>(gdpa(dev, "vkDestroyPipelineLayout"));
    dt.vkCreateSampler = reinterpret_cast<PFN_vkCreateSampler>(gdpa(dev, "vkCreateSampler"));
    dt.vkDestroySampler = reinterpret_cast<PFN_vkDestroySampler>(gdpa(dev, "vkDestroySampler"));
    dt.vkCreateDescriptorSetLayout = reinterpret_cast<PFN_vkCreateDescriptorSetLayout>(gdpa(dev, "vkCreateDescriptorSetLayout"));
    dt.vkDestroyDescriptorSetLayout = reinterpret_cast<PFN_vkDestroyDescriptorSetLayout>(gdpa(dev, "vkDestroyDescriptorSetLayout"));
    dt.vkCreateDescriptorPool = reinterpret_cast<PFN_vkCreateDescriptorPool>(gdpa(dev, "vkCreateDescriptorPool"));
    dt.vkDestroyDescriptorPool = reinterpret_cast<PFN_vkDestroyDescriptorPool>(gdpa(dev, "vkDestroyDescriptorPool"));
    dt.vkResetDescriptorPool = reinterpret_cast<PFN_vkResetDescriptorPool>(gdpa(dev, "vkResetDescriptorPool"));
    dt.vkAllocateDescriptorSets = reinterpret_cast<PFN_vkAllocateDescriptorSets>(gdpa(dev, "vkAllocateDescriptorSets"));
    dt.vkFreeDescriptorSets = reinterpret_cast<PFN_vkFreeDescriptorSets>(gdpa(dev, "vkFreeDescriptorSets"));
    dt.vkUpdateDescriptorSets = reinterpret_cast<PFN_vkUpdateDescriptorSets>(gdpa(dev, "vkUpdateDescriptorSets"));
    dt.vkCmdBindPipeline = reinterpret_cast<PFN_vkCmdBindPipeline>(gdpa(dev, "vkCmdBindPipeline"));
    dt.vkCmdBindDescriptorSets = reinterpret_cast<PFN_vkCmdBindDescriptorSets>(gdpa(dev, "vkCmdBindDescriptorSets"));
    dt.vkCmdClearColorImage = reinterpret_cast<PFN_vkCmdClearColorImage>(gdpa(dev, "vkCmdClearColorImage"));
    dt.vkCmdDispatch = reinterpret_cast<PFN_vkCmdDispatch>(gdpa(dev, "vkCmdDispatch"));
    dt.vkCmdDispatchIndirect = reinterpret_cast<PFN_vkCmdDispatchIndirect>(gdpa(dev, "vkCmdDispatchIndirect"));
    dt.vkCmdSetEvent = reinterpret_cast<PFN_vkCmdSetEvent>(gdpa(dev, "vkCmdSetEvent"));
    dt.vkCmdResetEvent = reinterpret_cast<PFN_vkCmdResetEvent>(gdpa(dev, "vkCmdResetEvent"));
    dt.vkCmdWaitEvents = reinterpret_cast<PFN_vkCmdWaitEvents>(gdpa(dev, "vkCmdWaitEvents"));
    dt.vkCmdPushConstants = reinterpret_cast<PFN_vkCmdPushConstants>(gdpa(dev, "vkCmdPushConstants"));
    dt.vkCreateGraphicsPipelines = reinterpret_cast<PFN_vkCreateGraphicsPipelines>(gdpa(dev, "vkCreateGraphicsPipelines"));
    dt.vkCreateFramebuffer = reinterpret_cast<PFN_vkCreateFramebuffer>(gdpa(dev, "vkCreateFramebuffer"));
    dt.vkDestroyFramebuffer = reinterpret_cast<PFN_vkDestroyFramebuffer>(gdpa(dev, "vkDestroyFramebuffer"));
    dt.vkCreateRenderPass = reinterpret_cast<PFN_vkCreateRenderPass>(gdpa(dev, "vkCreateRenderPass"));
    dt.vkDestroyRenderPass = reinterpret_cast<PFN_vkDestroyRenderPass>(gdpa(dev, "vkDestroyRenderPass"));
    dt.vkGetRenderAreaGranularity = reinterpret_cast<PFN_vkGetRenderAreaGranularity>(gdpa(dev, "vkGetRenderAreaGranularity"));
    dt.vkCmdSetViewport = reinterpret_cast<PFN_vkCmdSetViewport>(gdpa(dev, "vkCmdSetViewport"));
    dt.vkCmdSetScissor = reinterpret_cast<PFN_vkCmdSetScissor>(gdpa(dev, "vkCmdSetScissor"));
    dt.vkCmdSetLineWidth = reinterpret_cast<PFN_vkCmdSetLineWidth>(gdpa(dev, "vkCmdSetLineWidth"));
    dt.vkCmdSetDepthBias = reinterpret_cast<PFN_vkCmdSetDepthBias>(gdpa(dev, "vkCmdSetDepthBias"));
    dt.vkCmdSetBlendConstants = reinterpret_cast<PFN_vkCmdSetBlendConstants>(gdpa(dev, "vkCmdSetBlendConstants"));
    dt.vkCmdSetDepthBounds = reinterpret_cast<PFN_vkCmdSetDepthBounds>(gdpa(dev, "vkCmdSetDepthBounds"));
    dt.vkCmdSetStencilCompareMask = reinterpret_cast<PFN_vkCmdSetStencilCompareMask>(gdpa(dev, "vkCmdSetStencilCompareMask"));
    dt.vkCmdSetStencilWriteMask = reinterpret_cast<PFN_vkCmdSetStencilWriteMask>(gdpa(dev, "vkCmdSetStencilWriteMask"));
    dt.vkCmdSetStencilReference = reinterpret_cast<PFN_vkCmdSetStencilReference>(gdpa(dev, "vkCmdSetStencilReference"));
    dt.vkCmdBindIndexBuffer = reinterpret_cast<PFN_vkCmdBindIndexBuffer>(gdpa(dev, "vkCmdBindIndexBuffer"));
    dt.vkCmdBindVertexBuffers = reinterpret_cast<PFN_vkCmdBindVertexBuffers>(gdpa(dev, "vkCmdBindVertexBuffers"));
    dt.vkCmdDraw = reinterpret_cast<PFN_vkCmdDraw>(gdpa(dev, "vkCmdDraw"));
    dt.vkCmdDrawIndexed = reinterpret_cast<PFN_vkCmdDrawIndexed>(gdpa(dev, "vkCmdDrawIndexed"));
    dt.vkCmdDrawIndirect = reinterpret_cast<PFN_vkCmdDrawIndirect>(gdpa(dev, "vkCmdDrawIndirect"));
    dt.vkCmdDrawIndexedIndirect = reinterpret_cast<PFN_vkCmdDrawIndexedIndirect>(gdpa(dev, "vkCmdDrawIndexedIndirect"));
    dt.vkCmdBlitImage = reinterpret_cast<PFN_vkCmdBlitImage>(gdpa(dev, "vkCmdBlitImage"));
    dt.vkCmdClearDepthStencilImage = reinterpret_cast<PFN_vkCmdClearDepthStencilImage>(gdpa(dev, "vkCmdClearDepthStencilImage"));
    dt.vkCmdClearAttachments = reinterpret_cast<PFN_vkCmdClearAttachments>(gdpa(dev, "vkCmdClearAttachments"));
    dt.vkCmdResolveImage = reinterpret_cast<PFN_vkCmdResolveImage>(gdpa(dev, "vkCmdResolveImage"));
    dt.vkCmdBeginRenderPass = reinterpret_cast<PFN_vkCmdBeginRenderPass>(gdpa(dev, "vkCmdBeginRenderPass"));
    dt.vkCmdNextSubpass = reinterpret_cast<PFN_vkCmdNextSubpass>(gdpa(dev, "vkCmdNextSubpass"));
    dt.vkCmdEndRenderPass = reinterpret_cast<PFN_vkCmdEndRenderPass>(gdpa(dev, "vkCmdEndRenderPass"));
    dt.vkBindBufferMemory2 = reinterpret_cast<PFN_vkBindBufferMemory2>(gdpa(dev, "vkBindBufferMemory2"));
    dt.vkBindImageMemory2 = reinterpret_cast<PFN_vkBindImageMemory2>(gdpa(dev, "vkBindImageMemory2"));
    dt.vkGetDeviceGroupPeerMemoryFeatures = reinterpret_cast<PFN_vkGetDeviceGroupPeerMemoryFeatures>(gdpa(dev, "vkGetDeviceGroupPeerMemoryFeatures"));
    dt.vkCmdSetDeviceMask = reinterpret_cast<PFN_vkCmdSetDeviceMask>(gdpa(dev, "vkCmdSetDeviceMask"));
    dt.vkGetImageMemoryRequirements2 = reinterpret_cast<PFN_vkGetImageMemoryRequirements2>(gdpa(dev, "vkGetImageMemoryRequirements2"));
    dt.vkGetBufferMemoryRequirements2 = reinterpret_cast<PFN_vkGetBufferMemoryRequirements2>(gdpa(dev, "vkGetBufferMemoryRequirements2"));
    dt.vkGetImageSparseMemoryRequirements2 = reinterpret_cast<PFN_vkGetImageSparseMemoryRequirements2>(gdpa(dev, "vkGetImageSparseMemoryRequirements2"));
    dt.vkTrimCommandPool = reinterpret_cast<PFN_vkTrimCommandPool>(gdpa(dev, "vkTrimCommandPool"));
    dt.vkCmdDispatchBase = reinterpret_cast<PFN_vkCmdDispatchBase>(gdpa(dev, "vkCmdDispatchBase"));
    dt.vkCreateDescriptorUpdateTemplate = reinterpret_cast<PFN_vkCreateDescriptorUpdateTemplate>(gdpa(dev, "vkCreateDescriptorUpdateTemplate"));
    dt.vkDestroyDescriptorUpdateTemplate = reinterpret_cast<PFN_vkDestroyDescriptorUpdateTemplate>(gdpa(dev, "vkDestroyDescriptorUpdateTemplate"));
    dt.vkUpdateDescriptorSetWithTemplate = reinterpret_cast<PFN_vkUpdateDescriptorSetWithTemplate>(gdpa(dev, "vkUpdateDescriptorSetWithTemplate"));
    dt.vkGetDescriptorSetLayoutSupport = reinterpret_cast<PFN_vkGetDescriptorSetLayoutSupport>(gdpa(dev, "vkGetDescriptorSetLayoutSupport"));
    dt.vkCreateSamplerYcbcrConversion = reinterpret_cast<PFN_vkCreateSamplerYcbcrConversion>(gdpa(dev, "vkCreateSamplerYcbcrConversion"));
    dt.vkDestroySamplerYcbcrConversion = reinterpret_cast<PFN_vkDestroySamplerYcbcrConversion>(gdpa(dev, "vkDestroySamplerYcbcrConversion"));
    dt.vkResetQueryPool = reinterpret_cast<PFN_vkResetQueryPool>(gdpa(dev, "vkResetQueryPool"));
    dt.vkGetSemaphoreCounterValue = reinterpret_cast<PFN_vkGetSemaphoreCounterValue>(gdpa(dev, "vkGetSemaphoreCounterValue"));
    dt.vkWaitSemaphores = reinterpret_cast<PFN_vkWaitSemaphores>(gdpa(dev, "vkWaitSemaphores"));
    dt.vkSignalSemaphore = reinterpret_cast<PFN_vkSignalSemaphore>(gdpa(dev, "vkSignalSemaphore"));
    dt.vkGetBufferDeviceAddress = reinterpret_cast<PFN_vkGetBufferDeviceAddress>(gdpa(dev, "vkGetBufferDeviceAddress"));
    dt.vkGetBufferOpaqueCaptureAddress = reinterpret_cast<PFN_vkGetBufferOpaqueCaptureAddress>(gdpa(dev, "vkGetBufferOpaqueCaptureAddress"));
    dt.vkGetDeviceMemoryOpaqueCaptureAddress = reinterpret_cast<PFN_vkGetDeviceMemoryOpaqueCaptureAddress>(gdpa(dev, "vkGetDeviceMemoryOpaqueCaptureAddress"));
    dt.vkCmdDrawIndirectCount = reinterpret_cast<PFN_vkCmdDrawIndirectCount>(gdpa(dev, "vkCmdDrawIndirectCount"));
    dt.vkCmdDrawIndexedIndirectCount = reinterpret_cast<PFN_vkCmdDrawIndexedIndirectCount>(gdpa(dev, "vkCmdDrawIndexedIndirectCount"));
    dt.vkCreateRenderPass2 = reinterpret_cast<PFN_vkCreateRenderPass2>(gdpa(dev, "vkCreateRenderPass2"));
    dt.vkCmdBeginRenderPass2 = reinterpret_cast<PFN_vkCmdBeginRenderPass2>(gdpa(dev, "vkCmdBeginRenderPass2"));
    dt.vkCmdNextSubpass2 = reinterpret_cast<PFN_vkCmdNextSubpass2>(gdpa(dev, "vkCmdNextSubpass2"));
    dt.vkCmdEndRenderPass2 = reinterpret_cast<PFN_vkCmdEndRenderPass2>(gdpa(dev, "vkCmdEndRenderPass2"));
    dt.vkCreatePrivateDataSlot = reinterpret_cast<PFN_vkCreatePrivateDataSlot>(gdpa(dev, "vkCreatePrivateDataSlot"));
    dt.vkDestroyPrivateDataSlot = reinterpret_cast<PFN_vkDestroyPrivateDataSlot>(gdpa(dev, "vkDestroyPrivateDataSlot"));
    dt.vkSetPrivateData = reinterpret_cast<PFN_vkSetPrivateData>(gdpa(dev, "vkSetPrivateData"));
    dt.vkGetPrivateData = reinterpret_cast<PFN_vkGetPrivateData>(gdpa(dev, "vkGetPrivateData"));
    dt.vkCmdPipelineBarrier2 = reinterpret_cast<PFN_vkCmdPipelineBarrier2>(gdpa(dev, "vkCmdPipelineBarrier2"));
    dt.vkCmdWriteTimestamp2 = reinterpret_cast<PFN_vkCmdWriteTimestamp2>(gdpa(dev, "vkCmdWriteTimestamp2"));
    dt.vkQueueSubmit2 = reinterpret_cast<PFN_vkQueueSubmit2>(gdpa(dev, "vkQueueSubmit2"));
    dt.vkCmdCopyBuffer2 = reinterpret_cast<PFN_vkCmdCopyBuffer2>(gdpa(dev, "vkCmdCopyBuffer2"));
    dt.vkCmdCopyImage2 = reinterpret_cast<PFN_vkCmdCopyImage2>(gdpa(dev, "vkCmdCopyImage2"));
    dt.vkCmdCopyBufferToImage2 = reinterpret_cast<PFN_vkCmdCopyBufferToImage2>(gdpa(dev, "vkCmdCopyBufferToImage2"));
    dt.vkCmdCopyImageToBuffer2 = reinterpret_cast<PFN_vkCmdCopyImageToBuffer2>(gdpa(dev, "vkCmdCopyImageToBuffer2"));
    dt.vkGetDeviceBufferMemoryRequirements = reinterpret_cast<PFN_vkGetDeviceBufferMemoryRequirements>(gdpa(dev, "vkGetDeviceBufferMemoryRequirements"));
    dt.vkGetDeviceImageMemoryRequirements = reinterpret_cast<PFN_vkGetDeviceImageMemoryRequirements>(gdpa(dev, "vkGetDeviceImageMemoryRequirements"));
    dt.vkGetDeviceImageSparseMemoryRequirements = reinterpret_cast<PFN_vkGetDeviceImageSparseMemoryRequirements>(gdpa(dev, "vkGetDeviceImageSparseMemoryRequirements"));
    dt.vkCmdSetEvent2 = reinterpret_cast<PFN_vkCmdSetEvent2>(gdpa(dev, "vkCmdSetEvent2"));
    dt.vkCmdResetEvent2 = reinterpret_cast<PFN_vkCmdResetEvent2>(gdpa(dev, "vkCmdResetEvent2"));
    dt.vkCmdWaitEvents2 = reinterpret_cast<PFN_vkCmdWaitEvents2>(gdpa(dev, "vkCmdWaitEvents2"));
    dt.vkCmdBlitImage2 = reinterpret_cast<PFN_vkCmdBlitImage2>(gdpa(dev, "vkCmdBlitImage2"));
    dt.vkCmdResolveImage2 = reinterpret_cast<PFN_vkCmdResolveImage2>(gdpa(dev, "vkCmdResolveImage2"));
    dt.vkCmdBeginRendering = reinterpret_cast<PFN_vkCmdBeginRendering>(gdpa(dev, "vkCmdBeginRendering"));
    dt.vkCmdEndRendering = reinterpret_cast<PFN_vkCmdEndRendering>(gdpa(dev, "vkCmdEndRendering"));
    dt.vkCmdSetCullMode = reinterpret_cast<PFN_vkCmdSetCullMode>(gdpa(dev, "vkCmdSetCullMode"));
    dt.vkCmdSetFrontFace = reinterpret_cast<PFN_vkCmdSetFrontFace>(gdpa(dev, "vkCmdSetFrontFace"));
    dt.vkCmdSetPrimitiveTopology = reinterpret_cast<PFN_vkCmdSetPrimitiveTopology>(gdpa(dev, "vkCmdSetPrimitiveTopology"));
    dt.vkCmdSetViewportWithCount = reinterpret_cast<PFN_vkCmdSetViewportWithCount>(gdpa(dev, "vkCmdSetViewportWithCount"));
    dt.vkCmdSetScissorWithCount = reinterpret_cast<PFN_vkCmdSetScissorWithCount>(gdpa(dev, "vkCmdSetScissorWithCount"));
    dt.vkCmdBindVertexBuffers2 = reinterpret_cast<PFN_vkCmdBindVertexBuffers2>(gdpa(dev, "vkCmdBindVertexBuffers2"));
    dt.vkCmdSetDepthTestEnable = reinterpret_cast<PFN_vkCmdSetDepthTestEnable>(gdpa(dev, "vkCmdSetDepthTestEnable"));
    dt.vkCmdSetDepthWriteEnable = reinterpret_cast<PFN_vkCmdSetDepthWriteEnable>(gdpa(dev, "vkCmdSetDepthWriteEnable"));
    dt.vkCmdSetDepthCompareOp = reinterpret_cast<PFN_vkCmdSetDepthCompareOp>(gdpa(dev, "vkCmdSetDepthCompareOp"));
    dt.vkCmdSetDepthBoundsTestEnable = reinterpret_cast<PFN_vkCmdSetDepthBoundsTestEnable>(gdpa(dev, "vkCmdSetDepthBoundsTestEnable"));
    dt.vkCmdSetStencilTestEnable = reinterpret_cast<PFN_vkCmdSetStencilTestEnable>(gdpa(dev, "vkCmdSetStencilTestEnable"));
    dt.vkCmdSetStencilOp = reinterpret_cast<PFN_vkCmdSetStencilOp>(gdpa(dev, "vkCmdSetStencilOp"));
    dt.vkCmdSetRasterizerDiscardEnable = reinterpret_cast<PFN_vkCmdSetRasterizerDiscardEnable>(gdpa(dev, "vkCmdSetRasterizerDiscardEnable"));
    dt.vkCmdSetDepthBiasEnable = reinterpret_cast<PFN_vkCmdSetDepthBiasEnable>(gdpa(dev, "vkCmdSetDepthBiasEnable"));
    dt.vkCmdSetPrimitiveRestartEnable = reinterpret_cast<PFN_vkCmdSetPrimitiveRestartEnable>(gdpa(dev, "vkCmdSetPrimitiveRestartEnable"));
    dt.vkCmdBeginRenderingKHR = reinterpret_cast<PFN_vkCmdBeginRenderingKHR>(gdpa(dev, "vkCmdBeginRenderingKHR"));
    dt.vkCmdEndRenderingKHR = reinterpret_cast<PFN_vkCmdEndRenderingKHR>(gdpa(dev, "vkCmdEndRenderingKHR"));
    dt.vkTrimCommandPoolKHR = reinterpret_cast<PFN_vkTrimCommandPoolKHR>(gdpa(dev, "vkTrimCommandPoolKHR"));
    dt.vkCmdPushDescriptorSetKHR = reinterpret_cast<PFN_vkCmdPushDescriptorSetKHR>(gdpa(dev, "vkCmdPushDescriptorSetKHR"));
    dt.vkCmdPushDescriptorSetWithTemplateKHR = reinterpret_cast<PFN_vkCmdPushDescriptorSetWithTemplateKHR>(gdpa(dev, "vkCmdPushDescriptorSetWithTemplateKHR"));
    dt.vkCreateDescriptorUpdateTemplateKHR = reinterpret_cast<PFN_vkCreateDescriptorUpdateTemplateKHR>(gdpa(dev, "vkCreateDescriptorUpdateTemplateKHR"));
    dt.vkDestroyDescriptorUpdateTemplateKHR = reinterpret_cast<PFN_vkDestroyDescriptorUpdateTemplateKHR>(gdpa(dev, "vkDestroyDescriptorUpdateTemplateKHR"));
    dt.vkUpdateDescriptorSetWithTemplateKHR = reinterpret_cast<PFN_vkUpdateDescriptorSetWithTemplateKHR>(gdpa(dev, "vkUpdateDescriptorSetWithTemplateKHR"));
    dt.vkCreateRenderPass2KHR = reinterpret_cast<PFN_vkCreateRenderPass2KHR>(gdpa(dev, "vkCreateRenderPass2KHR"));
    dt.vkCmdBeginRenderPass2KHR = reinterpret_cast<PFN_vkCmdBeginRenderPass2KHR>(gdpa(dev, "vkCmdBeginRenderPass2KHR"));
    dt.vkCmdNextSubpass2KHR = reinterpret_cast<PFN_vkCmdNextSubpass2KHR>(gdpa(dev, "vkCmdNextSubpass2KHR"));
    dt.vkCmdEndRenderPass2KHR = reinterpret_cast<PFN_vkCmdEndRenderPass2KHR>(gdpa(dev, "vkCmdEndRenderPass2KHR"));
    dt.vkGetImageMemoryRequirements2KHR = reinterpret_cast<PFN_vkGetImageMemoryRequirements2KHR>(gdpa(dev, "vkGetImageMemoryRequirements2KHR"));
    dt.vkGetBufferMemoryRequirements2KHR = reinterpret_cast<PFN_vkGetBufferMemoryRequirements2KHR>(gdpa(dev, "vkGetBufferMemoryRequirements2KHR"));
    dt.vkGetImageSparseMemoryRequirements2KHR = reinterpret_cast<PFN_vkGetImageSparseMemoryRequirements2KHR>(gdpa(dev, "vkGetImageSparseMemoryRequirements2KHR"));
    dt.vkCreateSamplerYcbcrConversionKHR = reinterpret_cast<PFN_vkCreateSamplerYcbcrConversionKHR>(gdpa(dev, "vkCreateSamplerYcbcrConversionKHR"));
    dt.vkDestroySamplerYcbcrConversionKHR = reinterpret_cast<PFN_vkDestroySamplerYcbcrConversionKHR>(gdpa(dev, "vkDestroySamplerYcbcrConversionKHR"));
    dt.vkBindBufferMemory2KHR = reinterpret_cast<PFN_vkBindBufferMemory2KHR>(gdpa(dev, "vkBindBufferMemory2KHR"));
    dt.vkBindImageMemory2KHR = reinterpret_cast<PFN_vkBindImageMemory2KHR>(gdpa(dev, "vkBindImageMemory2KHR"));
    dt.vkGetDescriptorSetLayoutSupportKHR = reinterpret_cast<PFN_vkGetDescriptorSetLayoutSupportKHR>(gdpa(dev, "vkGetDescriptorSetLayoutSupportKHR"));
    dt.vkCmdDrawIndirectCountKHR = reinterpret_cast<PFN_vkCmdDrawIndirectCountKHR>(gdpa(dev, "vkCmdDrawIndirectCountKHR"));
    dt.vkCmdDrawIndexedIndirectCountKHR = reinterpret_cast<PFN_vkCmdDrawIndexedIndirectCountKHR>(gdpa(dev, "vkCmdDrawIndexedIndirectCountKHR"));
    dt.vkGetSemaphoreCounterValueKHR = reinterpret_cast<PFN_vkGetSemaphoreCounterValueKHR>(gdpa(dev, "vkGetSemaphoreCounterValueKHR"));
    dt.vkWaitSemaphoresKHR = reinterpret_cast<PFN_vkWaitSemaphoresKHR>(gdpa(dev, "vkWaitSemaphoresKHR"));
    dt.vkSignalSemaphoreKHR = reinterpret_cast<PFN_vkSignalSemaphoreKHR>(gdpa(dev, "vkSignalSemaphoreKHR"));
    dt.vkGetBufferDeviceAddressKHR = reinterpret_cast<PFN_vkGetBufferDeviceAddressKHR>(gdpa(dev, "vkGetBufferDeviceAddressKHR"));
    dt.vkGetBufferOpaqueCaptureAddressKHR = reinterpret_cast<PFN_vkGetBufferOpaqueCaptureAddressKHR>(gdpa(dev, "vkGetBufferOpaqueCaptureAddressKHR"));
    dt.vkGetDeviceMemoryOpaqueCaptureAddressKHR = reinterpret_cast<PFN_vkGetDeviceMemoryOpaqueCaptureAddressKHR>(gdpa(dev, "vkGetDeviceMemoryOpaqueCaptureAddressKHR"));
    dt.vkCmdSetEvent2KHR = reinterpret_cast<PFN_vkCmdSetEvent2KHR>(gdpa(dev, "vkCmdSetEvent2KHR"));
    dt.vkCmdResetEvent2KHR = reinterpret_cast<PFN_vkCmdResetEvent2KHR>(gdpa(dev, "vkCmdResetEvent2KHR"));
    dt.vkCmdWaitEvents2KHR = reinterpret_cast<PFN_vkCmdWaitEvents2KHR>(gdpa(dev, "vkCmdWaitEvents2KHR"));
    dt.vkCmdPipelineBarrier2KHR = reinterpret_cast<PFN_vkCmdPipelineBarrier2KHR>(gdpa(dev, "vkCmdPipelineBarrier2KHR"));
    dt.vkCmdWriteTimestamp2KHR = reinterpret_cast<PFN_vkCmdWriteTimestamp2KHR>(gdpa(dev, "vkCmdWriteTimestamp2KHR"));
    dt.vkQueueSubmit2KHR = reinterpret_cast<PFN_vkQueueSubmit2KHR>(gdpa(dev, "vkQueueSubmit2KHR"));
    dt.vkCmdCopyBuffer2KHR = reinterpret_cast<PFN_vkCmdCopyBuffer2KHR>(gdpa(dev, "vkCmdCopyBuffer2KHR"));
    dt.vkCmdCopyImage2KHR = reinterpret_cast<PFN_vkCmdCopyImage2KHR>(gdpa(dev, "vkCmdCopyImage2KHR"));
    dt.vkCmdCopyBufferToImage2KHR = reinterpret_cast<PFN_vkCmdCopyBufferToImage2KHR>(gdpa(dev, "vkCmdCopyBufferToImage2KHR"));
    dt.vkCmdCopyImageToBuffer2KHR = reinterpret_cast<PFN_vkCmdCopyImageToBuffer2KHR>(gdpa(dev, "vkCmdCopyImageToBuffer2KHR"));
    dt.vkCmdBlitImage2KHR = reinterpret_cast<PFN_vkCmdBlitImage2KHR>(gdpa(dev, "vkCmdBlitImage2KHR"));
    dt.vkCmdResolveImage2KHR = reinterpret_cast<PFN_vkCmdResolveImage2KHR>(gdpa(dev, "vkCmdResolveImage2KHR"));
    dt.vkGetDeviceBufferMemoryRequirementsKHR = reinterpret_cast<PFN_vkGetDeviceBufferMemoryRequirementsKHR>(gdpa(dev, "vkGetDeviceBufferMemoryRequirementsKHR"));
    dt.vkGetDeviceImageMemoryRequirementsKHR = reinterpret_cast<PFN_vkGetDeviceImageMemoryRequirementsKHR>(gdpa(dev, "vkGetDeviceImageMemoryRequirementsKHR"));
    dt.vkGetDeviceImageSparseMemoryRequirementsKHR = reinterpret_cast<PFN_vkGetDeviceImageSparseMemoryRequirementsKHR>(gdpa(dev, "vkGetDeviceImageSparseMemoryRequirementsKHR"));
    dt.vkCmdSetCullModeEXT = reinterpret_cast<PFN_vkCmdSetCullModeEXT>(gdpa(dev, "vkCmdSetCullModeEXT"));
    dt.vkCmdSetFrontFaceEXT = reinterpret_cast<PFN_vkCmdSetFrontFaceEXT>(gdpa(dev, "vkCmdSetFrontFaceEXT"));
    dt.vkCmdSetPrimitiveTopologyEXT = reinterpret_cast<PFN_vkCmdSetPrimitiveTopologyEXT>(gdpa(dev, "vkCmdSetPrimitiveTopologyEXT"));
    dt.vkCmdSetViewportWithCountEXT = reinterpret_cast<PFN_vkCmdSetViewportWithCountEXT>(gdpa(dev, "vkCmdSetViewportWithCountEXT"));
    dt.vkCmdSetScissorWithCountEXT = reinterpret_cast<PFN_vkCmdSetScissorWithCountEXT>(gdpa(dev, "vkCmdSetScissorWithCountEXT"));
    dt.vkCmdBindVertexBuffers2EXT = reinterpret_cast<PFN_vkCmdBindVertexBuffers2EXT>(gdpa(dev, "vkCmdBindVertexBuffers2EXT"));
    dt.vkCmdSetDepthTestEnableEXT = reinterpret_cast<PFN_vkCmdSetDepthTestEnableEXT>(gdpa(dev, "vkCmdSetDepthTestEnableEXT"));
    dt.vkCmdSetDepthWriteEnableEXT = reinterpret_cast<PFN_vkCmdSetDepthWriteEnableEXT>(gdpa(dev, "vkCmdSetDepthWriteEnableEXT"));
    dt.vkCmdSetDepthCompareOpEXT = reinterpret_cast<PFN_vkCmdSetDepthCompareOpEXT>(gdpa(dev, "vkCmdSetDepthCompareOpEXT"));
    dt.vkCmdSetDepthBoundsTestEnableEXT = reinterpret_cast<PFN_vkCmdSetDepthBoundsTestEnableEXT>(gdpa(dev, "vkCmdSetDepthBoundsTestEnableEXT"));
    dt.vkCmdSetStencilTestEnableEXT = reinterpret_cast<PFN_vkCmdSetStencilTestEnableEXT>(gdpa(dev, "vkCmdSetStencilTestEnableEXT"));
    dt.vkCmdSetStencilOpEXT = reinterpret_cast<PFN_vkCmdSetStencilOpEXT>(gdpa(dev, "vkCmdSetStencilOpEXT"));
    dt.vkCmdSetPatchControlPointsEXT = reinterpret_cast<PFN_vkCmdSetPatchControlPointsEXT>(gdpa(dev, "vkCmdSetPatchControlPointsEXT"));
    dt.vkCmdSetRasterizerDiscardEnableEXT = reinterpret_cast<PFN_vkCmdSetRasterizerDiscardEnableEXT>(gdpa(dev, "vkCmdSetRasterizerDiscardEnableEXT"));
    dt.vkCmdSetDepthBiasEnableEXT = reinterpret_cast<PFN_vkCmdSetDepthBiasEnableEXT>(gdpa(dev, "vkCmdSetDepthBiasEnableEXT"));
    dt.vkCmdSetLogicOpEXT = reinterpret_cast<PFN_vkCmdSetLogicOpEXT>(gdpa(dev, "vkCmdSetLogicOpEXT"));
    dt.vkCmdSetPrimitiveRestartEnableEXT = reinterpret_cast<PFN_vkCmdSetPrimitiveRestartEnableEXT>(gdpa(dev, "vkCmdSetPrimitiveRestartEnableEXT"));
}
#endif // GUEST_VULKAN_STAGE_TABLE

#ifdef GUEST_VULKAN_STAGE_IMPL
VKAPI_ATTR void VKAPI_CALL V_vkGetPhysicalDeviceFeatures(VkPhysicalDevice physicalDevice, VkPhysicalDeviceFeatures* pFeatures) {
    static_assert(std::is_same_v<PFN_vkGetPhysicalDeviceFeatures, decltype(&V_vkGetPhysicalDeviceFeatures)>);
    InstanceRecord* rec = LookupPhysicalDeviceInstance(physicalDevice);
    if (!rec) return;
    auto fn = reinterpret_cast<PFN_vkGetPhysicalDeviceFeatures>(rec->gipa(rec->hostInstance, "vkGetPhysicalDeviceFeatures"));
    if (fn) fn(physicalDevice, pFeatures);
}
VKAPI_ATTR void VKAPI_CALL V_vkGetPhysicalDeviceFormatProperties(VkPhysicalDevice physicalDevice, VkFormat format, VkFormatProperties* pFormatProperties) {
    static_assert(std::is_same_v<PFN_vkGetPhysicalDeviceFormatProperties, decltype(&V_vkGetPhysicalDeviceFormatProperties)>);
    InstanceRecord* rec = LookupPhysicalDeviceInstance(physicalDevice);
    if (!rec) return;
    auto fn = reinterpret_cast<PFN_vkGetPhysicalDeviceFormatProperties>(rec->gipa(rec->hostInstance, "vkGetPhysicalDeviceFormatProperties"));
    if (fn) fn(physicalDevice, format, pFormatProperties);
}
VKAPI_ATTR VkResult VKAPI_CALL V_vkGetPhysicalDeviceImageFormatProperties(VkPhysicalDevice physicalDevice, VkFormat format, VkImageType type, VkImageTiling tiling, VkImageUsageFlags usage, VkImageCreateFlags flags, VkImageFormatProperties* pImageFormatProperties) {
    static_assert(std::is_same_v<PFN_vkGetPhysicalDeviceImageFormatProperties, decltype(&V_vkGetPhysicalDeviceImageFormatProperties)>);
    InstanceRecord* rec = LookupPhysicalDeviceInstance(physicalDevice);
    if (!rec) return VK_ERROR_INITIALIZATION_FAILED;
    auto fn = reinterpret_cast<PFN_vkGetPhysicalDeviceImageFormatProperties>(rec->gipa(rec->hostInstance, "vkGetPhysicalDeviceImageFormatProperties"));
    return fn ? fn(physicalDevice, format, type, tiling, usage, flags, pImageFormatProperties) : VK_ERROR_EXTENSION_NOT_PRESENT;
}
VKAPI_ATTR void VKAPI_CALL V_vkGetPhysicalDeviceProperties(VkPhysicalDevice physicalDevice, VkPhysicalDeviceProperties* pProperties) {
    static_assert(std::is_same_v<PFN_vkGetPhysicalDeviceProperties, decltype(&V_vkGetPhysicalDeviceProperties)>);
    InstanceRecord* rec = LookupPhysicalDeviceInstance(physicalDevice);
    if (!rec) return;
    auto fn = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(rec->gipa(rec->hostInstance, "vkGetPhysicalDeviceProperties"));
    if (fn) fn(physicalDevice, pProperties);
}
VKAPI_ATTR void VKAPI_CALL V_vkGetPhysicalDeviceQueueFamilyProperties(VkPhysicalDevice physicalDevice, uint32_t* pQueueFamilyPropertyCount, VkQueueFamilyProperties* pQueueFamilyProperties) {
    static_assert(std::is_same_v<PFN_vkGetPhysicalDeviceQueueFamilyProperties, decltype(&V_vkGetPhysicalDeviceQueueFamilyProperties)>);
    InstanceRecord* rec = LookupPhysicalDeviceInstance(physicalDevice);
    if (!rec) return;
    auto fn = reinterpret_cast<PFN_vkGetPhysicalDeviceQueueFamilyProperties>(rec->gipa(rec->hostInstance, "vkGetPhysicalDeviceQueueFamilyProperties"));
    if (fn) fn(physicalDevice, pQueueFamilyPropertyCount, pQueueFamilyProperties);
}
VKAPI_ATTR void VKAPI_CALL V_vkGetPhysicalDeviceMemoryProperties(VkPhysicalDevice physicalDevice, VkPhysicalDeviceMemoryProperties* pMemoryProperties) {
    static_assert(std::is_same_v<PFN_vkGetPhysicalDeviceMemoryProperties, decltype(&V_vkGetPhysicalDeviceMemoryProperties)>);
    InstanceRecord* rec = LookupPhysicalDeviceInstance(physicalDevice);
    if (!rec) return;
    auto fn = reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties>(rec->gipa(rec->hostInstance, "vkGetPhysicalDeviceMemoryProperties"));
    if (fn) fn(physicalDevice, pMemoryProperties);
}
VKAPI_ATTR VkResult VKAPI_CALL V_vkQueueSubmit(VkQueue queue, uint32_t submitCount, const VkSubmitInfo* pSubmits, VkFence fence) {
    static_assert(std::is_same_v<PFN_vkQueueSubmit, decltype(&V_vkQueueSubmit)>);
    DeviceRecord* rec = LookupQueueDevice(queue);
    if (!rec || !rec->dt.vkQueueSubmit) return VK_ERROR_EXTENSION_NOT_PRESENT;
    return rec->dt.vkQueueSubmit(queue, submitCount, pSubmits, fence);
}
VKAPI_ATTR VkResult VKAPI_CALL V_vkQueueWaitIdle(VkQueue queue) {
    static_assert(std::is_same_v<PFN_vkQueueWaitIdle, decltype(&V_vkQueueWaitIdle)>);
    DeviceRecord* rec = LookupQueueDevice(queue);
    if (!rec || !rec->dt.vkQueueWaitIdle) return VK_ERROR_EXTENSION_NOT_PRESENT;
    return rec->dt.vkQueueWaitIdle(queue);
}
VKAPI_ATTR VkResult VKAPI_CALL V_vkDeviceWaitIdle(VkDevice device) {
    static_assert(std::is_same_v<PFN_vkDeviceWaitIdle, decltype(&V_vkDeviceWaitIdle)>);
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (!rec || !rec->dt.vkDeviceWaitIdle) return VK_ERROR_EXTENSION_NOT_PRESENT;
    return rec->dt.vkDeviceWaitIdle(device);
}
VKAPI_ATTR VkResult VKAPI_CALL V_vkAllocateMemory(VkDevice device, const VkMemoryAllocateInfo* pAllocateInfo, const VkAllocationCallbacks* pAllocator, VkDeviceMemory* pMemory) {
    static_assert(std::is_same_v<PFN_vkAllocateMemory, decltype(&V_vkAllocateMemory)>);
    if (pAllocateInfo) { VkResult pnextErr = ValidateGuestPNext(pAllocateInfo->pNext); if (pnextErr != VK_SUCCESS) return pnextErr; }
    bool allocOk = true;
    const VkAllocationCallbacks* hostAlloc = InternGuestAllocator(pAllocator, allocOk);
    if (!allocOk) return VK_ERROR_FEATURE_NOT_PRESENT;
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (!rec || !rec->dt.vkAllocateMemory) return VK_ERROR_EXTENSION_NOT_PRESENT;
    return rec->dt.vkAllocateMemory(device, pAllocateInfo, hostAlloc, pMemory);
}
VKAPI_ATTR void VKAPI_CALL V_vkFreeMemory(VkDevice device, VkDeviceMemory memory, const VkAllocationCallbacks* pAllocator) {
    static_assert(std::is_same_v<PFN_vkFreeMemory, decltype(&V_vkFreeMemory)>);
    bool allocOk = true;
    const VkAllocationCallbacks* hostAlloc = InternGuestAllocator(pAllocator, allocOk);
    if (!allocOk) return;
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (rec && rec->dt.vkFreeMemory) rec->dt.vkFreeMemory(device, memory, hostAlloc);
}
VKAPI_ATTR VkResult VKAPI_CALL V_vkMapMemory(VkDevice device, VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize size, VkMemoryMapFlags flags, void** ppData) {
    static_assert(std::is_same_v<PFN_vkMapMemory, decltype(&V_vkMapMemory)>);
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (!rec || !rec->dt.vkMapMemory) return VK_ERROR_EXTENSION_NOT_PRESENT;
    return rec->dt.vkMapMemory(device, memory, offset, size, flags, ppData);
}
VKAPI_ATTR void VKAPI_CALL V_vkUnmapMemory(VkDevice device, VkDeviceMemory memory) {
    static_assert(std::is_same_v<PFN_vkUnmapMemory, decltype(&V_vkUnmapMemory)>);
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (rec && rec->dt.vkUnmapMemory) rec->dt.vkUnmapMemory(device, memory);
}
VKAPI_ATTR VkResult VKAPI_CALL V_vkFlushMappedMemoryRanges(VkDevice device, uint32_t memoryRangeCount, const VkMappedMemoryRange* pMemoryRanges) {
    static_assert(std::is_same_v<PFN_vkFlushMappedMemoryRanges, decltype(&V_vkFlushMappedMemoryRanges)>);
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (!rec || !rec->dt.vkFlushMappedMemoryRanges) return VK_ERROR_EXTENSION_NOT_PRESENT;
    return rec->dt.vkFlushMappedMemoryRanges(device, memoryRangeCount, pMemoryRanges);
}
VKAPI_ATTR VkResult VKAPI_CALL V_vkInvalidateMappedMemoryRanges(VkDevice device, uint32_t memoryRangeCount, const VkMappedMemoryRange* pMemoryRanges) {
    static_assert(std::is_same_v<PFN_vkInvalidateMappedMemoryRanges, decltype(&V_vkInvalidateMappedMemoryRanges)>);
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (!rec || !rec->dt.vkInvalidateMappedMemoryRanges) return VK_ERROR_EXTENSION_NOT_PRESENT;
    return rec->dt.vkInvalidateMappedMemoryRanges(device, memoryRangeCount, pMemoryRanges);
}
VKAPI_ATTR void VKAPI_CALL V_vkGetDeviceMemoryCommitment(VkDevice device, VkDeviceMemory memory, VkDeviceSize* pCommittedMemoryInBytes) {
    static_assert(std::is_same_v<PFN_vkGetDeviceMemoryCommitment, decltype(&V_vkGetDeviceMemoryCommitment)>);
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (rec && rec->dt.vkGetDeviceMemoryCommitment) rec->dt.vkGetDeviceMemoryCommitment(device, memory, pCommittedMemoryInBytes);
}
VKAPI_ATTR VkResult VKAPI_CALL V_vkBindBufferMemory(VkDevice device, VkBuffer buffer, VkDeviceMemory memory, VkDeviceSize memoryOffset) {
    static_assert(std::is_same_v<PFN_vkBindBufferMemory, decltype(&V_vkBindBufferMemory)>);
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (!rec || !rec->dt.vkBindBufferMemory) return VK_ERROR_EXTENSION_NOT_PRESENT;
    return rec->dt.vkBindBufferMemory(device, buffer, memory, memoryOffset);
}
VKAPI_ATTR VkResult VKAPI_CALL V_vkBindImageMemory(VkDevice device, VkImage image, VkDeviceMemory memory, VkDeviceSize memoryOffset) {
    static_assert(std::is_same_v<PFN_vkBindImageMemory, decltype(&V_vkBindImageMemory)>);
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (!rec || !rec->dt.vkBindImageMemory) return VK_ERROR_EXTENSION_NOT_PRESENT;
    return rec->dt.vkBindImageMemory(device, image, memory, memoryOffset);
}
VKAPI_ATTR void VKAPI_CALL V_vkGetBufferMemoryRequirements(VkDevice device, VkBuffer buffer, VkMemoryRequirements* pMemoryRequirements) {
    static_assert(std::is_same_v<PFN_vkGetBufferMemoryRequirements, decltype(&V_vkGetBufferMemoryRequirements)>);
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (rec && rec->dt.vkGetBufferMemoryRequirements) rec->dt.vkGetBufferMemoryRequirements(device, buffer, pMemoryRequirements);
}
VKAPI_ATTR void VKAPI_CALL V_vkGetImageMemoryRequirements(VkDevice device, VkImage image, VkMemoryRequirements* pMemoryRequirements) {
    static_assert(std::is_same_v<PFN_vkGetImageMemoryRequirements, decltype(&V_vkGetImageMemoryRequirements)>);
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (rec && rec->dt.vkGetImageMemoryRequirements) rec->dt.vkGetImageMemoryRequirements(device, image, pMemoryRequirements);
}
VKAPI_ATTR void VKAPI_CALL V_vkGetImageSparseMemoryRequirements(VkDevice device, VkImage image, uint32_t* pSparseMemoryRequirementCount, VkSparseImageMemoryRequirements* pSparseMemoryRequirements) {
    static_assert(std::is_same_v<PFN_vkGetImageSparseMemoryRequirements, decltype(&V_vkGetImageSparseMemoryRequirements)>);
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (rec && rec->dt.vkGetImageSparseMemoryRequirements) rec->dt.vkGetImageSparseMemoryRequirements(device, image, pSparseMemoryRequirementCount, pSparseMemoryRequirements);
}
VKAPI_ATTR void VKAPI_CALL V_vkGetPhysicalDeviceSparseImageFormatProperties(VkPhysicalDevice physicalDevice, VkFormat format, VkImageType type, VkSampleCountFlagBits samples, VkImageUsageFlags usage, VkImageTiling tiling, uint32_t* pPropertyCount, VkSparseImageFormatProperties* pProperties) {
    static_assert(std::is_same_v<PFN_vkGetPhysicalDeviceSparseImageFormatProperties, decltype(&V_vkGetPhysicalDeviceSparseImageFormatProperties)>);
    InstanceRecord* rec = LookupPhysicalDeviceInstance(physicalDevice);
    if (!rec) return;
    auto fn = reinterpret_cast<PFN_vkGetPhysicalDeviceSparseImageFormatProperties>(rec->gipa(rec->hostInstance, "vkGetPhysicalDeviceSparseImageFormatProperties"));
    if (fn) fn(physicalDevice, format, type, samples, usage, tiling, pPropertyCount, pProperties);
}
VKAPI_ATTR VkResult VKAPI_CALL V_vkQueueBindSparse(VkQueue queue, uint32_t bindInfoCount, const VkBindSparseInfo* pBindInfo, VkFence fence) {
    static_assert(std::is_same_v<PFN_vkQueueBindSparse, decltype(&V_vkQueueBindSparse)>);
    DeviceRecord* rec = LookupQueueDevice(queue);
    if (!rec || !rec->dt.vkQueueBindSparse) return VK_ERROR_EXTENSION_NOT_PRESENT;
    return rec->dt.vkQueueBindSparse(queue, bindInfoCount, pBindInfo, fence);
}
VKAPI_ATTR VkResult VKAPI_CALL V_vkCreateFence(VkDevice device, const VkFenceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkFence* pFence) {
    static_assert(std::is_same_v<PFN_vkCreateFence, decltype(&V_vkCreateFence)>);
    if (pCreateInfo) { VkResult pnextErr = ValidateGuestPNext(pCreateInfo->pNext); if (pnextErr != VK_SUCCESS) return pnextErr; }
    bool allocOk = true;
    const VkAllocationCallbacks* hostAlloc = InternGuestAllocator(pAllocator, allocOk);
    if (!allocOk) return VK_ERROR_FEATURE_NOT_PRESENT;
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (!rec || !rec->dt.vkCreateFence) return VK_ERROR_EXTENSION_NOT_PRESENT;
    return rec->dt.vkCreateFence(device, pCreateInfo, hostAlloc, pFence);
}
VKAPI_ATTR void VKAPI_CALL V_vkDestroyFence(VkDevice device, VkFence fence, const VkAllocationCallbacks* pAllocator) {
    static_assert(std::is_same_v<PFN_vkDestroyFence, decltype(&V_vkDestroyFence)>);
    bool allocOk = true;
    const VkAllocationCallbacks* hostAlloc = InternGuestAllocator(pAllocator, allocOk);
    if (!allocOk) return;
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (rec && rec->dt.vkDestroyFence) rec->dt.vkDestroyFence(device, fence, hostAlloc);
}
VKAPI_ATTR VkResult VKAPI_CALL V_vkResetFences(VkDevice device, uint32_t fenceCount, const VkFence* pFences) {
    static_assert(std::is_same_v<PFN_vkResetFences, decltype(&V_vkResetFences)>);
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (!rec || !rec->dt.vkResetFences) return VK_ERROR_EXTENSION_NOT_PRESENT;
    return rec->dt.vkResetFences(device, fenceCount, pFences);
}
VKAPI_ATTR VkResult VKAPI_CALL V_vkGetFenceStatus(VkDevice device, VkFence fence) {
    static_assert(std::is_same_v<PFN_vkGetFenceStatus, decltype(&V_vkGetFenceStatus)>);
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (!rec || !rec->dt.vkGetFenceStatus) return VK_ERROR_EXTENSION_NOT_PRESENT;
    return rec->dt.vkGetFenceStatus(device, fence);
}
VKAPI_ATTR VkResult VKAPI_CALL V_vkWaitForFences(VkDevice device, uint32_t fenceCount, const VkFence* pFences, VkBool32 waitAll, uint64_t timeout) {
    static_assert(std::is_same_v<PFN_vkWaitForFences, decltype(&V_vkWaitForFences)>);
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (!rec || !rec->dt.vkWaitForFences) return VK_ERROR_EXTENSION_NOT_PRESENT;
    return rec->dt.vkWaitForFences(device, fenceCount, pFences, waitAll, timeout);
}
VKAPI_ATTR VkResult VKAPI_CALL V_vkCreateSemaphore(VkDevice device, const VkSemaphoreCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSemaphore* pSemaphore) {
    static_assert(std::is_same_v<PFN_vkCreateSemaphore, decltype(&V_vkCreateSemaphore)>);
    if (pCreateInfo) { VkResult pnextErr = ValidateGuestPNext(pCreateInfo->pNext); if (pnextErr != VK_SUCCESS) return pnextErr; }
    bool allocOk = true;
    const VkAllocationCallbacks* hostAlloc = InternGuestAllocator(pAllocator, allocOk);
    if (!allocOk) return VK_ERROR_FEATURE_NOT_PRESENT;
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (!rec || !rec->dt.vkCreateSemaphore) return VK_ERROR_EXTENSION_NOT_PRESENT;
    return rec->dt.vkCreateSemaphore(device, pCreateInfo, hostAlloc, pSemaphore);
}
VKAPI_ATTR void VKAPI_CALL V_vkDestroySemaphore(VkDevice device, VkSemaphore semaphore, const VkAllocationCallbacks* pAllocator) {
    static_assert(std::is_same_v<PFN_vkDestroySemaphore, decltype(&V_vkDestroySemaphore)>);
    bool allocOk = true;
    const VkAllocationCallbacks* hostAlloc = InternGuestAllocator(pAllocator, allocOk);
    if (!allocOk) return;
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (rec && rec->dt.vkDestroySemaphore) rec->dt.vkDestroySemaphore(device, semaphore, hostAlloc);
}
VKAPI_ATTR VkResult VKAPI_CALL V_vkCreateQueryPool(VkDevice device, const VkQueryPoolCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkQueryPool* pQueryPool) {
    static_assert(std::is_same_v<PFN_vkCreateQueryPool, decltype(&V_vkCreateQueryPool)>);
    if (pCreateInfo) { VkResult pnextErr = ValidateGuestPNext(pCreateInfo->pNext); if (pnextErr != VK_SUCCESS) return pnextErr; }
    bool allocOk = true;
    const VkAllocationCallbacks* hostAlloc = InternGuestAllocator(pAllocator, allocOk);
    if (!allocOk) return VK_ERROR_FEATURE_NOT_PRESENT;
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (!rec || !rec->dt.vkCreateQueryPool) return VK_ERROR_EXTENSION_NOT_PRESENT;
    return rec->dt.vkCreateQueryPool(device, pCreateInfo, hostAlloc, pQueryPool);
}
VKAPI_ATTR void VKAPI_CALL V_vkDestroyQueryPool(VkDevice device, VkQueryPool queryPool, const VkAllocationCallbacks* pAllocator) {
    static_assert(std::is_same_v<PFN_vkDestroyQueryPool, decltype(&V_vkDestroyQueryPool)>);
    bool allocOk = true;
    const VkAllocationCallbacks* hostAlloc = InternGuestAllocator(pAllocator, allocOk);
    if (!allocOk) return;
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (rec && rec->dt.vkDestroyQueryPool) rec->dt.vkDestroyQueryPool(device, queryPool, hostAlloc);
}
VKAPI_ATTR VkResult VKAPI_CALL V_vkGetQueryPoolResults(VkDevice device, VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount, size_t dataSize, void* pData, VkDeviceSize stride, VkQueryResultFlags flags) {
    static_assert(std::is_same_v<PFN_vkGetQueryPoolResults, decltype(&V_vkGetQueryPoolResults)>);
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (!rec || !rec->dt.vkGetQueryPoolResults) return VK_ERROR_EXTENSION_NOT_PRESENT;
    return rec->dt.vkGetQueryPoolResults(device, queryPool, firstQuery, queryCount, dataSize, pData, stride, flags);
}
VKAPI_ATTR VkResult VKAPI_CALL V_vkCreateBuffer(VkDevice device, const VkBufferCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkBuffer* pBuffer) {
    static_assert(std::is_same_v<PFN_vkCreateBuffer, decltype(&V_vkCreateBuffer)>);
    if (pCreateInfo) { VkResult pnextErr = ValidateGuestPNext(pCreateInfo->pNext); if (pnextErr != VK_SUCCESS) return pnextErr; }
    bool allocOk = true;
    const VkAllocationCallbacks* hostAlloc = InternGuestAllocator(pAllocator, allocOk);
    if (!allocOk) return VK_ERROR_FEATURE_NOT_PRESENT;
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (!rec || !rec->dt.vkCreateBuffer) return VK_ERROR_EXTENSION_NOT_PRESENT;
    return rec->dt.vkCreateBuffer(device, pCreateInfo, hostAlloc, pBuffer);
}
VKAPI_ATTR void VKAPI_CALL V_vkDestroyBuffer(VkDevice device, VkBuffer buffer, const VkAllocationCallbacks* pAllocator) {
    static_assert(std::is_same_v<PFN_vkDestroyBuffer, decltype(&V_vkDestroyBuffer)>);
    bool allocOk = true;
    const VkAllocationCallbacks* hostAlloc = InternGuestAllocator(pAllocator, allocOk);
    if (!allocOk) return;
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (rec && rec->dt.vkDestroyBuffer) rec->dt.vkDestroyBuffer(device, buffer, hostAlloc);
}
VKAPI_ATTR VkResult VKAPI_CALL V_vkCreateImage(VkDevice device, const VkImageCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkImage* pImage) {
    static_assert(std::is_same_v<PFN_vkCreateImage, decltype(&V_vkCreateImage)>);
    if (pCreateInfo) { VkResult pnextErr = ValidateGuestPNext(pCreateInfo->pNext); if (pnextErr != VK_SUCCESS) return pnextErr; }
    bool allocOk = true;
    const VkAllocationCallbacks* hostAlloc = InternGuestAllocator(pAllocator, allocOk);
    if (!allocOk) return VK_ERROR_FEATURE_NOT_PRESENT;
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (!rec || !rec->dt.vkCreateImage) return VK_ERROR_EXTENSION_NOT_PRESENT;
    return rec->dt.vkCreateImage(device, pCreateInfo, hostAlloc, pImage);
}
VKAPI_ATTR void VKAPI_CALL V_vkDestroyImage(VkDevice device, VkImage image, const VkAllocationCallbacks* pAllocator) {
    static_assert(std::is_same_v<PFN_vkDestroyImage, decltype(&V_vkDestroyImage)>);
    bool allocOk = true;
    const VkAllocationCallbacks* hostAlloc = InternGuestAllocator(pAllocator, allocOk);
    if (!allocOk) return;
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (rec && rec->dt.vkDestroyImage) rec->dt.vkDestroyImage(device, image, hostAlloc);
}
VKAPI_ATTR void VKAPI_CALL V_vkGetImageSubresourceLayout(VkDevice device, VkImage image, const VkImageSubresource* pSubresource, VkSubresourceLayout* pLayout) {
    static_assert(std::is_same_v<PFN_vkGetImageSubresourceLayout, decltype(&V_vkGetImageSubresourceLayout)>);
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (rec && rec->dt.vkGetImageSubresourceLayout) rec->dt.vkGetImageSubresourceLayout(device, image, pSubresource, pLayout);
}
VKAPI_ATTR VkResult VKAPI_CALL V_vkCreateImageView(VkDevice device, const VkImageViewCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkImageView* pView) {
    static_assert(std::is_same_v<PFN_vkCreateImageView, decltype(&V_vkCreateImageView)>);
    if (pCreateInfo) { VkResult pnextErr = ValidateGuestPNext(pCreateInfo->pNext); if (pnextErr != VK_SUCCESS) return pnextErr; }
    bool allocOk = true;
    const VkAllocationCallbacks* hostAlloc = InternGuestAllocator(pAllocator, allocOk);
    if (!allocOk) return VK_ERROR_FEATURE_NOT_PRESENT;
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (!rec || !rec->dt.vkCreateImageView) return VK_ERROR_EXTENSION_NOT_PRESENT;
    return rec->dt.vkCreateImageView(device, pCreateInfo, hostAlloc, pView);
}
VKAPI_ATTR void VKAPI_CALL V_vkDestroyImageView(VkDevice device, VkImageView imageView, const VkAllocationCallbacks* pAllocator) {
    static_assert(std::is_same_v<PFN_vkDestroyImageView, decltype(&V_vkDestroyImageView)>);
    bool allocOk = true;
    const VkAllocationCallbacks* hostAlloc = InternGuestAllocator(pAllocator, allocOk);
    if (!allocOk) return;
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (rec && rec->dt.vkDestroyImageView) rec->dt.vkDestroyImageView(device, imageView, hostAlloc);
}
VKAPI_ATTR VkResult VKAPI_CALL V_vkCreateCommandPool(VkDevice device, const VkCommandPoolCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkCommandPool* pCommandPool) {
    static_assert(std::is_same_v<PFN_vkCreateCommandPool, decltype(&V_vkCreateCommandPool)>);
    if (pCreateInfo) { VkResult pnextErr = ValidateGuestPNext(pCreateInfo->pNext); if (pnextErr != VK_SUCCESS) return pnextErr; }
    bool allocOk = true;
    const VkAllocationCallbacks* hostAlloc = InternGuestAllocator(pAllocator, allocOk);
    if (!allocOk) return VK_ERROR_FEATURE_NOT_PRESENT;
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (!rec || !rec->dt.vkCreateCommandPool) return VK_ERROR_EXTENSION_NOT_PRESENT;
    return rec->dt.vkCreateCommandPool(device, pCreateInfo, hostAlloc, pCommandPool);
}
VKAPI_ATTR VkResult VKAPI_CALL V_vkResetCommandPool(VkDevice device, VkCommandPool commandPool, VkCommandPoolResetFlags flags) {
    static_assert(std::is_same_v<PFN_vkResetCommandPool, decltype(&V_vkResetCommandPool)>);
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (!rec || !rec->dt.vkResetCommandPool) return VK_ERROR_EXTENSION_NOT_PRESENT;
    return rec->dt.vkResetCommandPool(device, commandPool, flags);
}
VKAPI_ATTR VkResult VKAPI_CALL V_vkBeginCommandBuffer(VkCommandBuffer commandBuffer, const VkCommandBufferBeginInfo* pBeginInfo) {
    static_assert(std::is_same_v<PFN_vkBeginCommandBuffer, decltype(&V_vkBeginCommandBuffer)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (!rec || !rec->dt.vkBeginCommandBuffer) return VK_ERROR_EXTENSION_NOT_PRESENT;
    return rec->dt.vkBeginCommandBuffer(commandBuffer, pBeginInfo);
}
VKAPI_ATTR VkResult VKAPI_CALL V_vkEndCommandBuffer(VkCommandBuffer commandBuffer) {
    static_assert(std::is_same_v<PFN_vkEndCommandBuffer, decltype(&V_vkEndCommandBuffer)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (!rec || !rec->dt.vkEndCommandBuffer) return VK_ERROR_EXTENSION_NOT_PRESENT;
    return rec->dt.vkEndCommandBuffer(commandBuffer);
}
VKAPI_ATTR VkResult VKAPI_CALL V_vkResetCommandBuffer(VkCommandBuffer commandBuffer, VkCommandBufferResetFlags flags) {
    static_assert(std::is_same_v<PFN_vkResetCommandBuffer, decltype(&V_vkResetCommandBuffer)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (!rec || !rec->dt.vkResetCommandBuffer) return VK_ERROR_EXTENSION_NOT_PRESENT;
    return rec->dt.vkResetCommandBuffer(commandBuffer, flags);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdCopyBuffer(VkCommandBuffer commandBuffer, VkBuffer srcBuffer, VkBuffer dstBuffer, uint32_t regionCount, const VkBufferCopy* pRegions) {
    static_assert(std::is_same_v<PFN_vkCmdCopyBuffer, decltype(&V_vkCmdCopyBuffer)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdCopyBuffer) rec->dt.vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, regionCount, pRegions);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdCopyImage(VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkImageCopy* pRegions) {
    static_assert(std::is_same_v<PFN_vkCmdCopyImage, decltype(&V_vkCmdCopyImage)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdCopyImage) rec->dt.vkCmdCopyImage(commandBuffer, srcImage, srcImageLayout, dstImage, dstImageLayout, regionCount, pRegions);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdCopyBufferToImage(VkCommandBuffer commandBuffer, VkBuffer srcBuffer, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkBufferImageCopy* pRegions) {
    static_assert(std::is_same_v<PFN_vkCmdCopyBufferToImage, decltype(&V_vkCmdCopyBufferToImage)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdCopyBufferToImage) rec->dt.vkCmdCopyBufferToImage(commandBuffer, srcBuffer, dstImage, dstImageLayout, regionCount, pRegions);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdCopyImageToBuffer(VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkBuffer dstBuffer, uint32_t regionCount, const VkBufferImageCopy* pRegions) {
    static_assert(std::is_same_v<PFN_vkCmdCopyImageToBuffer, decltype(&V_vkCmdCopyImageToBuffer)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdCopyImageToBuffer) rec->dt.vkCmdCopyImageToBuffer(commandBuffer, srcImage, srcImageLayout, dstBuffer, regionCount, pRegions);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdUpdateBuffer(VkCommandBuffer commandBuffer, VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize dataSize, const void* pData) {
    static_assert(std::is_same_v<PFN_vkCmdUpdateBuffer, decltype(&V_vkCmdUpdateBuffer)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdUpdateBuffer) rec->dt.vkCmdUpdateBuffer(commandBuffer, dstBuffer, dstOffset, dataSize, pData);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdFillBuffer(VkCommandBuffer commandBuffer, VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize size, uint32_t data) {
    static_assert(std::is_same_v<PFN_vkCmdFillBuffer, decltype(&V_vkCmdFillBuffer)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdFillBuffer) rec->dt.vkCmdFillBuffer(commandBuffer, dstBuffer, dstOffset, size, data);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdPipelineBarrier(VkCommandBuffer commandBuffer, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkDependencyFlags dependencyFlags, uint32_t memoryBarrierCount, const VkMemoryBarrier* pMemoryBarriers, uint32_t bufferMemoryBarrierCount, const VkBufferMemoryBarrier* pBufferMemoryBarriers, uint32_t imageMemoryBarrierCount, const VkImageMemoryBarrier* pImageMemoryBarriers) {
    static_assert(std::is_same_v<PFN_vkCmdPipelineBarrier, decltype(&V_vkCmdPipelineBarrier)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdPipelineBarrier) rec->dt.vkCmdPipelineBarrier(commandBuffer, srcStageMask, dstStageMask, dependencyFlags, memoryBarrierCount, pMemoryBarriers, bufferMemoryBarrierCount, pBufferMemoryBarriers, imageMemoryBarrierCount, pImageMemoryBarriers);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdBeginQuery(VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t query, VkQueryControlFlags flags) {
    static_assert(std::is_same_v<PFN_vkCmdBeginQuery, decltype(&V_vkCmdBeginQuery)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdBeginQuery) rec->dt.vkCmdBeginQuery(commandBuffer, queryPool, query, flags);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdEndQuery(VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t query) {
    static_assert(std::is_same_v<PFN_vkCmdEndQuery, decltype(&V_vkCmdEndQuery)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdEndQuery) rec->dt.vkCmdEndQuery(commandBuffer, queryPool, query);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdResetQueryPool(VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount) {
    static_assert(std::is_same_v<PFN_vkCmdResetQueryPool, decltype(&V_vkCmdResetQueryPool)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdResetQueryPool) rec->dt.vkCmdResetQueryPool(commandBuffer, queryPool, firstQuery, queryCount);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdWriteTimestamp(VkCommandBuffer commandBuffer, VkPipelineStageFlagBits pipelineStage, VkQueryPool queryPool, uint32_t query) {
    static_assert(std::is_same_v<PFN_vkCmdWriteTimestamp, decltype(&V_vkCmdWriteTimestamp)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdWriteTimestamp) rec->dt.vkCmdWriteTimestamp(commandBuffer, pipelineStage, queryPool, query);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdCopyQueryPoolResults(VkCommandBuffer commandBuffer, VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount, VkBuffer dstBuffer, VkDeviceSize dstOffset, VkDeviceSize stride, VkQueryResultFlags flags) {
    static_assert(std::is_same_v<PFN_vkCmdCopyQueryPoolResults, decltype(&V_vkCmdCopyQueryPoolResults)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdCopyQueryPoolResults) rec->dt.vkCmdCopyQueryPoolResults(commandBuffer, queryPool, firstQuery, queryCount, dstBuffer, dstOffset, stride, flags);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdExecuteCommands(VkCommandBuffer commandBuffer, uint32_t commandBufferCount, const VkCommandBuffer* pCommandBuffers) {
    static_assert(std::is_same_v<PFN_vkCmdExecuteCommands, decltype(&V_vkCmdExecuteCommands)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdExecuteCommands) rec->dt.vkCmdExecuteCommands(commandBuffer, commandBufferCount, pCommandBuffers);
}
VKAPI_ATTR VkResult VKAPI_CALL V_vkCreateEvent(VkDevice device, const VkEventCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkEvent* pEvent) {
    static_assert(std::is_same_v<PFN_vkCreateEvent, decltype(&V_vkCreateEvent)>);
    if (pCreateInfo) { VkResult pnextErr = ValidateGuestPNext(pCreateInfo->pNext); if (pnextErr != VK_SUCCESS) return pnextErr; }
    bool allocOk = true;
    const VkAllocationCallbacks* hostAlloc = InternGuestAllocator(pAllocator, allocOk);
    if (!allocOk) return VK_ERROR_FEATURE_NOT_PRESENT;
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (!rec || !rec->dt.vkCreateEvent) return VK_ERROR_EXTENSION_NOT_PRESENT;
    return rec->dt.vkCreateEvent(device, pCreateInfo, hostAlloc, pEvent);
}
VKAPI_ATTR void VKAPI_CALL V_vkDestroyEvent(VkDevice device, VkEvent event, const VkAllocationCallbacks* pAllocator) {
    static_assert(std::is_same_v<PFN_vkDestroyEvent, decltype(&V_vkDestroyEvent)>);
    bool allocOk = true;
    const VkAllocationCallbacks* hostAlloc = InternGuestAllocator(pAllocator, allocOk);
    if (!allocOk) return;
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (rec && rec->dt.vkDestroyEvent) rec->dt.vkDestroyEvent(device, event, hostAlloc);
}
VKAPI_ATTR VkResult VKAPI_CALL V_vkGetEventStatus(VkDevice device, VkEvent event) {
    static_assert(std::is_same_v<PFN_vkGetEventStatus, decltype(&V_vkGetEventStatus)>);
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (!rec || !rec->dt.vkGetEventStatus) return VK_ERROR_EXTENSION_NOT_PRESENT;
    return rec->dt.vkGetEventStatus(device, event);
}
VKAPI_ATTR VkResult VKAPI_CALL V_vkSetEvent(VkDevice device, VkEvent event) {
    static_assert(std::is_same_v<PFN_vkSetEvent, decltype(&V_vkSetEvent)>);
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (!rec || !rec->dt.vkSetEvent) return VK_ERROR_EXTENSION_NOT_PRESENT;
    return rec->dt.vkSetEvent(device, event);
}
VKAPI_ATTR VkResult VKAPI_CALL V_vkResetEvent(VkDevice device, VkEvent event) {
    static_assert(std::is_same_v<PFN_vkResetEvent, decltype(&V_vkResetEvent)>);
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (!rec || !rec->dt.vkResetEvent) return VK_ERROR_EXTENSION_NOT_PRESENT;
    return rec->dt.vkResetEvent(device, event);
}
VKAPI_ATTR VkResult VKAPI_CALL V_vkCreateBufferView(VkDevice device, const VkBufferViewCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkBufferView* pView) {
    static_assert(std::is_same_v<PFN_vkCreateBufferView, decltype(&V_vkCreateBufferView)>);
    if (pCreateInfo) { VkResult pnextErr = ValidateGuestPNext(pCreateInfo->pNext); if (pnextErr != VK_SUCCESS) return pnextErr; }
    bool allocOk = true;
    const VkAllocationCallbacks* hostAlloc = InternGuestAllocator(pAllocator, allocOk);
    if (!allocOk) return VK_ERROR_FEATURE_NOT_PRESENT;
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (!rec || !rec->dt.vkCreateBufferView) return VK_ERROR_EXTENSION_NOT_PRESENT;
    return rec->dt.vkCreateBufferView(device, pCreateInfo, hostAlloc, pView);
}
VKAPI_ATTR void VKAPI_CALL V_vkDestroyBufferView(VkDevice device, VkBufferView bufferView, const VkAllocationCallbacks* pAllocator) {
    static_assert(std::is_same_v<PFN_vkDestroyBufferView, decltype(&V_vkDestroyBufferView)>);
    bool allocOk = true;
    const VkAllocationCallbacks* hostAlloc = InternGuestAllocator(pAllocator, allocOk);
    if (!allocOk) return;
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (rec && rec->dt.vkDestroyBufferView) rec->dt.vkDestroyBufferView(device, bufferView, hostAlloc);
}
VKAPI_ATTR VkResult VKAPI_CALL V_vkCreateShaderModule(VkDevice device, const VkShaderModuleCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkShaderModule* pShaderModule) {
    static_assert(std::is_same_v<PFN_vkCreateShaderModule, decltype(&V_vkCreateShaderModule)>);
    if (pCreateInfo) { VkResult pnextErr = ValidateGuestPNext(pCreateInfo->pNext); if (pnextErr != VK_SUCCESS) return pnextErr; }
    bool allocOk = true;
    const VkAllocationCallbacks* hostAlloc = InternGuestAllocator(pAllocator, allocOk);
    if (!allocOk) return VK_ERROR_FEATURE_NOT_PRESENT;
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (!rec || !rec->dt.vkCreateShaderModule) return VK_ERROR_EXTENSION_NOT_PRESENT;
    return rec->dt.vkCreateShaderModule(device, pCreateInfo, hostAlloc, pShaderModule);
}
VKAPI_ATTR void VKAPI_CALL V_vkDestroyShaderModule(VkDevice device, VkShaderModule shaderModule, const VkAllocationCallbacks* pAllocator) {
    static_assert(std::is_same_v<PFN_vkDestroyShaderModule, decltype(&V_vkDestroyShaderModule)>);
    bool allocOk = true;
    const VkAllocationCallbacks* hostAlloc = InternGuestAllocator(pAllocator, allocOk);
    if (!allocOk) return;
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (rec && rec->dt.vkDestroyShaderModule) rec->dt.vkDestroyShaderModule(device, shaderModule, hostAlloc);
}
VKAPI_ATTR VkResult VKAPI_CALL V_vkCreatePipelineCache(VkDevice device, const VkPipelineCacheCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkPipelineCache* pPipelineCache) {
    static_assert(std::is_same_v<PFN_vkCreatePipelineCache, decltype(&V_vkCreatePipelineCache)>);
    if (pCreateInfo) { VkResult pnextErr = ValidateGuestPNext(pCreateInfo->pNext); if (pnextErr != VK_SUCCESS) return pnextErr; }
    bool allocOk = true;
    const VkAllocationCallbacks* hostAlloc = InternGuestAllocator(pAllocator, allocOk);
    if (!allocOk) return VK_ERROR_FEATURE_NOT_PRESENT;
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (!rec || !rec->dt.vkCreatePipelineCache) return VK_ERROR_EXTENSION_NOT_PRESENT;
    return rec->dt.vkCreatePipelineCache(device, pCreateInfo, hostAlloc, pPipelineCache);
}
VKAPI_ATTR void VKAPI_CALL V_vkDestroyPipelineCache(VkDevice device, VkPipelineCache pipelineCache, const VkAllocationCallbacks* pAllocator) {
    static_assert(std::is_same_v<PFN_vkDestroyPipelineCache, decltype(&V_vkDestroyPipelineCache)>);
    bool allocOk = true;
    const VkAllocationCallbacks* hostAlloc = InternGuestAllocator(pAllocator, allocOk);
    if (!allocOk) return;
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (rec && rec->dt.vkDestroyPipelineCache) rec->dt.vkDestroyPipelineCache(device, pipelineCache, hostAlloc);
}
VKAPI_ATTR VkResult VKAPI_CALL V_vkGetPipelineCacheData(VkDevice device, VkPipelineCache pipelineCache, size_t* pDataSize, void* pData) {
    static_assert(std::is_same_v<PFN_vkGetPipelineCacheData, decltype(&V_vkGetPipelineCacheData)>);
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (!rec || !rec->dt.vkGetPipelineCacheData) return VK_ERROR_EXTENSION_NOT_PRESENT;
    return rec->dt.vkGetPipelineCacheData(device, pipelineCache, pDataSize, pData);
}
VKAPI_ATTR VkResult VKAPI_CALL V_vkMergePipelineCaches(VkDevice device, VkPipelineCache dstCache, uint32_t srcCacheCount, const VkPipelineCache* pSrcCaches) {
    static_assert(std::is_same_v<PFN_vkMergePipelineCaches, decltype(&V_vkMergePipelineCaches)>);
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (!rec || !rec->dt.vkMergePipelineCaches) return VK_ERROR_EXTENSION_NOT_PRESENT;
    return rec->dt.vkMergePipelineCaches(device, dstCache, srcCacheCount, pSrcCaches);
}
VKAPI_ATTR VkResult VKAPI_CALL V_vkCreateComputePipelines(VkDevice device, VkPipelineCache pipelineCache, uint32_t createInfoCount, const VkComputePipelineCreateInfo* pCreateInfos, const VkAllocationCallbacks* pAllocator, VkPipeline* pPipelines) {
    static_assert(std::is_same_v<PFN_vkCreateComputePipelines, decltype(&V_vkCreateComputePipelines)>);
    bool allocOk = true;
    const VkAllocationCallbacks* hostAlloc = InternGuestAllocator(pAllocator, allocOk);
    if (!allocOk) return VK_ERROR_FEATURE_NOT_PRESENT;
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (!rec || !rec->dt.vkCreateComputePipelines) return VK_ERROR_EXTENSION_NOT_PRESENT;
    return rec->dt.vkCreateComputePipelines(device, pipelineCache, createInfoCount, pCreateInfos, hostAlloc, pPipelines);
}
VKAPI_ATTR void VKAPI_CALL V_vkDestroyPipeline(VkDevice device, VkPipeline pipeline, const VkAllocationCallbacks* pAllocator) {
    static_assert(std::is_same_v<PFN_vkDestroyPipeline, decltype(&V_vkDestroyPipeline)>);
    bool allocOk = true;
    const VkAllocationCallbacks* hostAlloc = InternGuestAllocator(pAllocator, allocOk);
    if (!allocOk) return;
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (rec && rec->dt.vkDestroyPipeline) rec->dt.vkDestroyPipeline(device, pipeline, hostAlloc);
}
VKAPI_ATTR VkResult VKAPI_CALL V_vkCreatePipelineLayout(VkDevice device, const VkPipelineLayoutCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkPipelineLayout* pPipelineLayout) {
    static_assert(std::is_same_v<PFN_vkCreatePipelineLayout, decltype(&V_vkCreatePipelineLayout)>);
    if (pCreateInfo) { VkResult pnextErr = ValidateGuestPNext(pCreateInfo->pNext); if (pnextErr != VK_SUCCESS) return pnextErr; }
    bool allocOk = true;
    const VkAllocationCallbacks* hostAlloc = InternGuestAllocator(pAllocator, allocOk);
    if (!allocOk) return VK_ERROR_FEATURE_NOT_PRESENT;
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (!rec || !rec->dt.vkCreatePipelineLayout) return VK_ERROR_EXTENSION_NOT_PRESENT;
    return rec->dt.vkCreatePipelineLayout(device, pCreateInfo, hostAlloc, pPipelineLayout);
}
VKAPI_ATTR void VKAPI_CALL V_vkDestroyPipelineLayout(VkDevice device, VkPipelineLayout pipelineLayout, const VkAllocationCallbacks* pAllocator) {
    static_assert(std::is_same_v<PFN_vkDestroyPipelineLayout, decltype(&V_vkDestroyPipelineLayout)>);
    bool allocOk = true;
    const VkAllocationCallbacks* hostAlloc = InternGuestAllocator(pAllocator, allocOk);
    if (!allocOk) return;
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (rec && rec->dt.vkDestroyPipelineLayout) rec->dt.vkDestroyPipelineLayout(device, pipelineLayout, hostAlloc);
}
VKAPI_ATTR VkResult VKAPI_CALL V_vkCreateSampler(VkDevice device, const VkSamplerCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSampler* pSampler) {
    static_assert(std::is_same_v<PFN_vkCreateSampler, decltype(&V_vkCreateSampler)>);
    if (pCreateInfo) { VkResult pnextErr = ValidateGuestPNext(pCreateInfo->pNext); if (pnextErr != VK_SUCCESS) return pnextErr; }
    bool allocOk = true;
    const VkAllocationCallbacks* hostAlloc = InternGuestAllocator(pAllocator, allocOk);
    if (!allocOk) return VK_ERROR_FEATURE_NOT_PRESENT;
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (!rec || !rec->dt.vkCreateSampler) return VK_ERROR_EXTENSION_NOT_PRESENT;
    return rec->dt.vkCreateSampler(device, pCreateInfo, hostAlloc, pSampler);
}
VKAPI_ATTR void VKAPI_CALL V_vkDestroySampler(VkDevice device, VkSampler sampler, const VkAllocationCallbacks* pAllocator) {
    static_assert(std::is_same_v<PFN_vkDestroySampler, decltype(&V_vkDestroySampler)>);
    bool allocOk = true;
    const VkAllocationCallbacks* hostAlloc = InternGuestAllocator(pAllocator, allocOk);
    if (!allocOk) return;
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (rec && rec->dt.vkDestroySampler) rec->dt.vkDestroySampler(device, sampler, hostAlloc);
}
VKAPI_ATTR VkResult VKAPI_CALL V_vkCreateDescriptorSetLayout(VkDevice device, const VkDescriptorSetLayoutCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDescriptorSetLayout* pSetLayout) {
    static_assert(std::is_same_v<PFN_vkCreateDescriptorSetLayout, decltype(&V_vkCreateDescriptorSetLayout)>);
    if (pCreateInfo) { VkResult pnextErr = ValidateGuestPNext(pCreateInfo->pNext); if (pnextErr != VK_SUCCESS) return pnextErr; }
    bool allocOk = true;
    const VkAllocationCallbacks* hostAlloc = InternGuestAllocator(pAllocator, allocOk);
    if (!allocOk) return VK_ERROR_FEATURE_NOT_PRESENT;
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (!rec || !rec->dt.vkCreateDescriptorSetLayout) return VK_ERROR_EXTENSION_NOT_PRESENT;
    return rec->dt.vkCreateDescriptorSetLayout(device, pCreateInfo, hostAlloc, pSetLayout);
}
VKAPI_ATTR void VKAPI_CALL V_vkDestroyDescriptorSetLayout(VkDevice device, VkDescriptorSetLayout descriptorSetLayout, const VkAllocationCallbacks* pAllocator) {
    static_assert(std::is_same_v<PFN_vkDestroyDescriptorSetLayout, decltype(&V_vkDestroyDescriptorSetLayout)>);
    bool allocOk = true;
    const VkAllocationCallbacks* hostAlloc = InternGuestAllocator(pAllocator, allocOk);
    if (!allocOk) return;
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (rec && rec->dt.vkDestroyDescriptorSetLayout) rec->dt.vkDestroyDescriptorSetLayout(device, descriptorSetLayout, hostAlloc);
}
VKAPI_ATTR VkResult VKAPI_CALL V_vkCreateDescriptorPool(VkDevice device, const VkDescriptorPoolCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDescriptorPool* pDescriptorPool) {
    static_assert(std::is_same_v<PFN_vkCreateDescriptorPool, decltype(&V_vkCreateDescriptorPool)>);
    if (pCreateInfo) { VkResult pnextErr = ValidateGuestPNext(pCreateInfo->pNext); if (pnextErr != VK_SUCCESS) return pnextErr; }
    bool allocOk = true;
    const VkAllocationCallbacks* hostAlloc = InternGuestAllocator(pAllocator, allocOk);
    if (!allocOk) return VK_ERROR_FEATURE_NOT_PRESENT;
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (!rec || !rec->dt.vkCreateDescriptorPool) return VK_ERROR_EXTENSION_NOT_PRESENT;
    return rec->dt.vkCreateDescriptorPool(device, pCreateInfo, hostAlloc, pDescriptorPool);
}
VKAPI_ATTR void VKAPI_CALL V_vkDestroyDescriptorPool(VkDevice device, VkDescriptorPool descriptorPool, const VkAllocationCallbacks* pAllocator) {
    static_assert(std::is_same_v<PFN_vkDestroyDescriptorPool, decltype(&V_vkDestroyDescriptorPool)>);
    bool allocOk = true;
    const VkAllocationCallbacks* hostAlloc = InternGuestAllocator(pAllocator, allocOk);
    if (!allocOk) return;
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (rec && rec->dt.vkDestroyDescriptorPool) rec->dt.vkDestroyDescriptorPool(device, descriptorPool, hostAlloc);
}
VKAPI_ATTR VkResult VKAPI_CALL V_vkResetDescriptorPool(VkDevice device, VkDescriptorPool descriptorPool, VkDescriptorPoolResetFlags flags) {
    static_assert(std::is_same_v<PFN_vkResetDescriptorPool, decltype(&V_vkResetDescriptorPool)>);
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (!rec || !rec->dt.vkResetDescriptorPool) return VK_ERROR_EXTENSION_NOT_PRESENT;
    return rec->dt.vkResetDescriptorPool(device, descriptorPool, flags);
}
VKAPI_ATTR VkResult VKAPI_CALL V_vkAllocateDescriptorSets(VkDevice device, const VkDescriptorSetAllocateInfo* pAllocateInfo, VkDescriptorSet* pDescriptorSets) {
    static_assert(std::is_same_v<PFN_vkAllocateDescriptorSets, decltype(&V_vkAllocateDescriptorSets)>);
    if (pAllocateInfo) { VkResult pnextErr = ValidateGuestPNext(pAllocateInfo->pNext); if (pnextErr != VK_SUCCESS) return pnextErr; }
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (!rec || !rec->dt.vkAllocateDescriptorSets) return VK_ERROR_EXTENSION_NOT_PRESENT;
    return rec->dt.vkAllocateDescriptorSets(device, pAllocateInfo, pDescriptorSets);
}
VKAPI_ATTR VkResult VKAPI_CALL V_vkFreeDescriptorSets(VkDevice device, VkDescriptorPool descriptorPool, uint32_t descriptorSetCount, const VkDescriptorSet* pDescriptorSets) {
    static_assert(std::is_same_v<PFN_vkFreeDescriptorSets, decltype(&V_vkFreeDescriptorSets)>);
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (!rec || !rec->dt.vkFreeDescriptorSets) return VK_ERROR_EXTENSION_NOT_PRESENT;
    return rec->dt.vkFreeDescriptorSets(device, descriptorPool, descriptorSetCount, pDescriptorSets);
}
VKAPI_ATTR void VKAPI_CALL V_vkUpdateDescriptorSets(VkDevice device, uint32_t descriptorWriteCount, const VkWriteDescriptorSet* pDescriptorWrites, uint32_t descriptorCopyCount, const VkCopyDescriptorSet* pDescriptorCopies) {
    static_assert(std::is_same_v<PFN_vkUpdateDescriptorSets, decltype(&V_vkUpdateDescriptorSets)>);
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (rec && rec->dt.vkUpdateDescriptorSets) rec->dt.vkUpdateDescriptorSets(device, descriptorWriteCount, pDescriptorWrites, descriptorCopyCount, pDescriptorCopies);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdBindPipeline(VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipeline pipeline) {
    static_assert(std::is_same_v<PFN_vkCmdBindPipeline, decltype(&V_vkCmdBindPipeline)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdBindPipeline) rec->dt.vkCmdBindPipeline(commandBuffer, pipelineBindPoint, pipeline);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdBindDescriptorSets(VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout, uint32_t firstSet, uint32_t descriptorSetCount, const VkDescriptorSet* pDescriptorSets, uint32_t dynamicOffsetCount, const uint32_t* pDynamicOffsets) {
    static_assert(std::is_same_v<PFN_vkCmdBindDescriptorSets, decltype(&V_vkCmdBindDescriptorSets)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdBindDescriptorSets) rec->dt.vkCmdBindDescriptorSets(commandBuffer, pipelineBindPoint, layout, firstSet, descriptorSetCount, pDescriptorSets, dynamicOffsetCount, pDynamicOffsets);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdClearColorImage(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout imageLayout, const VkClearColorValue* pColor, uint32_t rangeCount, const VkImageSubresourceRange* pRanges) {
    static_assert(std::is_same_v<PFN_vkCmdClearColorImage, decltype(&V_vkCmdClearColorImage)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdClearColorImage) rec->dt.vkCmdClearColorImage(commandBuffer, image, imageLayout, pColor, rangeCount, pRanges);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdDispatch(VkCommandBuffer commandBuffer, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) {
    static_assert(std::is_same_v<PFN_vkCmdDispatch, decltype(&V_vkCmdDispatch)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdDispatch) rec->dt.vkCmdDispatch(commandBuffer, groupCountX, groupCountY, groupCountZ);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdDispatchIndirect(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset) {
    static_assert(std::is_same_v<PFN_vkCmdDispatchIndirect, decltype(&V_vkCmdDispatchIndirect)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdDispatchIndirect) rec->dt.vkCmdDispatchIndirect(commandBuffer, buffer, offset);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdSetEvent(VkCommandBuffer commandBuffer, VkEvent event, VkPipelineStageFlags stageMask) {
    static_assert(std::is_same_v<PFN_vkCmdSetEvent, decltype(&V_vkCmdSetEvent)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdSetEvent) rec->dt.vkCmdSetEvent(commandBuffer, event, stageMask);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdResetEvent(VkCommandBuffer commandBuffer, VkEvent event, VkPipelineStageFlags stageMask) {
    static_assert(std::is_same_v<PFN_vkCmdResetEvent, decltype(&V_vkCmdResetEvent)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdResetEvent) rec->dt.vkCmdResetEvent(commandBuffer, event, stageMask);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdWaitEvents(VkCommandBuffer commandBuffer, uint32_t eventCount, const VkEvent* pEvents, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, uint32_t memoryBarrierCount, const VkMemoryBarrier* pMemoryBarriers, uint32_t bufferMemoryBarrierCount, const VkBufferMemoryBarrier* pBufferMemoryBarriers, uint32_t imageMemoryBarrierCount, const VkImageMemoryBarrier* pImageMemoryBarriers) {
    static_assert(std::is_same_v<PFN_vkCmdWaitEvents, decltype(&V_vkCmdWaitEvents)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdWaitEvents) rec->dt.vkCmdWaitEvents(commandBuffer, eventCount, pEvents, srcStageMask, dstStageMask, memoryBarrierCount, pMemoryBarriers, bufferMemoryBarrierCount, pBufferMemoryBarriers, imageMemoryBarrierCount, pImageMemoryBarriers);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdPushConstants(VkCommandBuffer commandBuffer, VkPipelineLayout layout, VkShaderStageFlags stageFlags, uint32_t offset, uint32_t size, const void* pValues) {
    static_assert(std::is_same_v<PFN_vkCmdPushConstants, decltype(&V_vkCmdPushConstants)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdPushConstants) rec->dt.vkCmdPushConstants(commandBuffer, layout, stageFlags, offset, size, pValues);
}
VKAPI_ATTR VkResult VKAPI_CALL V_vkCreateGraphicsPipelines(VkDevice device, VkPipelineCache pipelineCache, uint32_t createInfoCount, const VkGraphicsPipelineCreateInfo* pCreateInfos, const VkAllocationCallbacks* pAllocator, VkPipeline* pPipelines) {
    static_assert(std::is_same_v<PFN_vkCreateGraphicsPipelines, decltype(&V_vkCreateGraphicsPipelines)>);
    bool allocOk = true;
    const VkAllocationCallbacks* hostAlloc = InternGuestAllocator(pAllocator, allocOk);
    if (!allocOk) return VK_ERROR_FEATURE_NOT_PRESENT;
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (!rec || !rec->dt.vkCreateGraphicsPipelines) return VK_ERROR_EXTENSION_NOT_PRESENT;
    return rec->dt.vkCreateGraphicsPipelines(device, pipelineCache, createInfoCount, pCreateInfos, hostAlloc, pPipelines);
}
VKAPI_ATTR VkResult VKAPI_CALL V_vkCreateFramebuffer(VkDevice device, const VkFramebufferCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkFramebuffer* pFramebuffer) {
    static_assert(std::is_same_v<PFN_vkCreateFramebuffer, decltype(&V_vkCreateFramebuffer)>);
    if (pCreateInfo) { VkResult pnextErr = ValidateGuestPNext(pCreateInfo->pNext); if (pnextErr != VK_SUCCESS) return pnextErr; }
    bool allocOk = true;
    const VkAllocationCallbacks* hostAlloc = InternGuestAllocator(pAllocator, allocOk);
    if (!allocOk) return VK_ERROR_FEATURE_NOT_PRESENT;
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (!rec || !rec->dt.vkCreateFramebuffer) return VK_ERROR_EXTENSION_NOT_PRESENT;
    return rec->dt.vkCreateFramebuffer(device, pCreateInfo, hostAlloc, pFramebuffer);
}
VKAPI_ATTR void VKAPI_CALL V_vkDestroyFramebuffer(VkDevice device, VkFramebuffer framebuffer, const VkAllocationCallbacks* pAllocator) {
    static_assert(std::is_same_v<PFN_vkDestroyFramebuffer, decltype(&V_vkDestroyFramebuffer)>);
    bool allocOk = true;
    const VkAllocationCallbacks* hostAlloc = InternGuestAllocator(pAllocator, allocOk);
    if (!allocOk) return;
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (rec && rec->dt.vkDestroyFramebuffer) rec->dt.vkDestroyFramebuffer(device, framebuffer, hostAlloc);
}
VKAPI_ATTR VkResult VKAPI_CALL V_vkCreateRenderPass(VkDevice device, const VkRenderPassCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkRenderPass* pRenderPass) {
    static_assert(std::is_same_v<PFN_vkCreateRenderPass, decltype(&V_vkCreateRenderPass)>);
    if (pCreateInfo) { VkResult pnextErr = ValidateGuestPNext(pCreateInfo->pNext); if (pnextErr != VK_SUCCESS) return pnextErr; }
    bool allocOk = true;
    const VkAllocationCallbacks* hostAlloc = InternGuestAllocator(pAllocator, allocOk);
    if (!allocOk) return VK_ERROR_FEATURE_NOT_PRESENT;
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (!rec || !rec->dt.vkCreateRenderPass) return VK_ERROR_EXTENSION_NOT_PRESENT;
    return rec->dt.vkCreateRenderPass(device, pCreateInfo, hostAlloc, pRenderPass);
}
VKAPI_ATTR void VKAPI_CALL V_vkDestroyRenderPass(VkDevice device, VkRenderPass renderPass, const VkAllocationCallbacks* pAllocator) {
    static_assert(std::is_same_v<PFN_vkDestroyRenderPass, decltype(&V_vkDestroyRenderPass)>);
    bool allocOk = true;
    const VkAllocationCallbacks* hostAlloc = InternGuestAllocator(pAllocator, allocOk);
    if (!allocOk) return;
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (rec && rec->dt.vkDestroyRenderPass) rec->dt.vkDestroyRenderPass(device, renderPass, hostAlloc);
}
VKAPI_ATTR void VKAPI_CALL V_vkGetRenderAreaGranularity(VkDevice device, VkRenderPass renderPass, VkExtent2D* pGranularity) {
    static_assert(std::is_same_v<PFN_vkGetRenderAreaGranularity, decltype(&V_vkGetRenderAreaGranularity)>);
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (rec && rec->dt.vkGetRenderAreaGranularity) rec->dt.vkGetRenderAreaGranularity(device, renderPass, pGranularity);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdSetViewport(VkCommandBuffer commandBuffer, uint32_t firstViewport, uint32_t viewportCount, const VkViewport* pViewports) {
    static_assert(std::is_same_v<PFN_vkCmdSetViewport, decltype(&V_vkCmdSetViewport)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdSetViewport) rec->dt.vkCmdSetViewport(commandBuffer, firstViewport, viewportCount, pViewports);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdSetScissor(VkCommandBuffer commandBuffer, uint32_t firstScissor, uint32_t scissorCount, const VkRect2D* pScissors) {
    static_assert(std::is_same_v<PFN_vkCmdSetScissor, decltype(&V_vkCmdSetScissor)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdSetScissor) rec->dt.vkCmdSetScissor(commandBuffer, firstScissor, scissorCount, pScissors);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdSetLineWidth(VkCommandBuffer commandBuffer, float lineWidth) {
    static_assert(std::is_same_v<PFN_vkCmdSetLineWidth, decltype(&V_vkCmdSetLineWidth)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdSetLineWidth) rec->dt.vkCmdSetLineWidth(commandBuffer, lineWidth);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdSetDepthBias(VkCommandBuffer commandBuffer, float depthBiasConstantFactor, float depthBiasClamp, float depthBiasSlopeFactor) {
    static_assert(std::is_same_v<PFN_vkCmdSetDepthBias, decltype(&V_vkCmdSetDepthBias)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdSetDepthBias) rec->dt.vkCmdSetDepthBias(commandBuffer, depthBiasConstantFactor, depthBiasClamp, depthBiasSlopeFactor);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdSetBlendConstants(VkCommandBuffer commandBuffer, const float blendConstants[4]) {
    static_assert(std::is_same_v<PFN_vkCmdSetBlendConstants, decltype(&V_vkCmdSetBlendConstants)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdSetBlendConstants) rec->dt.vkCmdSetBlendConstants(commandBuffer, blendConstants);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdSetDepthBounds(VkCommandBuffer commandBuffer, float minDepthBounds, float maxDepthBounds) {
    static_assert(std::is_same_v<PFN_vkCmdSetDepthBounds, decltype(&V_vkCmdSetDepthBounds)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdSetDepthBounds) rec->dt.vkCmdSetDepthBounds(commandBuffer, minDepthBounds, maxDepthBounds);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdSetStencilCompareMask(VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, uint32_t compareMask) {
    static_assert(std::is_same_v<PFN_vkCmdSetStencilCompareMask, decltype(&V_vkCmdSetStencilCompareMask)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdSetStencilCompareMask) rec->dt.vkCmdSetStencilCompareMask(commandBuffer, faceMask, compareMask);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdSetStencilWriteMask(VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, uint32_t writeMask) {
    static_assert(std::is_same_v<PFN_vkCmdSetStencilWriteMask, decltype(&V_vkCmdSetStencilWriteMask)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdSetStencilWriteMask) rec->dt.vkCmdSetStencilWriteMask(commandBuffer, faceMask, writeMask);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdSetStencilReference(VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, uint32_t reference) {
    static_assert(std::is_same_v<PFN_vkCmdSetStencilReference, decltype(&V_vkCmdSetStencilReference)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdSetStencilReference) rec->dt.vkCmdSetStencilReference(commandBuffer, faceMask, reference);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdBindIndexBuffer(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkIndexType indexType) {
    static_assert(std::is_same_v<PFN_vkCmdBindIndexBuffer, decltype(&V_vkCmdBindIndexBuffer)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdBindIndexBuffer) rec->dt.vkCmdBindIndexBuffer(commandBuffer, buffer, offset, indexType);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdBindVertexBuffers(VkCommandBuffer commandBuffer, uint32_t firstBinding, uint32_t bindingCount, const VkBuffer* pBuffers, const VkDeviceSize* pOffsets) {
    static_assert(std::is_same_v<PFN_vkCmdBindVertexBuffers, decltype(&V_vkCmdBindVertexBuffers)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdBindVertexBuffers) rec->dt.vkCmdBindVertexBuffers(commandBuffer, firstBinding, bindingCount, pBuffers, pOffsets);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdDraw(VkCommandBuffer commandBuffer, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) {
    static_assert(std::is_same_v<PFN_vkCmdDraw, decltype(&V_vkCmdDraw)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdDraw) rec->dt.vkCmdDraw(commandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdDrawIndexed(VkCommandBuffer commandBuffer, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance) {
    static_assert(std::is_same_v<PFN_vkCmdDrawIndexed, decltype(&V_vkCmdDrawIndexed)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdDrawIndexed) rec->dt.vkCmdDrawIndexed(commandBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdDrawIndirect(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride) {
    static_assert(std::is_same_v<PFN_vkCmdDrawIndirect, decltype(&V_vkCmdDrawIndirect)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdDrawIndirect) rec->dt.vkCmdDrawIndirect(commandBuffer, buffer, offset, drawCount, stride);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdDrawIndexedIndirect(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride) {
    static_assert(std::is_same_v<PFN_vkCmdDrawIndexedIndirect, decltype(&V_vkCmdDrawIndexedIndirect)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdDrawIndexedIndirect) rec->dt.vkCmdDrawIndexedIndirect(commandBuffer, buffer, offset, drawCount, stride);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdBlitImage(VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkImageBlit* pRegions, VkFilter filter) {
    static_assert(std::is_same_v<PFN_vkCmdBlitImage, decltype(&V_vkCmdBlitImage)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdBlitImage) rec->dt.vkCmdBlitImage(commandBuffer, srcImage, srcImageLayout, dstImage, dstImageLayout, regionCount, pRegions, filter);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdClearDepthStencilImage(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout imageLayout, const VkClearDepthStencilValue* pDepthStencil, uint32_t rangeCount, const VkImageSubresourceRange* pRanges) {
    static_assert(std::is_same_v<PFN_vkCmdClearDepthStencilImage, decltype(&V_vkCmdClearDepthStencilImage)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdClearDepthStencilImage) rec->dt.vkCmdClearDepthStencilImage(commandBuffer, image, imageLayout, pDepthStencil, rangeCount, pRanges);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdClearAttachments(VkCommandBuffer commandBuffer, uint32_t attachmentCount, const VkClearAttachment* pAttachments, uint32_t rectCount, const VkClearRect* pRects) {
    static_assert(std::is_same_v<PFN_vkCmdClearAttachments, decltype(&V_vkCmdClearAttachments)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdClearAttachments) rec->dt.vkCmdClearAttachments(commandBuffer, attachmentCount, pAttachments, rectCount, pRects);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdResolveImage(VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkImageResolve* pRegions) {
    static_assert(std::is_same_v<PFN_vkCmdResolveImage, decltype(&V_vkCmdResolveImage)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdResolveImage) rec->dt.vkCmdResolveImage(commandBuffer, srcImage, srcImageLayout, dstImage, dstImageLayout, regionCount, pRegions);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdBeginRenderPass(VkCommandBuffer commandBuffer, const VkRenderPassBeginInfo* pRenderPassBegin, VkSubpassContents contents) {
    static_assert(std::is_same_v<PFN_vkCmdBeginRenderPass, decltype(&V_vkCmdBeginRenderPass)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdBeginRenderPass) rec->dt.vkCmdBeginRenderPass(commandBuffer, pRenderPassBegin, contents);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdNextSubpass(VkCommandBuffer commandBuffer, VkSubpassContents contents) {
    static_assert(std::is_same_v<PFN_vkCmdNextSubpass, decltype(&V_vkCmdNextSubpass)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdNextSubpass) rec->dt.vkCmdNextSubpass(commandBuffer, contents);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdEndRenderPass(VkCommandBuffer commandBuffer) {
    static_assert(std::is_same_v<PFN_vkCmdEndRenderPass, decltype(&V_vkCmdEndRenderPass)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdEndRenderPass) rec->dt.vkCmdEndRenderPass(commandBuffer);
}
VKAPI_ATTR VkResult VKAPI_CALL V_vkBindBufferMemory2(VkDevice device, uint32_t bindInfoCount, const VkBindBufferMemoryInfo* pBindInfos) {
    static_assert(std::is_same_v<PFN_vkBindBufferMemory2, decltype(&V_vkBindBufferMemory2)>);
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (!rec || !rec->dt.vkBindBufferMemory2) return VK_ERROR_EXTENSION_NOT_PRESENT;
    return rec->dt.vkBindBufferMemory2(device, bindInfoCount, pBindInfos);
}
VKAPI_ATTR VkResult VKAPI_CALL V_vkBindImageMemory2(VkDevice device, uint32_t bindInfoCount, const VkBindImageMemoryInfo* pBindInfos) {
    static_assert(std::is_same_v<PFN_vkBindImageMemory2, decltype(&V_vkBindImageMemory2)>);
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (!rec || !rec->dt.vkBindImageMemory2) return VK_ERROR_EXTENSION_NOT_PRESENT;
    return rec->dt.vkBindImageMemory2(device, bindInfoCount, pBindInfos);
}
VKAPI_ATTR void VKAPI_CALL V_vkGetDeviceGroupPeerMemoryFeatures(VkDevice device, uint32_t heapIndex, uint32_t localDeviceIndex, uint32_t remoteDeviceIndex, VkPeerMemoryFeatureFlags* pPeerMemoryFeatures) {
    static_assert(std::is_same_v<PFN_vkGetDeviceGroupPeerMemoryFeatures, decltype(&V_vkGetDeviceGroupPeerMemoryFeatures)>);
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (rec && rec->dt.vkGetDeviceGroupPeerMemoryFeatures) rec->dt.vkGetDeviceGroupPeerMemoryFeatures(device, heapIndex, localDeviceIndex, remoteDeviceIndex, pPeerMemoryFeatures);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdSetDeviceMask(VkCommandBuffer commandBuffer, uint32_t deviceMask) {
    static_assert(std::is_same_v<PFN_vkCmdSetDeviceMask, decltype(&V_vkCmdSetDeviceMask)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdSetDeviceMask) rec->dt.vkCmdSetDeviceMask(commandBuffer, deviceMask);
}
VKAPI_ATTR void VKAPI_CALL V_vkGetImageMemoryRequirements2(VkDevice device, const VkImageMemoryRequirementsInfo2* pInfo, VkMemoryRequirements2* pMemoryRequirements) {
    static_assert(std::is_same_v<PFN_vkGetImageMemoryRequirements2, decltype(&V_vkGetImageMemoryRequirements2)>);
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (rec && rec->dt.vkGetImageMemoryRequirements2) rec->dt.vkGetImageMemoryRequirements2(device, pInfo, pMemoryRequirements);
}
VKAPI_ATTR void VKAPI_CALL V_vkGetBufferMemoryRequirements2(VkDevice device, const VkBufferMemoryRequirementsInfo2* pInfo, VkMemoryRequirements2* pMemoryRequirements) {
    static_assert(std::is_same_v<PFN_vkGetBufferMemoryRequirements2, decltype(&V_vkGetBufferMemoryRequirements2)>);
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (rec && rec->dt.vkGetBufferMemoryRequirements2) rec->dt.vkGetBufferMemoryRequirements2(device, pInfo, pMemoryRequirements);
}
VKAPI_ATTR void VKAPI_CALL V_vkGetImageSparseMemoryRequirements2(VkDevice device, const VkImageSparseMemoryRequirementsInfo2* pInfo, uint32_t* pSparseMemoryRequirementCount, VkSparseImageMemoryRequirements2* pSparseMemoryRequirements) {
    static_assert(std::is_same_v<PFN_vkGetImageSparseMemoryRequirements2, decltype(&V_vkGetImageSparseMemoryRequirements2)>);
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (rec && rec->dt.vkGetImageSparseMemoryRequirements2) rec->dt.vkGetImageSparseMemoryRequirements2(device, pInfo, pSparseMemoryRequirementCount, pSparseMemoryRequirements);
}
VKAPI_ATTR void VKAPI_CALL V_vkGetPhysicalDeviceFeatures2(VkPhysicalDevice physicalDevice, VkPhysicalDeviceFeatures2* pFeatures) {
    static_assert(std::is_same_v<PFN_vkGetPhysicalDeviceFeatures2, decltype(&V_vkGetPhysicalDeviceFeatures2)>);
    InstanceRecord* rec = LookupPhysicalDeviceInstance(physicalDevice);
    if (!rec) return;
    auto fn = reinterpret_cast<PFN_vkGetPhysicalDeviceFeatures2>(rec->gipa(rec->hostInstance, "vkGetPhysicalDeviceFeatures2"));
    if (fn) fn(physicalDevice, pFeatures);
}
VKAPI_ATTR void VKAPI_CALL V_vkGetPhysicalDeviceProperties2(VkPhysicalDevice physicalDevice, VkPhysicalDeviceProperties2* pProperties) {
    static_assert(std::is_same_v<PFN_vkGetPhysicalDeviceProperties2, decltype(&V_vkGetPhysicalDeviceProperties2)>);
    InstanceRecord* rec = LookupPhysicalDeviceInstance(physicalDevice);
    if (!rec) return;
    auto fn = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties2>(rec->gipa(rec->hostInstance, "vkGetPhysicalDeviceProperties2"));
    if (fn) fn(physicalDevice, pProperties);
}
VKAPI_ATTR void VKAPI_CALL V_vkGetPhysicalDeviceFormatProperties2(VkPhysicalDevice physicalDevice, VkFormat format, VkFormatProperties2* pFormatProperties) {
    static_assert(std::is_same_v<PFN_vkGetPhysicalDeviceFormatProperties2, decltype(&V_vkGetPhysicalDeviceFormatProperties2)>);
    InstanceRecord* rec = LookupPhysicalDeviceInstance(physicalDevice);
    if (!rec) return;
    auto fn = reinterpret_cast<PFN_vkGetPhysicalDeviceFormatProperties2>(rec->gipa(rec->hostInstance, "vkGetPhysicalDeviceFormatProperties2"));
    if (fn) fn(physicalDevice, format, pFormatProperties);
}
VKAPI_ATTR VkResult VKAPI_CALL V_vkGetPhysicalDeviceImageFormatProperties2(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceImageFormatInfo2* pImageFormatInfo, VkImageFormatProperties2* pImageFormatProperties) {
    static_assert(std::is_same_v<PFN_vkGetPhysicalDeviceImageFormatProperties2, decltype(&V_vkGetPhysicalDeviceImageFormatProperties2)>);
    InstanceRecord* rec = LookupPhysicalDeviceInstance(physicalDevice);
    if (!rec) return VK_ERROR_INITIALIZATION_FAILED;
    auto fn = reinterpret_cast<PFN_vkGetPhysicalDeviceImageFormatProperties2>(rec->gipa(rec->hostInstance, "vkGetPhysicalDeviceImageFormatProperties2"));
    return fn ? fn(physicalDevice, pImageFormatInfo, pImageFormatProperties) : VK_ERROR_EXTENSION_NOT_PRESENT;
}
VKAPI_ATTR void VKAPI_CALL V_vkGetPhysicalDeviceQueueFamilyProperties2(VkPhysicalDevice physicalDevice, uint32_t* pQueueFamilyPropertyCount, VkQueueFamilyProperties2* pQueueFamilyProperties) {
    static_assert(std::is_same_v<PFN_vkGetPhysicalDeviceQueueFamilyProperties2, decltype(&V_vkGetPhysicalDeviceQueueFamilyProperties2)>);
    InstanceRecord* rec = LookupPhysicalDeviceInstance(physicalDevice);
    if (!rec) return;
    auto fn = reinterpret_cast<PFN_vkGetPhysicalDeviceQueueFamilyProperties2>(rec->gipa(rec->hostInstance, "vkGetPhysicalDeviceQueueFamilyProperties2"));
    if (fn) fn(physicalDevice, pQueueFamilyPropertyCount, pQueueFamilyProperties);
}
VKAPI_ATTR void VKAPI_CALL V_vkGetPhysicalDeviceMemoryProperties2(VkPhysicalDevice physicalDevice, VkPhysicalDeviceMemoryProperties2* pMemoryProperties) {
    static_assert(std::is_same_v<PFN_vkGetPhysicalDeviceMemoryProperties2, decltype(&V_vkGetPhysicalDeviceMemoryProperties2)>);
    InstanceRecord* rec = LookupPhysicalDeviceInstance(physicalDevice);
    if (!rec) return;
    auto fn = reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties2>(rec->gipa(rec->hostInstance, "vkGetPhysicalDeviceMemoryProperties2"));
    if (fn) fn(physicalDevice, pMemoryProperties);
}
VKAPI_ATTR void VKAPI_CALL V_vkGetPhysicalDeviceSparseImageFormatProperties2(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceSparseImageFormatInfo2* pFormatInfo, uint32_t* pPropertyCount, VkSparseImageFormatProperties2* pProperties) {
    static_assert(std::is_same_v<PFN_vkGetPhysicalDeviceSparseImageFormatProperties2, decltype(&V_vkGetPhysicalDeviceSparseImageFormatProperties2)>);
    InstanceRecord* rec = LookupPhysicalDeviceInstance(physicalDevice);
    if (!rec) return;
    auto fn = reinterpret_cast<PFN_vkGetPhysicalDeviceSparseImageFormatProperties2>(rec->gipa(rec->hostInstance, "vkGetPhysicalDeviceSparseImageFormatProperties2"));
    if (fn) fn(physicalDevice, pFormatInfo, pPropertyCount, pProperties);
}
VKAPI_ATTR void VKAPI_CALL V_vkTrimCommandPool(VkDevice device, VkCommandPool commandPool, VkCommandPoolTrimFlags flags) {
    static_assert(std::is_same_v<PFN_vkTrimCommandPool, decltype(&V_vkTrimCommandPool)>);
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (rec && rec->dt.vkTrimCommandPool) rec->dt.vkTrimCommandPool(device, commandPool, flags);
}
VKAPI_ATTR void VKAPI_CALL V_vkGetPhysicalDeviceExternalBufferProperties(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalBufferInfo* pExternalBufferInfo, VkExternalBufferProperties* pExternalBufferProperties) {
    static_assert(std::is_same_v<PFN_vkGetPhysicalDeviceExternalBufferProperties, decltype(&V_vkGetPhysicalDeviceExternalBufferProperties)>);
    InstanceRecord* rec = LookupPhysicalDeviceInstance(physicalDevice);
    if (!rec) return;
    auto fn = reinterpret_cast<PFN_vkGetPhysicalDeviceExternalBufferProperties>(rec->gipa(rec->hostInstance, "vkGetPhysicalDeviceExternalBufferProperties"));
    if (fn) fn(physicalDevice, pExternalBufferInfo, pExternalBufferProperties);
}
VKAPI_ATTR void VKAPI_CALL V_vkGetPhysicalDeviceExternalFenceProperties(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalFenceInfo* pExternalFenceInfo, VkExternalFenceProperties* pExternalFenceProperties) {
    static_assert(std::is_same_v<PFN_vkGetPhysicalDeviceExternalFenceProperties, decltype(&V_vkGetPhysicalDeviceExternalFenceProperties)>);
    InstanceRecord* rec = LookupPhysicalDeviceInstance(physicalDevice);
    if (!rec) return;
    auto fn = reinterpret_cast<PFN_vkGetPhysicalDeviceExternalFenceProperties>(rec->gipa(rec->hostInstance, "vkGetPhysicalDeviceExternalFenceProperties"));
    if (fn) fn(physicalDevice, pExternalFenceInfo, pExternalFenceProperties);
}
VKAPI_ATTR void VKAPI_CALL V_vkGetPhysicalDeviceExternalSemaphoreProperties(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceExternalSemaphoreInfo* pExternalSemaphoreInfo, VkExternalSemaphoreProperties* pExternalSemaphoreProperties) {
    static_assert(std::is_same_v<PFN_vkGetPhysicalDeviceExternalSemaphoreProperties, decltype(&V_vkGetPhysicalDeviceExternalSemaphoreProperties)>);
    InstanceRecord* rec = LookupPhysicalDeviceInstance(physicalDevice);
    if (!rec) return;
    auto fn = reinterpret_cast<PFN_vkGetPhysicalDeviceExternalSemaphoreProperties>(rec->gipa(rec->hostInstance, "vkGetPhysicalDeviceExternalSemaphoreProperties"));
    if (fn) fn(physicalDevice, pExternalSemaphoreInfo, pExternalSemaphoreProperties);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdDispatchBase(VkCommandBuffer commandBuffer, uint32_t baseGroupX, uint32_t baseGroupY, uint32_t baseGroupZ, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) {
    static_assert(std::is_same_v<PFN_vkCmdDispatchBase, decltype(&V_vkCmdDispatchBase)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdDispatchBase) rec->dt.vkCmdDispatchBase(commandBuffer, baseGroupX, baseGroupY, baseGroupZ, groupCountX, groupCountY, groupCountZ);
}
VKAPI_ATTR VkResult VKAPI_CALL V_vkCreateDescriptorUpdateTemplate(VkDevice device, const VkDescriptorUpdateTemplateCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDescriptorUpdateTemplate* pDescriptorUpdateTemplate) {
    static_assert(std::is_same_v<PFN_vkCreateDescriptorUpdateTemplate, decltype(&V_vkCreateDescriptorUpdateTemplate)>);
    if (pCreateInfo) { VkResult pnextErr = ValidateGuestPNext(pCreateInfo->pNext); if (pnextErr != VK_SUCCESS) return pnextErr; }
    bool allocOk = true;
    const VkAllocationCallbacks* hostAlloc = InternGuestAllocator(pAllocator, allocOk);
    if (!allocOk) return VK_ERROR_FEATURE_NOT_PRESENT;
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (!rec || !rec->dt.vkCreateDescriptorUpdateTemplate) return VK_ERROR_EXTENSION_NOT_PRESENT;
    return rec->dt.vkCreateDescriptorUpdateTemplate(device, pCreateInfo, hostAlloc, pDescriptorUpdateTemplate);
}
VKAPI_ATTR void VKAPI_CALL V_vkDestroyDescriptorUpdateTemplate(VkDevice device, VkDescriptorUpdateTemplate descriptorUpdateTemplate, const VkAllocationCallbacks* pAllocator) {
    static_assert(std::is_same_v<PFN_vkDestroyDescriptorUpdateTemplate, decltype(&V_vkDestroyDescriptorUpdateTemplate)>);
    bool allocOk = true;
    const VkAllocationCallbacks* hostAlloc = InternGuestAllocator(pAllocator, allocOk);
    if (!allocOk) return;
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (rec && rec->dt.vkDestroyDescriptorUpdateTemplate) rec->dt.vkDestroyDescriptorUpdateTemplate(device, descriptorUpdateTemplate, hostAlloc);
}
VKAPI_ATTR void VKAPI_CALL V_vkUpdateDescriptorSetWithTemplate(VkDevice device, VkDescriptorSet descriptorSet, VkDescriptorUpdateTemplate descriptorUpdateTemplate, const void* pData) {
    static_assert(std::is_same_v<PFN_vkUpdateDescriptorSetWithTemplate, decltype(&V_vkUpdateDescriptorSetWithTemplate)>);
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (rec && rec->dt.vkUpdateDescriptorSetWithTemplate) rec->dt.vkUpdateDescriptorSetWithTemplate(device, descriptorSet, descriptorUpdateTemplate, pData);
}
VKAPI_ATTR void VKAPI_CALL V_vkGetDescriptorSetLayoutSupport(VkDevice device, const VkDescriptorSetLayoutCreateInfo* pCreateInfo, VkDescriptorSetLayoutSupport* pSupport) {
    static_assert(std::is_same_v<PFN_vkGetDescriptorSetLayoutSupport, decltype(&V_vkGetDescriptorSetLayoutSupport)>);
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (rec && rec->dt.vkGetDescriptorSetLayoutSupport) rec->dt.vkGetDescriptorSetLayoutSupport(device, pCreateInfo, pSupport);
}
VKAPI_ATTR VkResult VKAPI_CALL V_vkCreateSamplerYcbcrConversion(VkDevice device, const VkSamplerYcbcrConversionCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSamplerYcbcrConversion* pYcbcrConversion) {
    static_assert(std::is_same_v<PFN_vkCreateSamplerYcbcrConversion, decltype(&V_vkCreateSamplerYcbcrConversion)>);
    if (pCreateInfo) { VkResult pnextErr = ValidateGuestPNext(pCreateInfo->pNext); if (pnextErr != VK_SUCCESS) return pnextErr; }
    bool allocOk = true;
    const VkAllocationCallbacks* hostAlloc = InternGuestAllocator(pAllocator, allocOk);
    if (!allocOk) return VK_ERROR_FEATURE_NOT_PRESENT;
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (!rec || !rec->dt.vkCreateSamplerYcbcrConversion) return VK_ERROR_EXTENSION_NOT_PRESENT;
    return rec->dt.vkCreateSamplerYcbcrConversion(device, pCreateInfo, hostAlloc, pYcbcrConversion);
}
VKAPI_ATTR void VKAPI_CALL V_vkDestroySamplerYcbcrConversion(VkDevice device, VkSamplerYcbcrConversion ycbcrConversion, const VkAllocationCallbacks* pAllocator) {
    static_assert(std::is_same_v<PFN_vkDestroySamplerYcbcrConversion, decltype(&V_vkDestroySamplerYcbcrConversion)>);
    bool allocOk = true;
    const VkAllocationCallbacks* hostAlloc = InternGuestAllocator(pAllocator, allocOk);
    if (!allocOk) return;
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (rec && rec->dt.vkDestroySamplerYcbcrConversion) rec->dt.vkDestroySamplerYcbcrConversion(device, ycbcrConversion, hostAlloc);
}
VKAPI_ATTR void VKAPI_CALL V_vkResetQueryPool(VkDevice device, VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount) {
    static_assert(std::is_same_v<PFN_vkResetQueryPool, decltype(&V_vkResetQueryPool)>);
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (rec && rec->dt.vkResetQueryPool) rec->dt.vkResetQueryPool(device, queryPool, firstQuery, queryCount);
}
VKAPI_ATTR VkResult VKAPI_CALL V_vkGetSemaphoreCounterValue(VkDevice device, VkSemaphore semaphore, uint64_t* pValue) {
    static_assert(std::is_same_v<PFN_vkGetSemaphoreCounterValue, decltype(&V_vkGetSemaphoreCounterValue)>);
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (!rec || !rec->dt.vkGetSemaphoreCounterValue) return VK_ERROR_EXTENSION_NOT_PRESENT;
    return rec->dt.vkGetSemaphoreCounterValue(device, semaphore, pValue);
}
VKAPI_ATTR VkResult VKAPI_CALL V_vkWaitSemaphores(VkDevice device, const VkSemaphoreWaitInfo* pWaitInfo, uint64_t timeout) {
    static_assert(std::is_same_v<PFN_vkWaitSemaphores, decltype(&V_vkWaitSemaphores)>);
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (!rec || !rec->dt.vkWaitSemaphores) return VK_ERROR_EXTENSION_NOT_PRESENT;
    return rec->dt.vkWaitSemaphores(device, pWaitInfo, timeout);
}
VKAPI_ATTR VkResult VKAPI_CALL V_vkSignalSemaphore(VkDevice device, const VkSemaphoreSignalInfo* pSignalInfo) {
    static_assert(std::is_same_v<PFN_vkSignalSemaphore, decltype(&V_vkSignalSemaphore)>);
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (!rec || !rec->dt.vkSignalSemaphore) return VK_ERROR_EXTENSION_NOT_PRESENT;
    return rec->dt.vkSignalSemaphore(device, pSignalInfo);
}
VKAPI_ATTR VkDeviceAddress VKAPI_CALL V_vkGetBufferDeviceAddress(VkDevice device, const VkBufferDeviceAddressInfo* pInfo) {
    static_assert(std::is_same_v<PFN_vkGetBufferDeviceAddress, decltype(&V_vkGetBufferDeviceAddress)>);
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (!rec || !rec->dt.vkGetBufferDeviceAddress) return static_cast<VkDeviceAddress>(0);
    return rec->dt.vkGetBufferDeviceAddress(device, pInfo);
}
VKAPI_ATTR uint64_t VKAPI_CALL V_vkGetBufferOpaqueCaptureAddress(VkDevice device, const VkBufferDeviceAddressInfo* pInfo) {
    static_assert(std::is_same_v<PFN_vkGetBufferOpaqueCaptureAddress, decltype(&V_vkGetBufferOpaqueCaptureAddress)>);
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (!rec || !rec->dt.vkGetBufferOpaqueCaptureAddress) return static_cast<uint64_t>(0);
    return rec->dt.vkGetBufferOpaqueCaptureAddress(device, pInfo);
}
VKAPI_ATTR uint64_t VKAPI_CALL V_vkGetDeviceMemoryOpaqueCaptureAddress(VkDevice device, const VkDeviceMemoryOpaqueCaptureAddressInfo* pInfo) {
    static_assert(std::is_same_v<PFN_vkGetDeviceMemoryOpaqueCaptureAddress, decltype(&V_vkGetDeviceMemoryOpaqueCaptureAddress)>);
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (!rec || !rec->dt.vkGetDeviceMemoryOpaqueCaptureAddress) return static_cast<uint64_t>(0);
    return rec->dt.vkGetDeviceMemoryOpaqueCaptureAddress(device, pInfo);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdDrawIndirectCount(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride) {
    static_assert(std::is_same_v<PFN_vkCmdDrawIndirectCount, decltype(&V_vkCmdDrawIndirectCount)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdDrawIndirectCount) rec->dt.vkCmdDrawIndirectCount(commandBuffer, buffer, offset, countBuffer, countBufferOffset, maxDrawCount, stride);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdDrawIndexedIndirectCount(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride) {
    static_assert(std::is_same_v<PFN_vkCmdDrawIndexedIndirectCount, decltype(&V_vkCmdDrawIndexedIndirectCount)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdDrawIndexedIndirectCount) rec->dt.vkCmdDrawIndexedIndirectCount(commandBuffer, buffer, offset, countBuffer, countBufferOffset, maxDrawCount, stride);
}
VKAPI_ATTR VkResult VKAPI_CALL V_vkCreateRenderPass2(VkDevice device, const VkRenderPassCreateInfo2* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkRenderPass* pRenderPass) {
    static_assert(std::is_same_v<PFN_vkCreateRenderPass2, decltype(&V_vkCreateRenderPass2)>);
    if (pCreateInfo) { VkResult pnextErr = ValidateGuestPNext(pCreateInfo->pNext); if (pnextErr != VK_SUCCESS) return pnextErr; }
    bool allocOk = true;
    const VkAllocationCallbacks* hostAlloc = InternGuestAllocator(pAllocator, allocOk);
    if (!allocOk) return VK_ERROR_FEATURE_NOT_PRESENT;
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (!rec || !rec->dt.vkCreateRenderPass2) return VK_ERROR_EXTENSION_NOT_PRESENT;
    return rec->dt.vkCreateRenderPass2(device, pCreateInfo, hostAlloc, pRenderPass);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdBeginRenderPass2(VkCommandBuffer commandBuffer, const VkRenderPassBeginInfo* pRenderPassBegin, const VkSubpassBeginInfo* pSubpassBeginInfo) {
    static_assert(std::is_same_v<PFN_vkCmdBeginRenderPass2, decltype(&V_vkCmdBeginRenderPass2)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdBeginRenderPass2) rec->dt.vkCmdBeginRenderPass2(commandBuffer, pRenderPassBegin, pSubpassBeginInfo);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdNextSubpass2(VkCommandBuffer commandBuffer, const VkSubpassBeginInfo* pSubpassBeginInfo, const VkSubpassEndInfo* pSubpassEndInfo) {
    static_assert(std::is_same_v<PFN_vkCmdNextSubpass2, decltype(&V_vkCmdNextSubpass2)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdNextSubpass2) rec->dt.vkCmdNextSubpass2(commandBuffer, pSubpassBeginInfo, pSubpassEndInfo);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdEndRenderPass2(VkCommandBuffer commandBuffer, const VkSubpassEndInfo* pSubpassEndInfo) {
    static_assert(std::is_same_v<PFN_vkCmdEndRenderPass2, decltype(&V_vkCmdEndRenderPass2)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdEndRenderPass2) rec->dt.vkCmdEndRenderPass2(commandBuffer, pSubpassEndInfo);
}
VKAPI_ATTR VkResult VKAPI_CALL V_vkGetPhysicalDeviceToolProperties(VkPhysicalDevice physicalDevice, uint32_t* pToolCount, VkPhysicalDeviceToolProperties* pToolProperties) {
    static_assert(std::is_same_v<PFN_vkGetPhysicalDeviceToolProperties, decltype(&V_vkGetPhysicalDeviceToolProperties)>);
    InstanceRecord* rec = LookupPhysicalDeviceInstance(physicalDevice);
    if (!rec) return VK_ERROR_INITIALIZATION_FAILED;
    auto fn = reinterpret_cast<PFN_vkGetPhysicalDeviceToolProperties>(rec->gipa(rec->hostInstance, "vkGetPhysicalDeviceToolProperties"));
    return fn ? fn(physicalDevice, pToolCount, pToolProperties) : VK_ERROR_EXTENSION_NOT_PRESENT;
}
VKAPI_ATTR VkResult VKAPI_CALL V_vkCreatePrivateDataSlot(VkDevice device, const VkPrivateDataSlotCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkPrivateDataSlot* pPrivateDataSlot) {
    static_assert(std::is_same_v<PFN_vkCreatePrivateDataSlot, decltype(&V_vkCreatePrivateDataSlot)>);
    if (pCreateInfo) { VkResult pnextErr = ValidateGuestPNext(pCreateInfo->pNext); if (pnextErr != VK_SUCCESS) return pnextErr; }
    bool allocOk = true;
    const VkAllocationCallbacks* hostAlloc = InternGuestAllocator(pAllocator, allocOk);
    if (!allocOk) return VK_ERROR_FEATURE_NOT_PRESENT;
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (!rec || !rec->dt.vkCreatePrivateDataSlot) return VK_ERROR_EXTENSION_NOT_PRESENT;
    return rec->dt.vkCreatePrivateDataSlot(device, pCreateInfo, hostAlloc, pPrivateDataSlot);
}
VKAPI_ATTR void VKAPI_CALL V_vkDestroyPrivateDataSlot(VkDevice device, VkPrivateDataSlot privateDataSlot, const VkAllocationCallbacks* pAllocator) {
    static_assert(std::is_same_v<PFN_vkDestroyPrivateDataSlot, decltype(&V_vkDestroyPrivateDataSlot)>);
    bool allocOk = true;
    const VkAllocationCallbacks* hostAlloc = InternGuestAllocator(pAllocator, allocOk);
    if (!allocOk) return;
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (rec && rec->dt.vkDestroyPrivateDataSlot) rec->dt.vkDestroyPrivateDataSlot(device, privateDataSlot, hostAlloc);
}
VKAPI_ATTR VkResult VKAPI_CALL V_vkSetPrivateData(VkDevice device, VkObjectType objectType, uint64_t objectHandle, VkPrivateDataSlot privateDataSlot, uint64_t data) {
    static_assert(std::is_same_v<PFN_vkSetPrivateData, decltype(&V_vkSetPrivateData)>);
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (!rec || !rec->dt.vkSetPrivateData) return VK_ERROR_EXTENSION_NOT_PRESENT;
    return rec->dt.vkSetPrivateData(device, objectType, objectHandle, privateDataSlot, data);
}
VKAPI_ATTR void VKAPI_CALL V_vkGetPrivateData(VkDevice device, VkObjectType objectType, uint64_t objectHandle, VkPrivateDataSlot privateDataSlot, uint64_t* pData) {
    static_assert(std::is_same_v<PFN_vkGetPrivateData, decltype(&V_vkGetPrivateData)>);
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (rec && rec->dt.vkGetPrivateData) rec->dt.vkGetPrivateData(device, objectType, objectHandle, privateDataSlot, pData);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdPipelineBarrier2(VkCommandBuffer commandBuffer, const VkDependencyInfo* pDependencyInfo) {
    static_assert(std::is_same_v<PFN_vkCmdPipelineBarrier2, decltype(&V_vkCmdPipelineBarrier2)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdPipelineBarrier2) rec->dt.vkCmdPipelineBarrier2(commandBuffer, pDependencyInfo);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdWriteTimestamp2(VkCommandBuffer commandBuffer, VkPipelineStageFlags2 stage, VkQueryPool queryPool, uint32_t query) {
    static_assert(std::is_same_v<PFN_vkCmdWriteTimestamp2, decltype(&V_vkCmdWriteTimestamp2)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdWriteTimestamp2) rec->dt.vkCmdWriteTimestamp2(commandBuffer, stage, queryPool, query);
}
VKAPI_ATTR VkResult VKAPI_CALL V_vkQueueSubmit2(VkQueue queue, uint32_t submitCount, const VkSubmitInfo2* pSubmits, VkFence fence) {
    static_assert(std::is_same_v<PFN_vkQueueSubmit2, decltype(&V_vkQueueSubmit2)>);
    DeviceRecord* rec = LookupQueueDevice(queue);
    if (!rec || !rec->dt.vkQueueSubmit2) return VK_ERROR_EXTENSION_NOT_PRESENT;
    return rec->dt.vkQueueSubmit2(queue, submitCount, pSubmits, fence);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdCopyBuffer2(VkCommandBuffer commandBuffer, const VkCopyBufferInfo2* pCopyBufferInfo) {
    static_assert(std::is_same_v<PFN_vkCmdCopyBuffer2, decltype(&V_vkCmdCopyBuffer2)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdCopyBuffer2) rec->dt.vkCmdCopyBuffer2(commandBuffer, pCopyBufferInfo);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdCopyImage2(VkCommandBuffer commandBuffer, const VkCopyImageInfo2* pCopyImageInfo) {
    static_assert(std::is_same_v<PFN_vkCmdCopyImage2, decltype(&V_vkCmdCopyImage2)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdCopyImage2) rec->dt.vkCmdCopyImage2(commandBuffer, pCopyImageInfo);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdCopyBufferToImage2(VkCommandBuffer commandBuffer, const VkCopyBufferToImageInfo2* pCopyBufferToImageInfo) {
    static_assert(std::is_same_v<PFN_vkCmdCopyBufferToImage2, decltype(&V_vkCmdCopyBufferToImage2)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdCopyBufferToImage2) rec->dt.vkCmdCopyBufferToImage2(commandBuffer, pCopyBufferToImageInfo);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdCopyImageToBuffer2(VkCommandBuffer commandBuffer, const VkCopyImageToBufferInfo2* pCopyImageToBufferInfo) {
    static_assert(std::is_same_v<PFN_vkCmdCopyImageToBuffer2, decltype(&V_vkCmdCopyImageToBuffer2)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdCopyImageToBuffer2) rec->dt.vkCmdCopyImageToBuffer2(commandBuffer, pCopyImageToBufferInfo);
}
VKAPI_ATTR void VKAPI_CALL V_vkGetDeviceBufferMemoryRequirements(VkDevice device, const VkDeviceBufferMemoryRequirements* pInfo, VkMemoryRequirements2* pMemoryRequirements) {
    static_assert(std::is_same_v<PFN_vkGetDeviceBufferMemoryRequirements, decltype(&V_vkGetDeviceBufferMemoryRequirements)>);
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (rec && rec->dt.vkGetDeviceBufferMemoryRequirements) rec->dt.vkGetDeviceBufferMemoryRequirements(device, pInfo, pMemoryRequirements);
}
VKAPI_ATTR void VKAPI_CALL V_vkGetDeviceImageMemoryRequirements(VkDevice device, const VkDeviceImageMemoryRequirements* pInfo, VkMemoryRequirements2* pMemoryRequirements) {
    static_assert(std::is_same_v<PFN_vkGetDeviceImageMemoryRequirements, decltype(&V_vkGetDeviceImageMemoryRequirements)>);
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (rec && rec->dt.vkGetDeviceImageMemoryRequirements) rec->dt.vkGetDeviceImageMemoryRequirements(device, pInfo, pMemoryRequirements);
}
VKAPI_ATTR void VKAPI_CALL V_vkGetDeviceImageSparseMemoryRequirements(VkDevice device, const VkDeviceImageMemoryRequirements* pInfo, uint32_t* pSparseMemoryRequirementCount, VkSparseImageMemoryRequirements2* pSparseMemoryRequirements) {
    static_assert(std::is_same_v<PFN_vkGetDeviceImageSparseMemoryRequirements, decltype(&V_vkGetDeviceImageSparseMemoryRequirements)>);
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (rec && rec->dt.vkGetDeviceImageSparseMemoryRequirements) rec->dt.vkGetDeviceImageSparseMemoryRequirements(device, pInfo, pSparseMemoryRequirementCount, pSparseMemoryRequirements);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdSetEvent2(VkCommandBuffer commandBuffer, VkEvent event, const VkDependencyInfo* pDependencyInfo) {
    static_assert(std::is_same_v<PFN_vkCmdSetEvent2, decltype(&V_vkCmdSetEvent2)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdSetEvent2) rec->dt.vkCmdSetEvent2(commandBuffer, event, pDependencyInfo);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdResetEvent2(VkCommandBuffer commandBuffer, VkEvent event, VkPipelineStageFlags2 stageMask) {
    static_assert(std::is_same_v<PFN_vkCmdResetEvent2, decltype(&V_vkCmdResetEvent2)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdResetEvent2) rec->dt.vkCmdResetEvent2(commandBuffer, event, stageMask);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdWaitEvents2(VkCommandBuffer commandBuffer, uint32_t eventCount, const VkEvent* pEvents, const VkDependencyInfo* pDependencyInfos) {
    static_assert(std::is_same_v<PFN_vkCmdWaitEvents2, decltype(&V_vkCmdWaitEvents2)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdWaitEvents2) rec->dt.vkCmdWaitEvents2(commandBuffer, eventCount, pEvents, pDependencyInfos);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdBlitImage2(VkCommandBuffer commandBuffer, const VkBlitImageInfo2* pBlitImageInfo) {
    static_assert(std::is_same_v<PFN_vkCmdBlitImage2, decltype(&V_vkCmdBlitImage2)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdBlitImage2) rec->dt.vkCmdBlitImage2(commandBuffer, pBlitImageInfo);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdResolveImage2(VkCommandBuffer commandBuffer, const VkResolveImageInfo2* pResolveImageInfo) {
    static_assert(std::is_same_v<PFN_vkCmdResolveImage2, decltype(&V_vkCmdResolveImage2)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdResolveImage2) rec->dt.vkCmdResolveImage2(commandBuffer, pResolveImageInfo);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdBeginRendering(VkCommandBuffer commandBuffer, const VkRenderingInfo* pRenderingInfo) {
    static_assert(std::is_same_v<PFN_vkCmdBeginRendering, decltype(&V_vkCmdBeginRendering)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdBeginRendering) rec->dt.vkCmdBeginRendering(commandBuffer, pRenderingInfo);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdEndRendering(VkCommandBuffer commandBuffer) {
    static_assert(std::is_same_v<PFN_vkCmdEndRendering, decltype(&V_vkCmdEndRendering)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdEndRendering) rec->dt.vkCmdEndRendering(commandBuffer);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdSetCullMode(VkCommandBuffer commandBuffer, VkCullModeFlags cullMode) {
    static_assert(std::is_same_v<PFN_vkCmdSetCullMode, decltype(&V_vkCmdSetCullMode)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdSetCullMode) rec->dt.vkCmdSetCullMode(commandBuffer, cullMode);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdSetFrontFace(VkCommandBuffer commandBuffer, VkFrontFace frontFace) {
    static_assert(std::is_same_v<PFN_vkCmdSetFrontFace, decltype(&V_vkCmdSetFrontFace)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdSetFrontFace) rec->dt.vkCmdSetFrontFace(commandBuffer, frontFace);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdSetPrimitiveTopology(VkCommandBuffer commandBuffer, VkPrimitiveTopology primitiveTopology) {
    static_assert(std::is_same_v<PFN_vkCmdSetPrimitiveTopology, decltype(&V_vkCmdSetPrimitiveTopology)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdSetPrimitiveTopology) rec->dt.vkCmdSetPrimitiveTopology(commandBuffer, primitiveTopology);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdSetViewportWithCount(VkCommandBuffer commandBuffer, uint32_t viewportCount, const VkViewport* pViewports) {
    static_assert(std::is_same_v<PFN_vkCmdSetViewportWithCount, decltype(&V_vkCmdSetViewportWithCount)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdSetViewportWithCount) rec->dt.vkCmdSetViewportWithCount(commandBuffer, viewportCount, pViewports);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdSetScissorWithCount(VkCommandBuffer commandBuffer, uint32_t scissorCount, const VkRect2D* pScissors) {
    static_assert(std::is_same_v<PFN_vkCmdSetScissorWithCount, decltype(&V_vkCmdSetScissorWithCount)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdSetScissorWithCount) rec->dt.vkCmdSetScissorWithCount(commandBuffer, scissorCount, pScissors);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdBindVertexBuffers2(VkCommandBuffer commandBuffer, uint32_t firstBinding, uint32_t bindingCount, const VkBuffer* pBuffers, const VkDeviceSize* pOffsets, const VkDeviceSize* pSizes, const VkDeviceSize* pStrides) {
    static_assert(std::is_same_v<PFN_vkCmdBindVertexBuffers2, decltype(&V_vkCmdBindVertexBuffers2)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdBindVertexBuffers2) rec->dt.vkCmdBindVertexBuffers2(commandBuffer, firstBinding, bindingCount, pBuffers, pOffsets, pSizes, pStrides);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdSetDepthTestEnable(VkCommandBuffer commandBuffer, VkBool32 depthTestEnable) {
    static_assert(std::is_same_v<PFN_vkCmdSetDepthTestEnable, decltype(&V_vkCmdSetDepthTestEnable)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdSetDepthTestEnable) rec->dt.vkCmdSetDepthTestEnable(commandBuffer, depthTestEnable);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdSetDepthWriteEnable(VkCommandBuffer commandBuffer, VkBool32 depthWriteEnable) {
    static_assert(std::is_same_v<PFN_vkCmdSetDepthWriteEnable, decltype(&V_vkCmdSetDepthWriteEnable)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdSetDepthWriteEnable) rec->dt.vkCmdSetDepthWriteEnable(commandBuffer, depthWriteEnable);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdSetDepthCompareOp(VkCommandBuffer commandBuffer, VkCompareOp depthCompareOp) {
    static_assert(std::is_same_v<PFN_vkCmdSetDepthCompareOp, decltype(&V_vkCmdSetDepthCompareOp)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdSetDepthCompareOp) rec->dt.vkCmdSetDepthCompareOp(commandBuffer, depthCompareOp);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdSetDepthBoundsTestEnable(VkCommandBuffer commandBuffer, VkBool32 depthBoundsTestEnable) {
    static_assert(std::is_same_v<PFN_vkCmdSetDepthBoundsTestEnable, decltype(&V_vkCmdSetDepthBoundsTestEnable)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdSetDepthBoundsTestEnable) rec->dt.vkCmdSetDepthBoundsTestEnable(commandBuffer, depthBoundsTestEnable);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdSetStencilTestEnable(VkCommandBuffer commandBuffer, VkBool32 stencilTestEnable) {
    static_assert(std::is_same_v<PFN_vkCmdSetStencilTestEnable, decltype(&V_vkCmdSetStencilTestEnable)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdSetStencilTestEnable) rec->dt.vkCmdSetStencilTestEnable(commandBuffer, stencilTestEnable);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdSetStencilOp(VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, VkStencilOp failOp, VkStencilOp passOp, VkStencilOp depthFailOp, VkCompareOp compareOp) {
    static_assert(std::is_same_v<PFN_vkCmdSetStencilOp, decltype(&V_vkCmdSetStencilOp)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdSetStencilOp) rec->dt.vkCmdSetStencilOp(commandBuffer, faceMask, failOp, passOp, depthFailOp, compareOp);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdSetRasterizerDiscardEnable(VkCommandBuffer commandBuffer, VkBool32 rasterizerDiscardEnable) {
    static_assert(std::is_same_v<PFN_vkCmdSetRasterizerDiscardEnable, decltype(&V_vkCmdSetRasterizerDiscardEnable)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdSetRasterizerDiscardEnable) rec->dt.vkCmdSetRasterizerDiscardEnable(commandBuffer, rasterizerDiscardEnable);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdSetDepthBiasEnable(VkCommandBuffer commandBuffer, VkBool32 depthBiasEnable) {
    static_assert(std::is_same_v<PFN_vkCmdSetDepthBiasEnable, decltype(&V_vkCmdSetDepthBiasEnable)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdSetDepthBiasEnable) rec->dt.vkCmdSetDepthBiasEnable(commandBuffer, depthBiasEnable);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdSetPrimitiveRestartEnable(VkCommandBuffer commandBuffer, VkBool32 primitiveRestartEnable) {
    static_assert(std::is_same_v<PFN_vkCmdSetPrimitiveRestartEnable, decltype(&V_vkCmdSetPrimitiveRestartEnable)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdSetPrimitiveRestartEnable) rec->dt.vkCmdSetPrimitiveRestartEnable(commandBuffer, primitiveRestartEnable);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdBeginRenderingKHR(VkCommandBuffer commandBuffer, const VkRenderingInfo* pRenderingInfo) {
    static_assert(std::is_same_v<PFN_vkCmdBeginRenderingKHR, decltype(&V_vkCmdBeginRenderingKHR)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdBeginRenderingKHR) rec->dt.vkCmdBeginRenderingKHR(commandBuffer, pRenderingInfo);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdEndRenderingKHR(VkCommandBuffer commandBuffer) {
    static_assert(std::is_same_v<PFN_vkCmdEndRenderingKHR, decltype(&V_vkCmdEndRenderingKHR)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdEndRenderingKHR) rec->dt.vkCmdEndRenderingKHR(commandBuffer);
}
VKAPI_ATTR void VKAPI_CALL V_vkGetPhysicalDeviceFeatures2KHR(VkPhysicalDevice physicalDevice, VkPhysicalDeviceFeatures2* pFeatures) {
    static_assert(std::is_same_v<PFN_vkGetPhysicalDeviceFeatures2KHR, decltype(&V_vkGetPhysicalDeviceFeatures2KHR)>);
    InstanceRecord* rec = LookupPhysicalDeviceInstance(physicalDevice);
    if (!rec) return;
    auto fn = reinterpret_cast<PFN_vkGetPhysicalDeviceFeatures2KHR>(rec->gipa(rec->hostInstance, "vkGetPhysicalDeviceFeatures2KHR"));
    if (fn) fn(physicalDevice, pFeatures);
}
VKAPI_ATTR void VKAPI_CALL V_vkGetPhysicalDeviceProperties2KHR(VkPhysicalDevice physicalDevice, VkPhysicalDeviceProperties2* pProperties) {
    static_assert(std::is_same_v<PFN_vkGetPhysicalDeviceProperties2KHR, decltype(&V_vkGetPhysicalDeviceProperties2KHR)>);
    InstanceRecord* rec = LookupPhysicalDeviceInstance(physicalDevice);
    if (!rec) return;
    auto fn = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties2KHR>(rec->gipa(rec->hostInstance, "vkGetPhysicalDeviceProperties2KHR"));
    if (fn) fn(physicalDevice, pProperties);
}
VKAPI_ATTR void VKAPI_CALL V_vkGetPhysicalDeviceFormatProperties2KHR(VkPhysicalDevice physicalDevice, VkFormat format, VkFormatProperties2* pFormatProperties) {
    static_assert(std::is_same_v<PFN_vkGetPhysicalDeviceFormatProperties2KHR, decltype(&V_vkGetPhysicalDeviceFormatProperties2KHR)>);
    InstanceRecord* rec = LookupPhysicalDeviceInstance(physicalDevice);
    if (!rec) return;
    auto fn = reinterpret_cast<PFN_vkGetPhysicalDeviceFormatProperties2KHR>(rec->gipa(rec->hostInstance, "vkGetPhysicalDeviceFormatProperties2KHR"));
    if (fn) fn(physicalDevice, format, pFormatProperties);
}
VKAPI_ATTR VkResult VKAPI_CALL V_vkGetPhysicalDeviceImageFormatProperties2KHR(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceImageFormatInfo2* pImageFormatInfo, VkImageFormatProperties2* pImageFormatProperties) {
    static_assert(std::is_same_v<PFN_vkGetPhysicalDeviceImageFormatProperties2KHR, decltype(&V_vkGetPhysicalDeviceImageFormatProperties2KHR)>);
    InstanceRecord* rec = LookupPhysicalDeviceInstance(physicalDevice);
    if (!rec) return VK_ERROR_INITIALIZATION_FAILED;
    auto fn = reinterpret_cast<PFN_vkGetPhysicalDeviceImageFormatProperties2KHR>(rec->gipa(rec->hostInstance, "vkGetPhysicalDeviceImageFormatProperties2KHR"));
    return fn ? fn(physicalDevice, pImageFormatInfo, pImageFormatProperties) : VK_ERROR_EXTENSION_NOT_PRESENT;
}
VKAPI_ATTR void VKAPI_CALL V_vkGetPhysicalDeviceQueueFamilyProperties2KHR(VkPhysicalDevice physicalDevice, uint32_t* pQueueFamilyPropertyCount, VkQueueFamilyProperties2* pQueueFamilyProperties) {
    static_assert(std::is_same_v<PFN_vkGetPhysicalDeviceQueueFamilyProperties2KHR, decltype(&V_vkGetPhysicalDeviceQueueFamilyProperties2KHR)>);
    InstanceRecord* rec = LookupPhysicalDeviceInstance(physicalDevice);
    if (!rec) return;
    auto fn = reinterpret_cast<PFN_vkGetPhysicalDeviceQueueFamilyProperties2KHR>(rec->gipa(rec->hostInstance, "vkGetPhysicalDeviceQueueFamilyProperties2KHR"));
    if (fn) fn(physicalDevice, pQueueFamilyPropertyCount, pQueueFamilyProperties);
}
VKAPI_ATTR void VKAPI_CALL V_vkGetPhysicalDeviceMemoryProperties2KHR(VkPhysicalDevice physicalDevice, VkPhysicalDeviceMemoryProperties2* pMemoryProperties) {
    static_assert(std::is_same_v<PFN_vkGetPhysicalDeviceMemoryProperties2KHR, decltype(&V_vkGetPhysicalDeviceMemoryProperties2KHR)>);
    InstanceRecord* rec = LookupPhysicalDeviceInstance(physicalDevice);
    if (!rec) return;
    auto fn = reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties2KHR>(rec->gipa(rec->hostInstance, "vkGetPhysicalDeviceMemoryProperties2KHR"));
    if (fn) fn(physicalDevice, pMemoryProperties);
}
VKAPI_ATTR void VKAPI_CALL V_vkGetPhysicalDeviceSparseImageFormatProperties2KHR(VkPhysicalDevice physicalDevice, const VkPhysicalDeviceSparseImageFormatInfo2* pFormatInfo, uint32_t* pPropertyCount, VkSparseImageFormatProperties2* pProperties) {
    static_assert(std::is_same_v<PFN_vkGetPhysicalDeviceSparseImageFormatProperties2KHR, decltype(&V_vkGetPhysicalDeviceSparseImageFormatProperties2KHR)>);
    InstanceRecord* rec = LookupPhysicalDeviceInstance(physicalDevice);
    if (!rec) return;
    auto fn = reinterpret_cast<PFN_vkGetPhysicalDeviceSparseImageFormatProperties2KHR>(rec->gipa(rec->hostInstance, "vkGetPhysicalDeviceSparseImageFormatProperties2KHR"));
    if (fn) fn(physicalDevice, pFormatInfo, pPropertyCount, pProperties);
}
VKAPI_ATTR void VKAPI_CALL V_vkTrimCommandPoolKHR(VkDevice device, VkCommandPool commandPool, VkCommandPoolTrimFlags flags) {
    static_assert(std::is_same_v<PFN_vkTrimCommandPoolKHR, decltype(&V_vkTrimCommandPoolKHR)>);
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (rec && rec->dt.vkTrimCommandPoolKHR) rec->dt.vkTrimCommandPoolKHR(device, commandPool, flags);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdPushDescriptorSetKHR(VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint, VkPipelineLayout layout, uint32_t set, uint32_t descriptorWriteCount, const VkWriteDescriptorSet* pDescriptorWrites) {
    static_assert(std::is_same_v<PFN_vkCmdPushDescriptorSetKHR, decltype(&V_vkCmdPushDescriptorSetKHR)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdPushDescriptorSetKHR) rec->dt.vkCmdPushDescriptorSetKHR(commandBuffer, pipelineBindPoint, layout, set, descriptorWriteCount, pDescriptorWrites);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdPushDescriptorSetWithTemplateKHR(VkCommandBuffer commandBuffer, VkDescriptorUpdateTemplate descriptorUpdateTemplate, VkPipelineLayout layout, uint32_t set, const void* pData) {
    static_assert(std::is_same_v<PFN_vkCmdPushDescriptorSetWithTemplateKHR, decltype(&V_vkCmdPushDescriptorSetWithTemplateKHR)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdPushDescriptorSetWithTemplateKHR) rec->dt.vkCmdPushDescriptorSetWithTemplateKHR(commandBuffer, descriptorUpdateTemplate, layout, set, pData);
}
VKAPI_ATTR VkResult VKAPI_CALL V_vkCreateDescriptorUpdateTemplateKHR(VkDevice device, const VkDescriptorUpdateTemplateCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDescriptorUpdateTemplate* pDescriptorUpdateTemplate) {
    static_assert(std::is_same_v<PFN_vkCreateDescriptorUpdateTemplateKHR, decltype(&V_vkCreateDescriptorUpdateTemplateKHR)>);
    if (pCreateInfo) { VkResult pnextErr = ValidateGuestPNext(pCreateInfo->pNext); if (pnextErr != VK_SUCCESS) return pnextErr; }
    bool allocOk = true;
    const VkAllocationCallbacks* hostAlloc = InternGuestAllocator(pAllocator, allocOk);
    if (!allocOk) return VK_ERROR_FEATURE_NOT_PRESENT;
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (!rec || !rec->dt.vkCreateDescriptorUpdateTemplateKHR) return VK_ERROR_EXTENSION_NOT_PRESENT;
    return rec->dt.vkCreateDescriptorUpdateTemplateKHR(device, pCreateInfo, hostAlloc, pDescriptorUpdateTemplate);
}
VKAPI_ATTR void VKAPI_CALL V_vkDestroyDescriptorUpdateTemplateKHR(VkDevice device, VkDescriptorUpdateTemplate descriptorUpdateTemplate, const VkAllocationCallbacks* pAllocator) {
    static_assert(std::is_same_v<PFN_vkDestroyDescriptorUpdateTemplateKHR, decltype(&V_vkDestroyDescriptorUpdateTemplateKHR)>);
    bool allocOk = true;
    const VkAllocationCallbacks* hostAlloc = InternGuestAllocator(pAllocator, allocOk);
    if (!allocOk) return;
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (rec && rec->dt.vkDestroyDescriptorUpdateTemplateKHR) rec->dt.vkDestroyDescriptorUpdateTemplateKHR(device, descriptorUpdateTemplate, hostAlloc);
}
VKAPI_ATTR void VKAPI_CALL V_vkUpdateDescriptorSetWithTemplateKHR(VkDevice device, VkDescriptorSet descriptorSet, VkDescriptorUpdateTemplate descriptorUpdateTemplate, const void* pData) {
    static_assert(std::is_same_v<PFN_vkUpdateDescriptorSetWithTemplateKHR, decltype(&V_vkUpdateDescriptorSetWithTemplateKHR)>);
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (rec && rec->dt.vkUpdateDescriptorSetWithTemplateKHR) rec->dt.vkUpdateDescriptorSetWithTemplateKHR(device, descriptorSet, descriptorUpdateTemplate, pData);
}
VKAPI_ATTR VkResult VKAPI_CALL V_vkCreateRenderPass2KHR(VkDevice device, const VkRenderPassCreateInfo2* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkRenderPass* pRenderPass) {
    static_assert(std::is_same_v<PFN_vkCreateRenderPass2KHR, decltype(&V_vkCreateRenderPass2KHR)>);
    if (pCreateInfo) { VkResult pnextErr = ValidateGuestPNext(pCreateInfo->pNext); if (pnextErr != VK_SUCCESS) return pnextErr; }
    bool allocOk = true;
    const VkAllocationCallbacks* hostAlloc = InternGuestAllocator(pAllocator, allocOk);
    if (!allocOk) return VK_ERROR_FEATURE_NOT_PRESENT;
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (!rec || !rec->dt.vkCreateRenderPass2KHR) return VK_ERROR_EXTENSION_NOT_PRESENT;
    return rec->dt.vkCreateRenderPass2KHR(device, pCreateInfo, hostAlloc, pRenderPass);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdBeginRenderPass2KHR(VkCommandBuffer commandBuffer, const VkRenderPassBeginInfo* pRenderPassBegin, const VkSubpassBeginInfo* pSubpassBeginInfo) {
    static_assert(std::is_same_v<PFN_vkCmdBeginRenderPass2KHR, decltype(&V_vkCmdBeginRenderPass2KHR)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdBeginRenderPass2KHR) rec->dt.vkCmdBeginRenderPass2KHR(commandBuffer, pRenderPassBegin, pSubpassBeginInfo);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdNextSubpass2KHR(VkCommandBuffer commandBuffer, const VkSubpassBeginInfo* pSubpassBeginInfo, const VkSubpassEndInfo* pSubpassEndInfo) {
    static_assert(std::is_same_v<PFN_vkCmdNextSubpass2KHR, decltype(&V_vkCmdNextSubpass2KHR)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdNextSubpass2KHR) rec->dt.vkCmdNextSubpass2KHR(commandBuffer, pSubpassBeginInfo, pSubpassEndInfo);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdEndRenderPass2KHR(VkCommandBuffer commandBuffer, const VkSubpassEndInfo* pSubpassEndInfo) {
    static_assert(std::is_same_v<PFN_vkCmdEndRenderPass2KHR, decltype(&V_vkCmdEndRenderPass2KHR)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdEndRenderPass2KHR) rec->dt.vkCmdEndRenderPass2KHR(commandBuffer, pSubpassEndInfo);
}
VKAPI_ATTR void VKAPI_CALL V_vkGetImageMemoryRequirements2KHR(VkDevice device, const VkImageMemoryRequirementsInfo2* pInfo, VkMemoryRequirements2* pMemoryRequirements) {
    static_assert(std::is_same_v<PFN_vkGetImageMemoryRequirements2KHR, decltype(&V_vkGetImageMemoryRequirements2KHR)>);
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (rec && rec->dt.vkGetImageMemoryRequirements2KHR) rec->dt.vkGetImageMemoryRequirements2KHR(device, pInfo, pMemoryRequirements);
}
VKAPI_ATTR void VKAPI_CALL V_vkGetBufferMemoryRequirements2KHR(VkDevice device, const VkBufferMemoryRequirementsInfo2* pInfo, VkMemoryRequirements2* pMemoryRequirements) {
    static_assert(std::is_same_v<PFN_vkGetBufferMemoryRequirements2KHR, decltype(&V_vkGetBufferMemoryRequirements2KHR)>);
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (rec && rec->dt.vkGetBufferMemoryRequirements2KHR) rec->dt.vkGetBufferMemoryRequirements2KHR(device, pInfo, pMemoryRequirements);
}
VKAPI_ATTR void VKAPI_CALL V_vkGetImageSparseMemoryRequirements2KHR(VkDevice device, const VkImageSparseMemoryRequirementsInfo2* pInfo, uint32_t* pSparseMemoryRequirementCount, VkSparseImageMemoryRequirements2* pSparseMemoryRequirements) {
    static_assert(std::is_same_v<PFN_vkGetImageSparseMemoryRequirements2KHR, decltype(&V_vkGetImageSparseMemoryRequirements2KHR)>);
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (rec && rec->dt.vkGetImageSparseMemoryRequirements2KHR) rec->dt.vkGetImageSparseMemoryRequirements2KHR(device, pInfo, pSparseMemoryRequirementCount, pSparseMemoryRequirements);
}
VKAPI_ATTR VkResult VKAPI_CALL V_vkCreateSamplerYcbcrConversionKHR(VkDevice device, const VkSamplerYcbcrConversionCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSamplerYcbcrConversion* pYcbcrConversion) {
    static_assert(std::is_same_v<PFN_vkCreateSamplerYcbcrConversionKHR, decltype(&V_vkCreateSamplerYcbcrConversionKHR)>);
    if (pCreateInfo) { VkResult pnextErr = ValidateGuestPNext(pCreateInfo->pNext); if (pnextErr != VK_SUCCESS) return pnextErr; }
    bool allocOk = true;
    const VkAllocationCallbacks* hostAlloc = InternGuestAllocator(pAllocator, allocOk);
    if (!allocOk) return VK_ERROR_FEATURE_NOT_PRESENT;
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (!rec || !rec->dt.vkCreateSamplerYcbcrConversionKHR) return VK_ERROR_EXTENSION_NOT_PRESENT;
    return rec->dt.vkCreateSamplerYcbcrConversionKHR(device, pCreateInfo, hostAlloc, pYcbcrConversion);
}
VKAPI_ATTR void VKAPI_CALL V_vkDestroySamplerYcbcrConversionKHR(VkDevice device, VkSamplerYcbcrConversion ycbcrConversion, const VkAllocationCallbacks* pAllocator) {
    static_assert(std::is_same_v<PFN_vkDestroySamplerYcbcrConversionKHR, decltype(&V_vkDestroySamplerYcbcrConversionKHR)>);
    bool allocOk = true;
    const VkAllocationCallbacks* hostAlloc = InternGuestAllocator(pAllocator, allocOk);
    if (!allocOk) return;
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (rec && rec->dt.vkDestroySamplerYcbcrConversionKHR) rec->dt.vkDestroySamplerYcbcrConversionKHR(device, ycbcrConversion, hostAlloc);
}
VKAPI_ATTR VkResult VKAPI_CALL V_vkBindBufferMemory2KHR(VkDevice device, uint32_t bindInfoCount, const VkBindBufferMemoryInfo* pBindInfos) {
    static_assert(std::is_same_v<PFN_vkBindBufferMemory2KHR, decltype(&V_vkBindBufferMemory2KHR)>);
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (!rec || !rec->dt.vkBindBufferMemory2KHR) return VK_ERROR_EXTENSION_NOT_PRESENT;
    return rec->dt.vkBindBufferMemory2KHR(device, bindInfoCount, pBindInfos);
}
VKAPI_ATTR VkResult VKAPI_CALL V_vkBindImageMemory2KHR(VkDevice device, uint32_t bindInfoCount, const VkBindImageMemoryInfo* pBindInfos) {
    static_assert(std::is_same_v<PFN_vkBindImageMemory2KHR, decltype(&V_vkBindImageMemory2KHR)>);
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (!rec || !rec->dt.vkBindImageMemory2KHR) return VK_ERROR_EXTENSION_NOT_PRESENT;
    return rec->dt.vkBindImageMemory2KHR(device, bindInfoCount, pBindInfos);
}
VKAPI_ATTR void VKAPI_CALL V_vkGetDescriptorSetLayoutSupportKHR(VkDevice device, const VkDescriptorSetLayoutCreateInfo* pCreateInfo, VkDescriptorSetLayoutSupport* pSupport) {
    static_assert(std::is_same_v<PFN_vkGetDescriptorSetLayoutSupportKHR, decltype(&V_vkGetDescriptorSetLayoutSupportKHR)>);
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (rec && rec->dt.vkGetDescriptorSetLayoutSupportKHR) rec->dt.vkGetDescriptorSetLayoutSupportKHR(device, pCreateInfo, pSupport);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdDrawIndirectCountKHR(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride) {
    static_assert(std::is_same_v<PFN_vkCmdDrawIndirectCountKHR, decltype(&V_vkCmdDrawIndirectCountKHR)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdDrawIndirectCountKHR) rec->dt.vkCmdDrawIndirectCountKHR(commandBuffer, buffer, offset, countBuffer, countBufferOffset, maxDrawCount, stride);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdDrawIndexedIndirectCountKHR(VkCommandBuffer commandBuffer, VkBuffer buffer, VkDeviceSize offset, VkBuffer countBuffer, VkDeviceSize countBufferOffset, uint32_t maxDrawCount, uint32_t stride) {
    static_assert(std::is_same_v<PFN_vkCmdDrawIndexedIndirectCountKHR, decltype(&V_vkCmdDrawIndexedIndirectCountKHR)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdDrawIndexedIndirectCountKHR) rec->dt.vkCmdDrawIndexedIndirectCountKHR(commandBuffer, buffer, offset, countBuffer, countBufferOffset, maxDrawCount, stride);
}
VKAPI_ATTR VkResult VKAPI_CALL V_vkGetSemaphoreCounterValueKHR(VkDevice device, VkSemaphore semaphore, uint64_t* pValue) {
    static_assert(std::is_same_v<PFN_vkGetSemaphoreCounterValueKHR, decltype(&V_vkGetSemaphoreCounterValueKHR)>);
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (!rec || !rec->dt.vkGetSemaphoreCounterValueKHR) return VK_ERROR_EXTENSION_NOT_PRESENT;
    return rec->dt.vkGetSemaphoreCounterValueKHR(device, semaphore, pValue);
}
VKAPI_ATTR VkResult VKAPI_CALL V_vkWaitSemaphoresKHR(VkDevice device, const VkSemaphoreWaitInfo* pWaitInfo, uint64_t timeout) {
    static_assert(std::is_same_v<PFN_vkWaitSemaphoresKHR, decltype(&V_vkWaitSemaphoresKHR)>);
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (!rec || !rec->dt.vkWaitSemaphoresKHR) return VK_ERROR_EXTENSION_NOT_PRESENT;
    return rec->dt.vkWaitSemaphoresKHR(device, pWaitInfo, timeout);
}
VKAPI_ATTR VkResult VKAPI_CALL V_vkSignalSemaphoreKHR(VkDevice device, const VkSemaphoreSignalInfo* pSignalInfo) {
    static_assert(std::is_same_v<PFN_vkSignalSemaphoreKHR, decltype(&V_vkSignalSemaphoreKHR)>);
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (!rec || !rec->dt.vkSignalSemaphoreKHR) return VK_ERROR_EXTENSION_NOT_PRESENT;
    return rec->dt.vkSignalSemaphoreKHR(device, pSignalInfo);
}
VKAPI_ATTR VkDeviceAddress VKAPI_CALL V_vkGetBufferDeviceAddressKHR(VkDevice device, const VkBufferDeviceAddressInfo* pInfo) {
    static_assert(std::is_same_v<PFN_vkGetBufferDeviceAddressKHR, decltype(&V_vkGetBufferDeviceAddressKHR)>);
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (!rec || !rec->dt.vkGetBufferDeviceAddressKHR) return static_cast<VkDeviceAddress>(0);
    return rec->dt.vkGetBufferDeviceAddressKHR(device, pInfo);
}
VKAPI_ATTR uint64_t VKAPI_CALL V_vkGetBufferOpaqueCaptureAddressKHR(VkDevice device, const VkBufferDeviceAddressInfo* pInfo) {
    static_assert(std::is_same_v<PFN_vkGetBufferOpaqueCaptureAddressKHR, decltype(&V_vkGetBufferOpaqueCaptureAddressKHR)>);
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (!rec || !rec->dt.vkGetBufferOpaqueCaptureAddressKHR) return static_cast<uint64_t>(0);
    return rec->dt.vkGetBufferOpaqueCaptureAddressKHR(device, pInfo);
}
VKAPI_ATTR uint64_t VKAPI_CALL V_vkGetDeviceMemoryOpaqueCaptureAddressKHR(VkDevice device, const VkDeviceMemoryOpaqueCaptureAddressInfo* pInfo) {
    static_assert(std::is_same_v<PFN_vkGetDeviceMemoryOpaqueCaptureAddressKHR, decltype(&V_vkGetDeviceMemoryOpaqueCaptureAddressKHR)>);
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (!rec || !rec->dt.vkGetDeviceMemoryOpaqueCaptureAddressKHR) return static_cast<uint64_t>(0);
    return rec->dt.vkGetDeviceMemoryOpaqueCaptureAddressKHR(device, pInfo);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdSetEvent2KHR(VkCommandBuffer commandBuffer, VkEvent event, const VkDependencyInfo* pDependencyInfo) {
    static_assert(std::is_same_v<PFN_vkCmdSetEvent2KHR, decltype(&V_vkCmdSetEvent2KHR)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdSetEvent2KHR) rec->dt.vkCmdSetEvent2KHR(commandBuffer, event, pDependencyInfo);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdResetEvent2KHR(VkCommandBuffer commandBuffer, VkEvent event, VkPipelineStageFlags2 stageMask) {
    static_assert(std::is_same_v<PFN_vkCmdResetEvent2KHR, decltype(&V_vkCmdResetEvent2KHR)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdResetEvent2KHR) rec->dt.vkCmdResetEvent2KHR(commandBuffer, event, stageMask);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdWaitEvents2KHR(VkCommandBuffer commandBuffer, uint32_t eventCount, const VkEvent* pEvents, const VkDependencyInfo* pDependencyInfos) {
    static_assert(std::is_same_v<PFN_vkCmdWaitEvents2KHR, decltype(&V_vkCmdWaitEvents2KHR)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdWaitEvents2KHR) rec->dt.vkCmdWaitEvents2KHR(commandBuffer, eventCount, pEvents, pDependencyInfos);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdPipelineBarrier2KHR(VkCommandBuffer commandBuffer, const VkDependencyInfo* pDependencyInfo) {
    static_assert(std::is_same_v<PFN_vkCmdPipelineBarrier2KHR, decltype(&V_vkCmdPipelineBarrier2KHR)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdPipelineBarrier2KHR) rec->dt.vkCmdPipelineBarrier2KHR(commandBuffer, pDependencyInfo);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdWriteTimestamp2KHR(VkCommandBuffer commandBuffer, VkPipelineStageFlags2 stage, VkQueryPool queryPool, uint32_t query) {
    static_assert(std::is_same_v<PFN_vkCmdWriteTimestamp2KHR, decltype(&V_vkCmdWriteTimestamp2KHR)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdWriteTimestamp2KHR) rec->dt.vkCmdWriteTimestamp2KHR(commandBuffer, stage, queryPool, query);
}
VKAPI_ATTR VkResult VKAPI_CALL V_vkQueueSubmit2KHR(VkQueue queue, uint32_t submitCount, const VkSubmitInfo2* pSubmits, VkFence fence) {
    static_assert(std::is_same_v<PFN_vkQueueSubmit2KHR, decltype(&V_vkQueueSubmit2KHR)>);
    DeviceRecord* rec = LookupQueueDevice(queue);
    if (!rec || !rec->dt.vkQueueSubmit2KHR) return VK_ERROR_EXTENSION_NOT_PRESENT;
    return rec->dt.vkQueueSubmit2KHR(queue, submitCount, pSubmits, fence);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdCopyBuffer2KHR(VkCommandBuffer commandBuffer, const VkCopyBufferInfo2* pCopyBufferInfo) {
    static_assert(std::is_same_v<PFN_vkCmdCopyBuffer2KHR, decltype(&V_vkCmdCopyBuffer2KHR)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdCopyBuffer2KHR) rec->dt.vkCmdCopyBuffer2KHR(commandBuffer, pCopyBufferInfo);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdCopyImage2KHR(VkCommandBuffer commandBuffer, const VkCopyImageInfo2* pCopyImageInfo) {
    static_assert(std::is_same_v<PFN_vkCmdCopyImage2KHR, decltype(&V_vkCmdCopyImage2KHR)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdCopyImage2KHR) rec->dt.vkCmdCopyImage2KHR(commandBuffer, pCopyImageInfo);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdCopyBufferToImage2KHR(VkCommandBuffer commandBuffer, const VkCopyBufferToImageInfo2* pCopyBufferToImageInfo) {
    static_assert(std::is_same_v<PFN_vkCmdCopyBufferToImage2KHR, decltype(&V_vkCmdCopyBufferToImage2KHR)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdCopyBufferToImage2KHR) rec->dt.vkCmdCopyBufferToImage2KHR(commandBuffer, pCopyBufferToImageInfo);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdCopyImageToBuffer2KHR(VkCommandBuffer commandBuffer, const VkCopyImageToBufferInfo2* pCopyImageToBufferInfo) {
    static_assert(std::is_same_v<PFN_vkCmdCopyImageToBuffer2KHR, decltype(&V_vkCmdCopyImageToBuffer2KHR)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdCopyImageToBuffer2KHR) rec->dt.vkCmdCopyImageToBuffer2KHR(commandBuffer, pCopyImageToBufferInfo);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdBlitImage2KHR(VkCommandBuffer commandBuffer, const VkBlitImageInfo2* pBlitImageInfo) {
    static_assert(std::is_same_v<PFN_vkCmdBlitImage2KHR, decltype(&V_vkCmdBlitImage2KHR)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdBlitImage2KHR) rec->dt.vkCmdBlitImage2KHR(commandBuffer, pBlitImageInfo);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdResolveImage2KHR(VkCommandBuffer commandBuffer, const VkResolveImageInfo2* pResolveImageInfo) {
    static_assert(std::is_same_v<PFN_vkCmdResolveImage2KHR, decltype(&V_vkCmdResolveImage2KHR)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdResolveImage2KHR) rec->dt.vkCmdResolveImage2KHR(commandBuffer, pResolveImageInfo);
}
VKAPI_ATTR void VKAPI_CALL V_vkGetDeviceBufferMemoryRequirementsKHR(VkDevice device, const VkDeviceBufferMemoryRequirements* pInfo, VkMemoryRequirements2* pMemoryRequirements) {
    static_assert(std::is_same_v<PFN_vkGetDeviceBufferMemoryRequirementsKHR, decltype(&V_vkGetDeviceBufferMemoryRequirementsKHR)>);
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (rec && rec->dt.vkGetDeviceBufferMemoryRequirementsKHR) rec->dt.vkGetDeviceBufferMemoryRequirementsKHR(device, pInfo, pMemoryRequirements);
}
VKAPI_ATTR void VKAPI_CALL V_vkGetDeviceImageMemoryRequirementsKHR(VkDevice device, const VkDeviceImageMemoryRequirements* pInfo, VkMemoryRequirements2* pMemoryRequirements) {
    static_assert(std::is_same_v<PFN_vkGetDeviceImageMemoryRequirementsKHR, decltype(&V_vkGetDeviceImageMemoryRequirementsKHR)>);
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (rec && rec->dt.vkGetDeviceImageMemoryRequirementsKHR) rec->dt.vkGetDeviceImageMemoryRequirementsKHR(device, pInfo, pMemoryRequirements);
}
VKAPI_ATTR void VKAPI_CALL V_vkGetDeviceImageSparseMemoryRequirementsKHR(VkDevice device, const VkDeviceImageMemoryRequirements* pInfo, uint32_t* pSparseMemoryRequirementCount, VkSparseImageMemoryRequirements2* pSparseMemoryRequirements) {
    static_assert(std::is_same_v<PFN_vkGetDeviceImageSparseMemoryRequirementsKHR, decltype(&V_vkGetDeviceImageSparseMemoryRequirementsKHR)>);
    DeviceRecord* rec = LookupDeviceRecord(device);
    if (rec && rec->dt.vkGetDeviceImageSparseMemoryRequirementsKHR) rec->dt.vkGetDeviceImageSparseMemoryRequirementsKHR(device, pInfo, pSparseMemoryRequirementCount, pSparseMemoryRequirements);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdSetCullModeEXT(VkCommandBuffer commandBuffer, VkCullModeFlags cullMode) {
    static_assert(std::is_same_v<PFN_vkCmdSetCullModeEXT, decltype(&V_vkCmdSetCullModeEXT)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdSetCullModeEXT) rec->dt.vkCmdSetCullModeEXT(commandBuffer, cullMode);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdSetFrontFaceEXT(VkCommandBuffer commandBuffer, VkFrontFace frontFace) {
    static_assert(std::is_same_v<PFN_vkCmdSetFrontFaceEXT, decltype(&V_vkCmdSetFrontFaceEXT)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdSetFrontFaceEXT) rec->dt.vkCmdSetFrontFaceEXT(commandBuffer, frontFace);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdSetPrimitiveTopologyEXT(VkCommandBuffer commandBuffer, VkPrimitiveTopology primitiveTopology) {
    static_assert(std::is_same_v<PFN_vkCmdSetPrimitiveTopologyEXT, decltype(&V_vkCmdSetPrimitiveTopologyEXT)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdSetPrimitiveTopologyEXT) rec->dt.vkCmdSetPrimitiveTopologyEXT(commandBuffer, primitiveTopology);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdSetViewportWithCountEXT(VkCommandBuffer commandBuffer, uint32_t viewportCount, const VkViewport* pViewports) {
    static_assert(std::is_same_v<PFN_vkCmdSetViewportWithCountEXT, decltype(&V_vkCmdSetViewportWithCountEXT)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdSetViewportWithCountEXT) rec->dt.vkCmdSetViewportWithCountEXT(commandBuffer, viewportCount, pViewports);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdSetScissorWithCountEXT(VkCommandBuffer commandBuffer, uint32_t scissorCount, const VkRect2D* pScissors) {
    static_assert(std::is_same_v<PFN_vkCmdSetScissorWithCountEXT, decltype(&V_vkCmdSetScissorWithCountEXT)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdSetScissorWithCountEXT) rec->dt.vkCmdSetScissorWithCountEXT(commandBuffer, scissorCount, pScissors);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdBindVertexBuffers2EXT(VkCommandBuffer commandBuffer, uint32_t firstBinding, uint32_t bindingCount, const VkBuffer* pBuffers, const VkDeviceSize* pOffsets, const VkDeviceSize* pSizes, const VkDeviceSize* pStrides) {
    static_assert(std::is_same_v<PFN_vkCmdBindVertexBuffers2EXT, decltype(&V_vkCmdBindVertexBuffers2EXT)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdBindVertexBuffers2EXT) rec->dt.vkCmdBindVertexBuffers2EXT(commandBuffer, firstBinding, bindingCount, pBuffers, pOffsets, pSizes, pStrides);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdSetDepthTestEnableEXT(VkCommandBuffer commandBuffer, VkBool32 depthTestEnable) {
    static_assert(std::is_same_v<PFN_vkCmdSetDepthTestEnableEXT, decltype(&V_vkCmdSetDepthTestEnableEXT)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdSetDepthTestEnableEXT) rec->dt.vkCmdSetDepthTestEnableEXT(commandBuffer, depthTestEnable);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdSetDepthWriteEnableEXT(VkCommandBuffer commandBuffer, VkBool32 depthWriteEnable) {
    static_assert(std::is_same_v<PFN_vkCmdSetDepthWriteEnableEXT, decltype(&V_vkCmdSetDepthWriteEnableEXT)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdSetDepthWriteEnableEXT) rec->dt.vkCmdSetDepthWriteEnableEXT(commandBuffer, depthWriteEnable);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdSetDepthCompareOpEXT(VkCommandBuffer commandBuffer, VkCompareOp depthCompareOp) {
    static_assert(std::is_same_v<PFN_vkCmdSetDepthCompareOpEXT, decltype(&V_vkCmdSetDepthCompareOpEXT)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdSetDepthCompareOpEXT) rec->dt.vkCmdSetDepthCompareOpEXT(commandBuffer, depthCompareOp);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdSetDepthBoundsTestEnableEXT(VkCommandBuffer commandBuffer, VkBool32 depthBoundsTestEnable) {
    static_assert(std::is_same_v<PFN_vkCmdSetDepthBoundsTestEnableEXT, decltype(&V_vkCmdSetDepthBoundsTestEnableEXT)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdSetDepthBoundsTestEnableEXT) rec->dt.vkCmdSetDepthBoundsTestEnableEXT(commandBuffer, depthBoundsTestEnable);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdSetStencilTestEnableEXT(VkCommandBuffer commandBuffer, VkBool32 stencilTestEnable) {
    static_assert(std::is_same_v<PFN_vkCmdSetStencilTestEnableEXT, decltype(&V_vkCmdSetStencilTestEnableEXT)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdSetStencilTestEnableEXT) rec->dt.vkCmdSetStencilTestEnableEXT(commandBuffer, stencilTestEnable);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdSetStencilOpEXT(VkCommandBuffer commandBuffer, VkStencilFaceFlags faceMask, VkStencilOp failOp, VkStencilOp passOp, VkStencilOp depthFailOp, VkCompareOp compareOp) {
    static_assert(std::is_same_v<PFN_vkCmdSetStencilOpEXT, decltype(&V_vkCmdSetStencilOpEXT)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdSetStencilOpEXT) rec->dt.vkCmdSetStencilOpEXT(commandBuffer, faceMask, failOp, passOp, depthFailOp, compareOp);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdSetPatchControlPointsEXT(VkCommandBuffer commandBuffer, uint32_t patchControlPoints) {
    static_assert(std::is_same_v<PFN_vkCmdSetPatchControlPointsEXT, decltype(&V_vkCmdSetPatchControlPointsEXT)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdSetPatchControlPointsEXT) rec->dt.vkCmdSetPatchControlPointsEXT(commandBuffer, patchControlPoints);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdSetRasterizerDiscardEnableEXT(VkCommandBuffer commandBuffer, VkBool32 rasterizerDiscardEnable) {
    static_assert(std::is_same_v<PFN_vkCmdSetRasterizerDiscardEnableEXT, decltype(&V_vkCmdSetRasterizerDiscardEnableEXT)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdSetRasterizerDiscardEnableEXT) rec->dt.vkCmdSetRasterizerDiscardEnableEXT(commandBuffer, rasterizerDiscardEnable);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdSetDepthBiasEnableEXT(VkCommandBuffer commandBuffer, VkBool32 depthBiasEnable) {
    static_assert(std::is_same_v<PFN_vkCmdSetDepthBiasEnableEXT, decltype(&V_vkCmdSetDepthBiasEnableEXT)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdSetDepthBiasEnableEXT) rec->dt.vkCmdSetDepthBiasEnableEXT(commandBuffer, depthBiasEnable);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdSetLogicOpEXT(VkCommandBuffer commandBuffer, VkLogicOp logicOp) {
    static_assert(std::is_same_v<PFN_vkCmdSetLogicOpEXT, decltype(&V_vkCmdSetLogicOpEXT)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdSetLogicOpEXT) rec->dt.vkCmdSetLogicOpEXT(commandBuffer, logicOp);
}
VKAPI_ATTR void VKAPI_CALL V_vkCmdSetPrimitiveRestartEnableEXT(VkCommandBuffer commandBuffer, VkBool32 primitiveRestartEnable) {
    static_assert(std::is_same_v<PFN_vkCmdSetPrimitiveRestartEnableEXT, decltype(&V_vkCmdSetPrimitiveRestartEnableEXT)>);
    DeviceRecord* rec = LookupCmdBufDevice(commandBuffer);
    if (rec && rec->dt.vkCmdSetPrimitiveRestartEnableEXT) rec->dt.vkCmdSetPrimitiveRestartEnableEXT(commandBuffer, primitiveRestartEnable);
}

struct AutoVulkanEntry { const char* name; void* fn; bool isDevice; bool exported; };
const AutoVulkanEntry kAutoVulkanEntries[] = {
    {"vkGetPhysicalDeviceFeatures", reinterpret_cast<void*>(&V_vkGetPhysicalDeviceFeatures), false, true},
    {"vkGetPhysicalDeviceFormatProperties", reinterpret_cast<void*>(&V_vkGetPhysicalDeviceFormatProperties), false, true},
    {"vkGetPhysicalDeviceImageFormatProperties", reinterpret_cast<void*>(&V_vkGetPhysicalDeviceImageFormatProperties), false, true},
    {"vkGetPhysicalDeviceProperties", reinterpret_cast<void*>(&V_vkGetPhysicalDeviceProperties), false, true},
    {"vkGetPhysicalDeviceQueueFamilyProperties", reinterpret_cast<void*>(&V_vkGetPhysicalDeviceQueueFamilyProperties), false, true},
    {"vkGetPhysicalDeviceMemoryProperties", reinterpret_cast<void*>(&V_vkGetPhysicalDeviceMemoryProperties), false, true},
    {"vkQueueSubmit", reinterpret_cast<void*>(&V_vkQueueSubmit), true, true},
    {"vkQueueWaitIdle", reinterpret_cast<void*>(&V_vkQueueWaitIdle), true, true},
    {"vkDeviceWaitIdle", reinterpret_cast<void*>(&V_vkDeviceWaitIdle), true, true},
    {"vkAllocateMemory", reinterpret_cast<void*>(&V_vkAllocateMemory), true, true},
    {"vkFreeMemory", reinterpret_cast<void*>(&V_vkFreeMemory), true, true},
    {"vkMapMemory", reinterpret_cast<void*>(&V_vkMapMemory), true, true},
    {"vkUnmapMemory", reinterpret_cast<void*>(&V_vkUnmapMemory), true, true},
    {"vkFlushMappedMemoryRanges", reinterpret_cast<void*>(&V_vkFlushMappedMemoryRanges), true, true},
    {"vkInvalidateMappedMemoryRanges", reinterpret_cast<void*>(&V_vkInvalidateMappedMemoryRanges), true, true},
    {"vkGetDeviceMemoryCommitment", reinterpret_cast<void*>(&V_vkGetDeviceMemoryCommitment), true, true},
    {"vkBindBufferMemory", reinterpret_cast<void*>(&V_vkBindBufferMemory), true, true},
    {"vkBindImageMemory", reinterpret_cast<void*>(&V_vkBindImageMemory), true, true},
    {"vkGetBufferMemoryRequirements", reinterpret_cast<void*>(&V_vkGetBufferMemoryRequirements), true, true},
    {"vkGetImageMemoryRequirements", reinterpret_cast<void*>(&V_vkGetImageMemoryRequirements), true, true},
    {"vkGetImageSparseMemoryRequirements", reinterpret_cast<void*>(&V_vkGetImageSparseMemoryRequirements), true, true},
    {"vkGetPhysicalDeviceSparseImageFormatProperties", reinterpret_cast<void*>(&V_vkGetPhysicalDeviceSparseImageFormatProperties), false, true},
    {"vkQueueBindSparse", reinterpret_cast<void*>(&V_vkQueueBindSparse), true, true},
    {"vkCreateFence", reinterpret_cast<void*>(&V_vkCreateFence), true, true},
    {"vkDestroyFence", reinterpret_cast<void*>(&V_vkDestroyFence), true, true},
    {"vkResetFences", reinterpret_cast<void*>(&V_vkResetFences), true, true},
    {"vkGetFenceStatus", reinterpret_cast<void*>(&V_vkGetFenceStatus), true, true},
    {"vkWaitForFences", reinterpret_cast<void*>(&V_vkWaitForFences), true, true},
    {"vkCreateSemaphore", reinterpret_cast<void*>(&V_vkCreateSemaphore), true, true},
    {"vkDestroySemaphore", reinterpret_cast<void*>(&V_vkDestroySemaphore), true, true},
    {"vkCreateQueryPool", reinterpret_cast<void*>(&V_vkCreateQueryPool), true, true},
    {"vkDestroyQueryPool", reinterpret_cast<void*>(&V_vkDestroyQueryPool), true, true},
    {"vkGetQueryPoolResults", reinterpret_cast<void*>(&V_vkGetQueryPoolResults), true, true},
    {"vkCreateBuffer", reinterpret_cast<void*>(&V_vkCreateBuffer), true, true},
    {"vkDestroyBuffer", reinterpret_cast<void*>(&V_vkDestroyBuffer), true, true},
    {"vkCreateImage", reinterpret_cast<void*>(&V_vkCreateImage), true, true},
    {"vkDestroyImage", reinterpret_cast<void*>(&V_vkDestroyImage), true, true},
    {"vkGetImageSubresourceLayout", reinterpret_cast<void*>(&V_vkGetImageSubresourceLayout), true, true},
    {"vkCreateImageView", reinterpret_cast<void*>(&V_vkCreateImageView), true, true},
    {"vkDestroyImageView", reinterpret_cast<void*>(&V_vkDestroyImageView), true, true},
    {"vkCreateCommandPool", reinterpret_cast<void*>(&V_vkCreateCommandPool), true, true},
    {"vkResetCommandPool", reinterpret_cast<void*>(&V_vkResetCommandPool), true, true},
    {"vkBeginCommandBuffer", reinterpret_cast<void*>(&V_vkBeginCommandBuffer), true, true},
    {"vkEndCommandBuffer", reinterpret_cast<void*>(&V_vkEndCommandBuffer), true, true},
    {"vkResetCommandBuffer", reinterpret_cast<void*>(&V_vkResetCommandBuffer), true, true},
    {"vkCmdCopyBuffer", reinterpret_cast<void*>(&V_vkCmdCopyBuffer), true, true},
    {"vkCmdCopyImage", reinterpret_cast<void*>(&V_vkCmdCopyImage), true, true},
    {"vkCmdCopyBufferToImage", reinterpret_cast<void*>(&V_vkCmdCopyBufferToImage), true, true},
    {"vkCmdCopyImageToBuffer", reinterpret_cast<void*>(&V_vkCmdCopyImageToBuffer), true, true},
    {"vkCmdUpdateBuffer", reinterpret_cast<void*>(&V_vkCmdUpdateBuffer), true, true},
    {"vkCmdFillBuffer", reinterpret_cast<void*>(&V_vkCmdFillBuffer), true, true},
    {"vkCmdPipelineBarrier", reinterpret_cast<void*>(&V_vkCmdPipelineBarrier), true, true},
    {"vkCmdBeginQuery", reinterpret_cast<void*>(&V_vkCmdBeginQuery), true, true},
    {"vkCmdEndQuery", reinterpret_cast<void*>(&V_vkCmdEndQuery), true, true},
    {"vkCmdResetQueryPool", reinterpret_cast<void*>(&V_vkCmdResetQueryPool), true, true},
    {"vkCmdWriteTimestamp", reinterpret_cast<void*>(&V_vkCmdWriteTimestamp), true, true},
    {"vkCmdCopyQueryPoolResults", reinterpret_cast<void*>(&V_vkCmdCopyQueryPoolResults), true, true},
    {"vkCmdExecuteCommands", reinterpret_cast<void*>(&V_vkCmdExecuteCommands), true, true},
    {"vkCreateEvent", reinterpret_cast<void*>(&V_vkCreateEvent), true, true},
    {"vkDestroyEvent", reinterpret_cast<void*>(&V_vkDestroyEvent), true, true},
    {"vkGetEventStatus", reinterpret_cast<void*>(&V_vkGetEventStatus), true, true},
    {"vkSetEvent", reinterpret_cast<void*>(&V_vkSetEvent), true, true},
    {"vkResetEvent", reinterpret_cast<void*>(&V_vkResetEvent), true, true},
    {"vkCreateBufferView", reinterpret_cast<void*>(&V_vkCreateBufferView), true, true},
    {"vkDestroyBufferView", reinterpret_cast<void*>(&V_vkDestroyBufferView), true, true},
    {"vkCreateShaderModule", reinterpret_cast<void*>(&V_vkCreateShaderModule), true, true},
    {"vkDestroyShaderModule", reinterpret_cast<void*>(&V_vkDestroyShaderModule), true, true},
    {"vkCreatePipelineCache", reinterpret_cast<void*>(&V_vkCreatePipelineCache), true, true},
    {"vkDestroyPipelineCache", reinterpret_cast<void*>(&V_vkDestroyPipelineCache), true, true},
    {"vkGetPipelineCacheData", reinterpret_cast<void*>(&V_vkGetPipelineCacheData), true, true},
    {"vkMergePipelineCaches", reinterpret_cast<void*>(&V_vkMergePipelineCaches), true, true},
    {"vkCreateComputePipelines", reinterpret_cast<void*>(&V_vkCreateComputePipelines), true, true},
    {"vkDestroyPipeline", reinterpret_cast<void*>(&V_vkDestroyPipeline), true, true},
    {"vkCreatePipelineLayout", reinterpret_cast<void*>(&V_vkCreatePipelineLayout), true, true},
    {"vkDestroyPipelineLayout", reinterpret_cast<void*>(&V_vkDestroyPipelineLayout), true, true},
    {"vkCreateSampler", reinterpret_cast<void*>(&V_vkCreateSampler), true, true},
    {"vkDestroySampler", reinterpret_cast<void*>(&V_vkDestroySampler), true, true},
    {"vkCreateDescriptorSetLayout", reinterpret_cast<void*>(&V_vkCreateDescriptorSetLayout), true, true},
    {"vkDestroyDescriptorSetLayout", reinterpret_cast<void*>(&V_vkDestroyDescriptorSetLayout), true, true},
    {"vkCreateDescriptorPool", reinterpret_cast<void*>(&V_vkCreateDescriptorPool), true, true},
    {"vkDestroyDescriptorPool", reinterpret_cast<void*>(&V_vkDestroyDescriptorPool), true, true},
    {"vkResetDescriptorPool", reinterpret_cast<void*>(&V_vkResetDescriptorPool), true, true},
    {"vkAllocateDescriptorSets", reinterpret_cast<void*>(&V_vkAllocateDescriptorSets), true, true},
    {"vkFreeDescriptorSets", reinterpret_cast<void*>(&V_vkFreeDescriptorSets), true, true},
    {"vkUpdateDescriptorSets", reinterpret_cast<void*>(&V_vkUpdateDescriptorSets), true, true},
    {"vkCmdBindPipeline", reinterpret_cast<void*>(&V_vkCmdBindPipeline), true, true},
    {"vkCmdBindDescriptorSets", reinterpret_cast<void*>(&V_vkCmdBindDescriptorSets), true, true},
    {"vkCmdClearColorImage", reinterpret_cast<void*>(&V_vkCmdClearColorImage), true, true},
    {"vkCmdDispatch", reinterpret_cast<void*>(&V_vkCmdDispatch), true, true},
    {"vkCmdDispatchIndirect", reinterpret_cast<void*>(&V_vkCmdDispatchIndirect), true, true},
    {"vkCmdSetEvent", reinterpret_cast<void*>(&V_vkCmdSetEvent), true, true},
    {"vkCmdResetEvent", reinterpret_cast<void*>(&V_vkCmdResetEvent), true, true},
    {"vkCmdWaitEvents", reinterpret_cast<void*>(&V_vkCmdWaitEvents), true, true},
    {"vkCmdPushConstants", reinterpret_cast<void*>(&V_vkCmdPushConstants), true, true},
    {"vkCreateGraphicsPipelines", reinterpret_cast<void*>(&V_vkCreateGraphicsPipelines), true, true},
    {"vkCreateFramebuffer", reinterpret_cast<void*>(&V_vkCreateFramebuffer), true, true},
    {"vkDestroyFramebuffer", reinterpret_cast<void*>(&V_vkDestroyFramebuffer), true, true},
    {"vkCreateRenderPass", reinterpret_cast<void*>(&V_vkCreateRenderPass), true, true},
    {"vkDestroyRenderPass", reinterpret_cast<void*>(&V_vkDestroyRenderPass), true, true},
    {"vkGetRenderAreaGranularity", reinterpret_cast<void*>(&V_vkGetRenderAreaGranularity), true, true},
    {"vkCmdSetViewport", reinterpret_cast<void*>(&V_vkCmdSetViewport), true, true},
    {"vkCmdSetScissor", reinterpret_cast<void*>(&V_vkCmdSetScissor), true, true},
    {"vkCmdSetLineWidth", reinterpret_cast<void*>(&V_vkCmdSetLineWidth), true, true},
    {"vkCmdSetDepthBias", reinterpret_cast<void*>(&V_vkCmdSetDepthBias), true, true},
    {"vkCmdSetBlendConstants", reinterpret_cast<void*>(&V_vkCmdSetBlendConstants), true, true},
    {"vkCmdSetDepthBounds", reinterpret_cast<void*>(&V_vkCmdSetDepthBounds), true, true},
    {"vkCmdSetStencilCompareMask", reinterpret_cast<void*>(&V_vkCmdSetStencilCompareMask), true, true},
    {"vkCmdSetStencilWriteMask", reinterpret_cast<void*>(&V_vkCmdSetStencilWriteMask), true, true},
    {"vkCmdSetStencilReference", reinterpret_cast<void*>(&V_vkCmdSetStencilReference), true, true},
    {"vkCmdBindIndexBuffer", reinterpret_cast<void*>(&V_vkCmdBindIndexBuffer), true, true},
    {"vkCmdBindVertexBuffers", reinterpret_cast<void*>(&V_vkCmdBindVertexBuffers), true, true},
    {"vkCmdDraw", reinterpret_cast<void*>(&V_vkCmdDraw), true, true},
    {"vkCmdDrawIndexed", reinterpret_cast<void*>(&V_vkCmdDrawIndexed), true, true},
    {"vkCmdDrawIndirect", reinterpret_cast<void*>(&V_vkCmdDrawIndirect), true, true},
    {"vkCmdDrawIndexedIndirect", reinterpret_cast<void*>(&V_vkCmdDrawIndexedIndirect), true, true},
    {"vkCmdBlitImage", reinterpret_cast<void*>(&V_vkCmdBlitImage), true, true},
    {"vkCmdClearDepthStencilImage", reinterpret_cast<void*>(&V_vkCmdClearDepthStencilImage), true, true},
    {"vkCmdClearAttachments", reinterpret_cast<void*>(&V_vkCmdClearAttachments), true, true},
    {"vkCmdResolveImage", reinterpret_cast<void*>(&V_vkCmdResolveImage), true, true},
    {"vkCmdBeginRenderPass", reinterpret_cast<void*>(&V_vkCmdBeginRenderPass), true, true},
    {"vkCmdNextSubpass", reinterpret_cast<void*>(&V_vkCmdNextSubpass), true, true},
    {"vkCmdEndRenderPass", reinterpret_cast<void*>(&V_vkCmdEndRenderPass), true, true},
    {"vkBindBufferMemory2", reinterpret_cast<void*>(&V_vkBindBufferMemory2), true, true},
    {"vkBindImageMemory2", reinterpret_cast<void*>(&V_vkBindImageMemory2), true, true},
    {"vkGetDeviceGroupPeerMemoryFeatures", reinterpret_cast<void*>(&V_vkGetDeviceGroupPeerMemoryFeatures), true, true},
    {"vkCmdSetDeviceMask", reinterpret_cast<void*>(&V_vkCmdSetDeviceMask), true, true},
    {"vkGetImageMemoryRequirements2", reinterpret_cast<void*>(&V_vkGetImageMemoryRequirements2), true, true},
    {"vkGetBufferMemoryRequirements2", reinterpret_cast<void*>(&V_vkGetBufferMemoryRequirements2), true, true},
    {"vkGetImageSparseMemoryRequirements2", reinterpret_cast<void*>(&V_vkGetImageSparseMemoryRequirements2), true, true},
    {"vkGetPhysicalDeviceFeatures2", reinterpret_cast<void*>(&V_vkGetPhysicalDeviceFeatures2), false, true},
    {"vkGetPhysicalDeviceProperties2", reinterpret_cast<void*>(&V_vkGetPhysicalDeviceProperties2), false, true},
    {"vkGetPhysicalDeviceFormatProperties2", reinterpret_cast<void*>(&V_vkGetPhysicalDeviceFormatProperties2), false, true},
    {"vkGetPhysicalDeviceImageFormatProperties2", reinterpret_cast<void*>(&V_vkGetPhysicalDeviceImageFormatProperties2), false, true},
    {"vkGetPhysicalDeviceQueueFamilyProperties2", reinterpret_cast<void*>(&V_vkGetPhysicalDeviceQueueFamilyProperties2), false, true},
    {"vkGetPhysicalDeviceMemoryProperties2", reinterpret_cast<void*>(&V_vkGetPhysicalDeviceMemoryProperties2), false, true},
    {"vkGetPhysicalDeviceSparseImageFormatProperties2", reinterpret_cast<void*>(&V_vkGetPhysicalDeviceSparseImageFormatProperties2), false, true},
    {"vkTrimCommandPool", reinterpret_cast<void*>(&V_vkTrimCommandPool), true, true},
    {"vkGetPhysicalDeviceExternalBufferProperties", reinterpret_cast<void*>(&V_vkGetPhysicalDeviceExternalBufferProperties), false, true},
    {"vkGetPhysicalDeviceExternalFenceProperties", reinterpret_cast<void*>(&V_vkGetPhysicalDeviceExternalFenceProperties), false, true},
    {"vkGetPhysicalDeviceExternalSemaphoreProperties", reinterpret_cast<void*>(&V_vkGetPhysicalDeviceExternalSemaphoreProperties), false, true},
    {"vkCmdDispatchBase", reinterpret_cast<void*>(&V_vkCmdDispatchBase), true, true},
    {"vkCreateDescriptorUpdateTemplate", reinterpret_cast<void*>(&V_vkCreateDescriptorUpdateTemplate), true, true},
    {"vkDestroyDescriptorUpdateTemplate", reinterpret_cast<void*>(&V_vkDestroyDescriptorUpdateTemplate), true, true},
    {"vkUpdateDescriptorSetWithTemplate", reinterpret_cast<void*>(&V_vkUpdateDescriptorSetWithTemplate), true, true},
    {"vkGetDescriptorSetLayoutSupport", reinterpret_cast<void*>(&V_vkGetDescriptorSetLayoutSupport), true, true},
    {"vkCreateSamplerYcbcrConversion", reinterpret_cast<void*>(&V_vkCreateSamplerYcbcrConversion), true, true},
    {"vkDestroySamplerYcbcrConversion", reinterpret_cast<void*>(&V_vkDestroySamplerYcbcrConversion), true, true},
    {"vkResetQueryPool", reinterpret_cast<void*>(&V_vkResetQueryPool), true, true},
    {"vkGetSemaphoreCounterValue", reinterpret_cast<void*>(&V_vkGetSemaphoreCounterValue), true, true},
    {"vkWaitSemaphores", reinterpret_cast<void*>(&V_vkWaitSemaphores), true, true},
    {"vkSignalSemaphore", reinterpret_cast<void*>(&V_vkSignalSemaphore), true, true},
    {"vkGetBufferDeviceAddress", reinterpret_cast<void*>(&V_vkGetBufferDeviceAddress), true, true},
    {"vkGetBufferOpaqueCaptureAddress", reinterpret_cast<void*>(&V_vkGetBufferOpaqueCaptureAddress), true, true},
    {"vkGetDeviceMemoryOpaqueCaptureAddress", reinterpret_cast<void*>(&V_vkGetDeviceMemoryOpaqueCaptureAddress), true, true},
    {"vkCmdDrawIndirectCount", reinterpret_cast<void*>(&V_vkCmdDrawIndirectCount), true, true},
    {"vkCmdDrawIndexedIndirectCount", reinterpret_cast<void*>(&V_vkCmdDrawIndexedIndirectCount), true, true},
    {"vkCreateRenderPass2", reinterpret_cast<void*>(&V_vkCreateRenderPass2), true, true},
    {"vkCmdBeginRenderPass2", reinterpret_cast<void*>(&V_vkCmdBeginRenderPass2), true, true},
    {"vkCmdNextSubpass2", reinterpret_cast<void*>(&V_vkCmdNextSubpass2), true, true},
    {"vkCmdEndRenderPass2", reinterpret_cast<void*>(&V_vkCmdEndRenderPass2), true, true},
    {"vkGetPhysicalDeviceToolProperties", reinterpret_cast<void*>(&V_vkGetPhysicalDeviceToolProperties), false, true},
    {"vkCreatePrivateDataSlot", reinterpret_cast<void*>(&V_vkCreatePrivateDataSlot), true, true},
    {"vkDestroyPrivateDataSlot", reinterpret_cast<void*>(&V_vkDestroyPrivateDataSlot), true, true},
    {"vkSetPrivateData", reinterpret_cast<void*>(&V_vkSetPrivateData), true, true},
    {"vkGetPrivateData", reinterpret_cast<void*>(&V_vkGetPrivateData), true, true},
    {"vkCmdPipelineBarrier2", reinterpret_cast<void*>(&V_vkCmdPipelineBarrier2), true, true},
    {"vkCmdWriteTimestamp2", reinterpret_cast<void*>(&V_vkCmdWriteTimestamp2), true, true},
    {"vkQueueSubmit2", reinterpret_cast<void*>(&V_vkQueueSubmit2), true, true},
    {"vkCmdCopyBuffer2", reinterpret_cast<void*>(&V_vkCmdCopyBuffer2), true, true},
    {"vkCmdCopyImage2", reinterpret_cast<void*>(&V_vkCmdCopyImage2), true, true},
    {"vkCmdCopyBufferToImage2", reinterpret_cast<void*>(&V_vkCmdCopyBufferToImage2), true, true},
    {"vkCmdCopyImageToBuffer2", reinterpret_cast<void*>(&V_vkCmdCopyImageToBuffer2), true, true},
    {"vkGetDeviceBufferMemoryRequirements", reinterpret_cast<void*>(&V_vkGetDeviceBufferMemoryRequirements), true, true},
    {"vkGetDeviceImageMemoryRequirements", reinterpret_cast<void*>(&V_vkGetDeviceImageMemoryRequirements), true, true},
    {"vkGetDeviceImageSparseMemoryRequirements", reinterpret_cast<void*>(&V_vkGetDeviceImageSparseMemoryRequirements), true, true},
    {"vkCmdSetEvent2", reinterpret_cast<void*>(&V_vkCmdSetEvent2), true, true},
    {"vkCmdResetEvent2", reinterpret_cast<void*>(&V_vkCmdResetEvent2), true, true},
    {"vkCmdWaitEvents2", reinterpret_cast<void*>(&V_vkCmdWaitEvents2), true, true},
    {"vkCmdBlitImage2", reinterpret_cast<void*>(&V_vkCmdBlitImage2), true, true},
    {"vkCmdResolveImage2", reinterpret_cast<void*>(&V_vkCmdResolveImage2), true, true},
    {"vkCmdBeginRendering", reinterpret_cast<void*>(&V_vkCmdBeginRendering), true, true},
    {"vkCmdEndRendering", reinterpret_cast<void*>(&V_vkCmdEndRendering), true, true},
    {"vkCmdSetCullMode", reinterpret_cast<void*>(&V_vkCmdSetCullMode), true, true},
    {"vkCmdSetFrontFace", reinterpret_cast<void*>(&V_vkCmdSetFrontFace), true, true},
    {"vkCmdSetPrimitiveTopology", reinterpret_cast<void*>(&V_vkCmdSetPrimitiveTopology), true, true},
    {"vkCmdSetViewportWithCount", reinterpret_cast<void*>(&V_vkCmdSetViewportWithCount), true, true},
    {"vkCmdSetScissorWithCount", reinterpret_cast<void*>(&V_vkCmdSetScissorWithCount), true, true},
    {"vkCmdBindVertexBuffers2", reinterpret_cast<void*>(&V_vkCmdBindVertexBuffers2), true, true},
    {"vkCmdSetDepthTestEnable", reinterpret_cast<void*>(&V_vkCmdSetDepthTestEnable), true, true},
    {"vkCmdSetDepthWriteEnable", reinterpret_cast<void*>(&V_vkCmdSetDepthWriteEnable), true, true},
    {"vkCmdSetDepthCompareOp", reinterpret_cast<void*>(&V_vkCmdSetDepthCompareOp), true, true},
    {"vkCmdSetDepthBoundsTestEnable", reinterpret_cast<void*>(&V_vkCmdSetDepthBoundsTestEnable), true, true},
    {"vkCmdSetStencilTestEnable", reinterpret_cast<void*>(&V_vkCmdSetStencilTestEnable), true, true},
    {"vkCmdSetStencilOp", reinterpret_cast<void*>(&V_vkCmdSetStencilOp), true, true},
    {"vkCmdSetRasterizerDiscardEnable", reinterpret_cast<void*>(&V_vkCmdSetRasterizerDiscardEnable), true, true},
    {"vkCmdSetDepthBiasEnable", reinterpret_cast<void*>(&V_vkCmdSetDepthBiasEnable), true, true},
    {"vkCmdSetPrimitiveRestartEnable", reinterpret_cast<void*>(&V_vkCmdSetPrimitiveRestartEnable), true, true},
    {"vkCmdBeginRenderingKHR", reinterpret_cast<void*>(&V_vkCmdBeginRenderingKHR), true, false},
    {"vkCmdEndRenderingKHR", reinterpret_cast<void*>(&V_vkCmdEndRenderingKHR), true, false},
    {"vkGetPhysicalDeviceFeatures2KHR", reinterpret_cast<void*>(&V_vkGetPhysicalDeviceFeatures2KHR), false, false},
    {"vkGetPhysicalDeviceProperties2KHR", reinterpret_cast<void*>(&V_vkGetPhysicalDeviceProperties2KHR), false, false},
    {"vkGetPhysicalDeviceFormatProperties2KHR", reinterpret_cast<void*>(&V_vkGetPhysicalDeviceFormatProperties2KHR), false, false},
    {"vkGetPhysicalDeviceImageFormatProperties2KHR", reinterpret_cast<void*>(&V_vkGetPhysicalDeviceImageFormatProperties2KHR), false, false},
    {"vkGetPhysicalDeviceQueueFamilyProperties2KHR", reinterpret_cast<void*>(&V_vkGetPhysicalDeviceQueueFamilyProperties2KHR), false, false},
    {"vkGetPhysicalDeviceMemoryProperties2KHR", reinterpret_cast<void*>(&V_vkGetPhysicalDeviceMemoryProperties2KHR), false, false},
    {"vkGetPhysicalDeviceSparseImageFormatProperties2KHR", reinterpret_cast<void*>(&V_vkGetPhysicalDeviceSparseImageFormatProperties2KHR), false, false},
    {"vkTrimCommandPoolKHR", reinterpret_cast<void*>(&V_vkTrimCommandPoolKHR), true, false},
    {"vkCmdPushDescriptorSetKHR", reinterpret_cast<void*>(&V_vkCmdPushDescriptorSetKHR), true, false},
    {"vkCmdPushDescriptorSetWithTemplateKHR", reinterpret_cast<void*>(&V_vkCmdPushDescriptorSetWithTemplateKHR), true, false},
    {"vkCreateDescriptorUpdateTemplateKHR", reinterpret_cast<void*>(&V_vkCreateDescriptorUpdateTemplateKHR), true, false},
    {"vkDestroyDescriptorUpdateTemplateKHR", reinterpret_cast<void*>(&V_vkDestroyDescriptorUpdateTemplateKHR), true, false},
    {"vkUpdateDescriptorSetWithTemplateKHR", reinterpret_cast<void*>(&V_vkUpdateDescriptorSetWithTemplateKHR), true, false},
    {"vkCreateRenderPass2KHR", reinterpret_cast<void*>(&V_vkCreateRenderPass2KHR), true, false},
    {"vkCmdBeginRenderPass2KHR", reinterpret_cast<void*>(&V_vkCmdBeginRenderPass2KHR), true, false},
    {"vkCmdNextSubpass2KHR", reinterpret_cast<void*>(&V_vkCmdNextSubpass2KHR), true, false},
    {"vkCmdEndRenderPass2KHR", reinterpret_cast<void*>(&V_vkCmdEndRenderPass2KHR), true, false},
    {"vkGetImageMemoryRequirements2KHR", reinterpret_cast<void*>(&V_vkGetImageMemoryRequirements2KHR), true, false},
    {"vkGetBufferMemoryRequirements2KHR", reinterpret_cast<void*>(&V_vkGetBufferMemoryRequirements2KHR), true, false},
    {"vkGetImageSparseMemoryRequirements2KHR", reinterpret_cast<void*>(&V_vkGetImageSparseMemoryRequirements2KHR), true, false},
    {"vkCreateSamplerYcbcrConversionKHR", reinterpret_cast<void*>(&V_vkCreateSamplerYcbcrConversionKHR), true, false},
    {"vkDestroySamplerYcbcrConversionKHR", reinterpret_cast<void*>(&V_vkDestroySamplerYcbcrConversionKHR), true, false},
    {"vkBindBufferMemory2KHR", reinterpret_cast<void*>(&V_vkBindBufferMemory2KHR), true, false},
    {"vkBindImageMemory2KHR", reinterpret_cast<void*>(&V_vkBindImageMemory2KHR), true, false},
    {"vkGetDescriptorSetLayoutSupportKHR", reinterpret_cast<void*>(&V_vkGetDescriptorSetLayoutSupportKHR), true, false},
    {"vkCmdDrawIndirectCountKHR", reinterpret_cast<void*>(&V_vkCmdDrawIndirectCountKHR), true, false},
    {"vkCmdDrawIndexedIndirectCountKHR", reinterpret_cast<void*>(&V_vkCmdDrawIndexedIndirectCountKHR), true, false},
    {"vkGetSemaphoreCounterValueKHR", reinterpret_cast<void*>(&V_vkGetSemaphoreCounterValueKHR), true, false},
    {"vkWaitSemaphoresKHR", reinterpret_cast<void*>(&V_vkWaitSemaphoresKHR), true, false},
    {"vkSignalSemaphoreKHR", reinterpret_cast<void*>(&V_vkSignalSemaphoreKHR), true, false},
    {"vkGetBufferDeviceAddressKHR", reinterpret_cast<void*>(&V_vkGetBufferDeviceAddressKHR), true, false},
    {"vkGetBufferOpaqueCaptureAddressKHR", reinterpret_cast<void*>(&V_vkGetBufferOpaqueCaptureAddressKHR), true, false},
    {"vkGetDeviceMemoryOpaqueCaptureAddressKHR", reinterpret_cast<void*>(&V_vkGetDeviceMemoryOpaqueCaptureAddressKHR), true, false},
    {"vkCmdSetEvent2KHR", reinterpret_cast<void*>(&V_vkCmdSetEvent2KHR), true, false},
    {"vkCmdResetEvent2KHR", reinterpret_cast<void*>(&V_vkCmdResetEvent2KHR), true, false},
    {"vkCmdWaitEvents2KHR", reinterpret_cast<void*>(&V_vkCmdWaitEvents2KHR), true, false},
    {"vkCmdPipelineBarrier2KHR", reinterpret_cast<void*>(&V_vkCmdPipelineBarrier2KHR), true, false},
    {"vkCmdWriteTimestamp2KHR", reinterpret_cast<void*>(&V_vkCmdWriteTimestamp2KHR), true, false},
    {"vkQueueSubmit2KHR", reinterpret_cast<void*>(&V_vkQueueSubmit2KHR), true, false},
    {"vkCmdCopyBuffer2KHR", reinterpret_cast<void*>(&V_vkCmdCopyBuffer2KHR), true, false},
    {"vkCmdCopyImage2KHR", reinterpret_cast<void*>(&V_vkCmdCopyImage2KHR), true, false},
    {"vkCmdCopyBufferToImage2KHR", reinterpret_cast<void*>(&V_vkCmdCopyBufferToImage2KHR), true, false},
    {"vkCmdCopyImageToBuffer2KHR", reinterpret_cast<void*>(&V_vkCmdCopyImageToBuffer2KHR), true, false},
    {"vkCmdBlitImage2KHR", reinterpret_cast<void*>(&V_vkCmdBlitImage2KHR), true, false},
    {"vkCmdResolveImage2KHR", reinterpret_cast<void*>(&V_vkCmdResolveImage2KHR), true, false},
    {"vkGetDeviceBufferMemoryRequirementsKHR", reinterpret_cast<void*>(&V_vkGetDeviceBufferMemoryRequirementsKHR), true, false},
    {"vkGetDeviceImageMemoryRequirementsKHR", reinterpret_cast<void*>(&V_vkGetDeviceImageMemoryRequirementsKHR), true, false},
    {"vkGetDeviceImageSparseMemoryRequirementsKHR", reinterpret_cast<void*>(&V_vkGetDeviceImageSparseMemoryRequirementsKHR), true, false},
    {"vkCmdSetCullModeEXT", reinterpret_cast<void*>(&V_vkCmdSetCullModeEXT), true, false},
    {"vkCmdSetFrontFaceEXT", reinterpret_cast<void*>(&V_vkCmdSetFrontFaceEXT), true, false},
    {"vkCmdSetPrimitiveTopologyEXT", reinterpret_cast<void*>(&V_vkCmdSetPrimitiveTopologyEXT), true, false},
    {"vkCmdSetViewportWithCountEXT", reinterpret_cast<void*>(&V_vkCmdSetViewportWithCountEXT), true, false},
    {"vkCmdSetScissorWithCountEXT", reinterpret_cast<void*>(&V_vkCmdSetScissorWithCountEXT), true, false},
    {"vkCmdBindVertexBuffers2EXT", reinterpret_cast<void*>(&V_vkCmdBindVertexBuffers2EXT), true, false},
    {"vkCmdSetDepthTestEnableEXT", reinterpret_cast<void*>(&V_vkCmdSetDepthTestEnableEXT), true, false},
    {"vkCmdSetDepthWriteEnableEXT", reinterpret_cast<void*>(&V_vkCmdSetDepthWriteEnableEXT), true, false},
    {"vkCmdSetDepthCompareOpEXT", reinterpret_cast<void*>(&V_vkCmdSetDepthCompareOpEXT), true, false},
    {"vkCmdSetDepthBoundsTestEnableEXT", reinterpret_cast<void*>(&V_vkCmdSetDepthBoundsTestEnableEXT), true, false},
    {"vkCmdSetStencilTestEnableEXT", reinterpret_cast<void*>(&V_vkCmdSetStencilTestEnableEXT), true, false},
    {"vkCmdSetStencilOpEXT", reinterpret_cast<void*>(&V_vkCmdSetStencilOpEXT), true, false},
    {"vkCmdSetPatchControlPointsEXT", reinterpret_cast<void*>(&V_vkCmdSetPatchControlPointsEXT), true, false},
    {"vkCmdSetRasterizerDiscardEnableEXT", reinterpret_cast<void*>(&V_vkCmdSetRasterizerDiscardEnableEXT), true, false},
    {"vkCmdSetDepthBiasEnableEXT", reinterpret_cast<void*>(&V_vkCmdSetDepthBiasEnableEXT), true, false},
    {"vkCmdSetLogicOpEXT", reinterpret_cast<void*>(&V_vkCmdSetLogicOpEXT), true, false},
    {"vkCmdSetPrimitiveRestartEnableEXT", reinterpret_cast<void*>(&V_vkCmdSetPrimitiveRestartEnableEXT), true, false},
};
#endif // GUEST_VULKAN_STAGE_IMPL
