#include "pm4_to_vulkan.h"

#include <cstring>

namespace
{
    constexpr uint32_t PACKET_TYPE3 = 3;

    constexpr uint32_t TYPE_SHIFT = 30;
    constexpr uint32_t TYPE_MASK = 0x3;

    constexpr uint32_t OPCODE_SHIFT = 8;
    constexpr uint32_t OPCODE_MASK = 0xFF;

    constexpr uint32_t COMPUTE_BIT = 1u << 1;

    // Only opcodes explicitly supplied in the PM4 definitions.
    constexpr uint32_t PACKET3_DISPATCH_DIRECT = 0x15;
    constexpr uint32_t PACKET3_DRAW_INDEX_AUTO = 0x2D;
    constexpr uint32_t PACKET3_DRAW_INDEX_IMMD = 0x2E;
    constexpr uint32_t PACKET3_WRITE_DATA = 0x37;
    constexpr uint32_t PACKET3_COPY_DATA = 0x40;
    constexpr uint32_t PACKET3_EVENT_WRITE = 0x46;

    // GFX8 registers explicitly supplied in si.h.
    constexpr uint32_t VGT_PRIMITIVE_TYPE = 0x2256;
    constexpr uint32_t VGT_INDEX_TYPE     = 0x2257;
    constexpr uint32_t VGT_NUM_INDICES    = 0x225C;
    constexpr uint32_t VGT_NUM_INSTANCES  = 0x225D;

}

bool PM4ToVulkan::Initialize(
    VkDevice device,
    VkPhysicalDevice physicalDevice,
    PM4VulkanBackbuffer* backbuffer)
{
    if (device == VK_NULL_HANDLE)
        return false;

    if (physicalDevice == VK_NULL_HANDLE)
        return false;

    if (backbuffer == nullptr)
        return false;

    m_device = device;
    m_physicalDevice = physicalDevice;
    m_backbuffer = backbuffer;

    m_commandBuffer =
        backbuffer->GetCommandBuffer();

    return m_commandBuffer != VK_NULL_HANDLE;
}

void PM4ToVulkan::Reset()
{
    m_graphicsState = {};

    m_commandBuffer = VK_NULL_HANDLE;

    if (m_backbuffer != nullptr)
    {
        m_commandBuffer =
            m_backbuffer->GetCommandBuffer();
    }
}

VkCommandBuffer PM4ToVulkan::GetCommandBuffer() const
{
    return m_commandBuffer;
}

bool PM4ToVulkan::TranslatePacket(
    const PM4::Packet& packet)
{
    if (m_commandBuffer == VK_NULL_HANDLE)
        return false;

    if (!packet.valid)
        return false;

    switch (packet.header_info.type)
    {
        case PM4::PacketType::Type3:
            return TranslateType3(packet);

        case PM4::PacketType::Type0:
        case PM4::PacketType::Type1:
        case PM4::PacketType::Type2:
            // These packets are decoded by the PM4 layer,
            // but do not directly become Vulkan commands here.
            return true;

        default:
            return false;
    }
}

bool PM4ToVulkan::TranslateStream(
    const uint32_t* stream,
    std::size_t dwordCount)
{
    if (stream == nullptr)
        return false;

    if (m_commandBuffer == VK_NULL_HANDLE)
        return false;

    PM4::Decoder decoder;

    const auto packets =
        decoder.DecodeStream(
            stream,
            dwordCount);

    for (const auto& packet : packets)
    {
        if (!TranslatePacket(packet))
            return false;
    }

    return true;
}

bool PM4ToVulkan::TranslateType3(
    const PM4::Packet& packet)
{
    const uint32_t opcode =
        packet.header_info.opcode;

    switch (opcode)
    {
        case PACKET3_DRAW_INDEX_IMMD:
            return TranslateDrawIndexImmediate(packet);

        case PACKET3_DRAW_INDEX_AUTO:
            return TranslateDrawIndexAuto(packet);

        case PACKET3_DISPATCH_DIRECT:
            return TranslateDispatchDirect(packet);

        case PACKET3_EVENT_WRITE:
            return TranslateEventWrite(packet);

        case PACKET3_COPY_DATA:
            return TranslateCopyData(packet);

        case PACKET3_WRITE_DATA:
            return TranslateWriteData(packet);

        default:
            /*
             * We deliberately do not guess how unsupported
             * PM4 packets should behave.
             *
             * They remain decoded/raw in the PM4 decoder until
             * their exact packet layout and Vulkan translation
             * are established.
             */
            return true;
    }
}

bool PM4ToVulkan::TranslateDrawIndexImmediate(
    const PM4::Packet& packet)
{
    /*
     * DRAW_INDEX_IMMD requires the exact PS4/GFX8 packet
     * layout and the translated graphics pipeline/state.
     *
     * We have the opcode, but we do NOT have enough verified
     * field definitions here to safely construct vkCmdDrawIndexed.
     *
     * Therefore this stays intentionally unimplemented.
     */
    (void)packet;

    return true;
}

bool PM4ToVulkan::TranslateDrawIndexAuto(
    const PM4::Packet& packet)
{
    /*
     * Same rule as DRAW_INDEX_IMMD:
     * don't invent the packet fields or PS4 register semantics.
     */
    (void)packet;

    return true;
}

bool PM4ToVulkan::TranslateDispatchDirect(
    const PM4::Packet& packet)
{
    /*
     * The established packet layout is:
     *
     * payload[0] = 8
     * payload[1] = 1
     * payload[2] = 1
     * payload[3] = compute shader enable field
     *
     * However, Vulkan dispatch dimensions still require
     * verified interpretation of the packet fields.
     *
     * Do not manufacture vkCmdDispatch dimensions.
     */
    if (packet.payload.size() < 4)
        return false;

    return true;
}

bool PM4ToVulkan::TranslateEventWrite(
    const PM4::Packet& packet)
{
    /*
     * EVENT_WRITE is a synchronization/event packet.
     *
     * Vulkan has several possible synchronization mechanisms,
     * but EVENT_WRITE's exact PS4 semantics cannot simply be
     * replaced with an arbitrary Vulkan command.
     */
    if (packet.payload.empty())
        return false;

    return true;
}
bool PM4ToVulkan::TranslateSetContextReg(
    const PM4::Packet& packet)
{
    if (packet.payload.empty())
        return false;

    const uint32_t registerOffset =
        packet.payload[0];

const uint32_t firstRegister =
    0x000A000 + registerOffset;

    for (std::size_t i = 1;
         i < packet.payload.size();
         ++i)
    {
        const uint32_t reg =
            firstRegister +
            static_cast<uint32_t>(i - 1);

        UpdateContextRegister(
            reg,
            packet.payload[i]);
    }

    return true;
}
bool PM4ToVulkan::TranslateCopyData(
    const PM4::Packet& packet)
{
    /*
     * COPY_DATA packet layout established from gfx_v8_0.c:
     *
     * payload[0] = control
     * payload[1] = source register
     * payload[2] = 0
     * payload[3] = destination address low
     * payload[4] = destination address high
     *
     * This is not directly representable as a Vulkan command
     * without the emulator's GPU-memory/register model.
     */
    if (packet.payload.size() < 5)
        return false;

    return true;
}

bool PM4ToVulkan::TranslateWriteData(
    const PM4::Packet& packet)
{
    /*
     * WRITE_DATA:
     *
     * payload[0] = control
     * payload[1] = address low
     * payload[2] = address high
     * payload[3...] = data
     *
     * Vulkan does not provide a generic "write PS4 GPU
     * register/memory" command. This belongs to the emulator's
     * memory/register subsystem.
     */
    if (packet.payload.size() < 4)
        return false;

    return true;
}
void PM4ToVulkan::SetPipeline(
    VkPipeline pipeline,
    VkPipelineLayout pipelineLayout)
{
    m_graphicsState.pipeline = pipeline;
    m_graphicsState.pipelineLayout = pipelineLayout;

    m_graphicsState.pipelineBound =
        pipeline != VK_NULL_HANDLE;
}

void PM4ToVulkan::SetVertexBuffer(
    VkBuffer buffer,
    VkDeviceSize offset)
{
    m_graphicsState.vertexBuffer = buffer;
    m_graphicsState.vertexOffset = offset;

    m_graphicsState.vertexBufferBound =
        buffer != VK_NULL_HANDLE;
}

void PM4ToVulkan::SetIndexBuffer(
    VkBuffer buffer,
    VkDeviceSize offset,
    VkIndexType indexType)
{
    m_graphicsState.indexBuffer = buffer;
    m_graphicsState.indexOffset = offset;
    m_graphicsState.indexType = indexType;

    m_graphicsState.indexBufferBound =
        buffer != VK_NULL_HANDLE;
}

bool PM4ToVulkan::DrawIndexed(
    uint32_t indexCount,
    uint32_t instanceCount,
    uint32_t firstIndex,
    int32_t vertexOffset,
    uint32_t firstInstance)
{
    if (m_commandBuffer == VK_NULL_HANDLE)
        return false;

    if (!m_graphicsState.pipelineBound)
        return false;

    if (!m_graphicsState.indexBufferBound)
        return false;

    vkCmdBindPipeline(
        m_commandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        m_graphicsState.pipeline);

    if (m_graphicsState.vertexBufferBound)
    {
        vkCmdBindVertexBuffers(
            m_commandBuffer,
            0,
            1,
            &m_graphicsState.vertexBuffer,
            &m_graphicsState.vertexOffset);
    }

    vkCmdBindIndexBuffer(
        m_commandBuffer,
        m_graphicsState.indexBuffer,
        m_graphicsState.indexOffset,
        m_graphicsState.indexType);

    vkCmdDrawIndexed(
        m_commandBuffer,
        indexCount,
        instanceCount,
        firstIndex,
        vertexOffset,
        firstInstance);

    return true;
}

const PM4GraphicsState&
PM4ToVulkan::GetGraphicsState() const
{
    return m_graphicsState;
}
void PM4ToVulkan::UpdateContextRegister(
    uint32_t reg,
    uint32_t value)
{
    switch (reg)
    {
        case VGT_PRIMITIVE_TYPE:
            m_graphicsState.primitiveType = value;
            break;

        case VGT_INDEX_TYPE:
            /*
             * Keep the raw GFX8 register value here.
             *
             * We do not guess its bit layout yet.
             */
            break;

        case VGT_NUM_INDICES:
            m_graphicsState.numIndices = value;
            break;

        case VGT_NUM_INSTANCES:
            m_graphicsState.numInstances = value;
            break;

        default:
            break;
    }
}