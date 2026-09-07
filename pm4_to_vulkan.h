#pragma once

#include "pm4-decoder.h"
#include "pm4_vulkan_backbuffer.h"

#include <vulkan/vulkan.h>
#include <cstddef>
#include <cstdint>

struct PM4GraphicsState
{
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;

    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    VkDeviceSize vertexOffset = 0;

    VkBuffer indexBuffer = VK_NULL_HANDLE;
    VkDeviceSize indexOffset = 0;

    VkIndexType indexType = VK_INDEX_TYPE_UINT32;

    uint32_t primitiveType = 0;
    uint32_t numIndices = 0;
    uint32_t numInstances = 1;

    bool pipelineBound = false;
    bool vertexBufferBound = false;
    bool indexBufferBound = false;
};

class PM4ToVulkan
{
public:
    PM4ToVulkan() = default;

    bool Initialize(
        VkDevice device,
        VkPhysicalDevice physicalDevice,
        PM4VulkanBackbuffer* backbuffer);

    void Reset();

    bool TranslatePacket(
        const PM4::Packet& packet);

    bool TranslateStream(
        const uint32_t* stream,
        std::size_t dwordCount);

    VkCommandBuffer GetCommandBuffer() const;

private:
    bool TranslateType3(
        const PM4::Packet& packet);

    bool TranslateDrawIndexImmediate(
        const PM4::Packet& packet);

    bool TranslateDrawIndexAuto(
        const PM4::Packet& packet);

    bool TranslateDispatchDirect(
        const PM4::Packet& packet);

    bool TranslateEventWrite(
        const PM4::Packet& packet);

    bool TranslateCopyData(
        const PM4::Packet& packet);

    bool TranslateWriteData(
        const PM4::Packet& packet);

private:
    VkDevice m_device = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;

    PM4VulkanBackbuffer* m_backbuffer = nullptr;
    PM4GraphicsState m_graphicsState{};

    VkCommandBuffer m_commandBuffer = VK_NULL_HANDLE;

    void SetPipeline(
    VkPipeline pipeline,
    VkPipelineLayout pipelineLayout);

void SetVertexBuffer(
    VkBuffer buffer,
    VkDeviceSize offset = 0);

void SetIndexBuffer(
    VkBuffer buffer,
    VkDeviceSize offset,
    VkIndexType indexType);

bool DrawIndexed(
    uint32_t indexCount,
    uint32_t instanceCount,
    uint32_t firstIndex,
    int32_t vertexOffset,
    uint32_t firstInstance);
    bool TranslateSetContextReg(
        const PM4::Packet& packet);

    void UpdateContextRegister(
        uint32_t reg,
        uint32_t value);
const PM4GraphicsState& GetGraphicsState() const;

};
 