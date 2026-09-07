#include "PM4-decoder.h"

#include <iomanip>
#include <sstream>

namespace PM4 {

namespace {

constexpr uint32_t TYPE_SHIFT = 30;
constexpr uint32_t TYPE_MASK = 0x3;

constexpr uint32_t TYPE0_REG_MASK = 0xFFFF;
constexpr uint32_t TYPE0_COUNT_MASK = 0x3FFF;

constexpr uint32_t TYPE3_OPCODE_SHIFT = 8;
constexpr uint32_t TYPE3_OPCODE_MASK = 0xFF;
constexpr uint32_t TYPE3_COUNT_MASK = 0x3FFF;

// -----------------------------------------------------------------------------
// PM4 opcodes supplied by the user.
// Do NOT add values here unless they are explicitly established.
// -----------------------------------------------------------------------------

enum Opcode : uint32_t {
    OP_NOP                       = 0x10,
    OP_SET_BASE                  = 0x11,
    OP_CLEAR_STATE               = 0x12,
    OP_INDEX_BUFFER_SIZE        = 0x13,
    OP_DISPATCH_DIRECT          = 0x15,
    OP_DISPATCH_INDIRECT        = 0x16,

    OP_ALLOC_GDS                = 0x1B,
    OP_WRITE_GDS_RAM            = 0x1C,
    OP_ATOMIC_GDS               = 0x1D,
    OP_ATOMIC                   = 0x1E,
    OP_OCCLUSION_QUERY          = 0x1F,

    OP_SET_PREDICATION          = 0x20,
    OP_REG_RMW                  = 0x21,
    OP_COND_EXEC                = 0x22,
    OP_PRED_EXEC                = 0x23,

    OP_DRAW_INDIRECT             = 0x24,
    OP_DRAW_INDEX_INDIRECT       = 0x25,
    OP_INDEX_BASE                = 0x26,
    OP_DRAW_INDEX_2              = 0x27,

    OP_CONTEXT_CONTROL           = 0x28,

    OP_INDEX_TYPE                = 0x2A,

    OP_DRAW_INDIRECT_MULTI       = 0x2C,
    OP_DRAW_INDEX_AUTO           = 0x2D,
    OP_DRAW_INDEX_IMMD           = 0x2E,
    OP_NUM_INSTANCES             = 0x2F,
    OP_DRAW_INDEX_MULTI_AUTO     = 0x30,

    OP_INDIRECT_BUFFER_CONST     = 0x31,

    OP_STRMOUT_BUFFER_UPDATE     = 0x34,
    OP_DRAW_INDEX_OFFSET_2       = 0x35,
    OP_DRAW_INDEX_MULTI_ELEMENT  = 0x36,

    OP_WRITE_DATA                = 0x37,
    OP_DRAW_INDEX_INDIRECT_MULTI = 0x38,

    OP_MEM_SEMAPHORE             = 0x39,
    OP_MPEG_INDEX                = 0x3A,
    OP_COPY_DW                   = 0x3B,
    OP_WAIT_REG_MEM              = 0x3C,
    OP_MEM_WRITE                 = 0x3D,

    OP_INDIRECT_BUFFER           = 0x3F,
    OP_COPY_DATA                 = 0x40,
    OP_CP_DMA                    = 0x41,
    OP_PFP_SYNC_ME               = 0x42,
    OP_SURFACE_SYNC              = 0x43,
    OP_ME_INITIALIZE             = 0x44,
    OP_COND_WRITE                = 0x45,
    OP_EVENT_WRITE               = 0x46,
    OP_EVENT_WRITE_EOP           = 0x47,
    OP_EVENT_WRITE_EOS           = 0x48,
    OP_RELEASE_MEM               = 0x49,
    OP_PREAMBLE_CNTL             = 0x4A,

    OP_ONE_REG_WRITE             = 0x57,
    OP_ACQUIRE_MEM               = 0x58,

    OP_LOAD_CONFIG_REG           = 0x5F,
    OP_LOAD_CONTEXT_REG          = 0x60,
    OP_LOAD_SH_REG               = 0x61,

    OP_SET_CONFIG_REG            = 0x68,
    OP_SET_CONTEXT_REG           = 0x69,

    OP_SET_CONTEXT_REG_INDIRECT  = 0x73,
    OP_SET_RESOURCE_INDIRECT     = 0x74,

    OP_SET_SH_REG                = 0x76,
    OP_SET_SH_REG_OFFSET         = 0x77,

    OP_ME_WRITE                  = 0x7A,

    OP_SCRATCH_RAM_WRITE         = 0x7D,
    OP_SCRATCH_RAM_READ          = 0x7E,
    OP_CE_WRITE                  = 0x7F,

    OP_LOAD_CONST_RAM            = 0x80,
    OP_WRITE_CONST_RAM           = 0x81,
    OP_WRITE_CONST_RAM_OFFSET    = 0x82,
    OP_DUMP_CONST_RAM            = 0x83,

    OP_INCREMENT_CE_COUNTER      = 0x84,
    OP_INCREMENT_DE_COUNTER      = 0x85,
    OP_WAIT_ON_CE_COUNTER        = 0x86,
    OP_WAIT_ON_DE_COUNTER        = 0x87,
    OP_WAIT_ON_DE_COUNTER_DIFF   = 0x88,
    OP_SET_CE_DE_COUNTERS        = 0x89,
    OP_WAIT_ON_AVAIL_BUFFER      = 0x8A,
    OP_SWITCH_BUFFER             = 0x8B
};

// -----------------------------------------------------------------------------
// WRITE_DATA
// -----------------------------------------------------------------------------

constexpr unsigned WRITE_DATA_DST_SEL_SHIFT = 8;
constexpr uint32_t WRITE_DATA_DST_SEL_MASK = 0x7;

constexpr unsigned WRITE_DATA_ENGINE_SEL_SHIFT = 30;
constexpr uint32_t WRITE_DATA_ENGINE_SEL_MASK = 0x3;

constexpr uint32_t WR_ONE_ADDR = 1u << 16;
constexpr uint32_t WR_CONFIRM  = 1u << 20;

// -----------------------------------------------------------------------------
// WAIT_REG_MEM
// -----------------------------------------------------------------------------

constexpr unsigned WAIT_FUNCTION_SHIFT = 0;
constexpr uint32_t WAIT_FUNCTION_MASK = 0x7;

constexpr unsigned WAIT_MEM_SPACE_SHIFT = 4;
constexpr uint32_t WAIT_MEM_SPACE_MASK = 0x1;

constexpr unsigned WAIT_ENGINE_SHIFT = 8;
constexpr uint32_t WAIT_ENGINE_MASK = 0x3;

// -----------------------------------------------------------------------------
// CP_DMA
// -----------------------------------------------------------------------------

constexpr unsigned CP_DMA_DST_SEL_SHIFT = 20;
constexpr uint32_t CP_DMA_DST_SEL_MASK = 0x1;

constexpr unsigned CP_DMA_ENGINE_SHIFT = 27;
constexpr uint32_t CP_DMA_ENGINE_MASK = 0x1;

constexpr unsigned CP_DMA_SRC_SEL_SHIFT = 29;
constexpr uint32_t CP_DMA_SRC_SEL_MASK = 0x3;

constexpr uint32_t CP_DMA_CP_SYNC = 1u << 31;

constexpr uint32_t CP_DMA_DIS_WC   = 1u << 21;
constexpr uint32_t CP_DMA_SAS      = 1u << 26;
constexpr uint32_t CP_DMA_DAS      = 1u << 27;
constexpr uint32_t CP_DMA_SAIC     = 1u << 28;
constexpr uint32_t CP_DMA_DAIC     = 1u << 29;
constexpr uint32_t CP_DMA_RAW_WAIT = 1u << 30;

constexpr unsigned CP_DMA_SRC_SWAP_SHIFT = 22;
constexpr unsigned CP_DMA_DST_SWAP_SHIFT = 24;

// -----------------------------------------------------------------------------
// EVENT
// -----------------------------------------------------------------------------

constexpr unsigned EVENT_TYPE_SHIFT = 0;
constexpr uint32_t EVENT_TYPE_MASK = 0xFF;

constexpr unsigned EVENT_INDEX_SHIFT = 8;
constexpr uint32_t EVENT_INDEX_MASK = 0xF;

constexpr uint32_t INV_L2 = 1u << 20;

// -----------------------------------------------------------------------------
// EOP
// -----------------------------------------------------------------------------

constexpr unsigned INT_SEL_SHIFT = 24;
constexpr uint32_t INT_SEL_MASK = 0x3;

constexpr unsigned DATA_SEL_SHIFT = 29;
constexpr uint32_t DATA_SEL_MASK = 0x7;

// -----------------------------------------------------------------------------
// PREAMBLE
// -----------------------------------------------------------------------------

constexpr unsigned PREAMBLE_MODE_SHIFT = 28;
constexpr uint32_t PREAMBLE_MODE_MASK = 0xF;

// -----------------------------------------------------------------------------
// SET_BASE
// -----------------------------------------------------------------------------

constexpr uint32_t GDS_PARTITION_BASE = 2;
constexpr uint32_t CE_PARTITION_BASE = 3;

// -----------------------------------------------------------------------------
// Register ranges supplied by the user.
// -----------------------------------------------------------------------------

constexpr uint32_t SET_CONFIG_REG_START   = 0x00002000;
constexpr uint32_t SET_CONFIG_REG_END     = 0x00002C00;

constexpr uint32_t SET_CONTEXT_REG_START  = 0x000A000;
constexpr uint32_t SET_CONTEXT_REG_END    = 0x000A400;

constexpr uint32_t SET_SH_REG_START       = 0x00002C00;
constexpr uint32_t SET_SH_REG_END         = 0x00003000;

} // namespace

// ============================================================================
// Utility
// ============================================================================

std::string Decoder::Hex(uint32_t value)
{
    std::ostringstream ss;
    ss << "0x"
       << std::hex
       << std::uppercase
       << std::setw(8)
       << std::setfill('0')
       << value;
    return ss.str();
}

std::string Decoder::Hex64(uint64_t value)
{
    std::ostringstream ss;
    ss << "0x"
       << std::hex
       << std::uppercase
       << std::setw(16)
       << std::setfill('0')
       << value;
    return ss.str();
}

uint32_t Decoder::ReadPayload(const Packet& packet,
                              std::size_t index)
{
    if (index >= packet.payload.size())
        return 0;

    return packet.payload[index];
}

uint64_t Decoder::MakeAddress(uint32_t low,
                              uint32_t high,
                              unsigned high_bits)
{
    if (high_bits >= 32)
        return (static_cast<uint64_t>(high) << 32) |
               static_cast<uint64_t>(low);

    const uint32_t mask =
        high_bits == 0
            ? 0
            : ((1u << high_bits) - 1u);

    return (static_cast<uint64_t>(high & mask) << 32) |
           static_cast<uint64_t>(low);
}

// ============================================================================
// Header decoding
// ============================================================================

PacketType Decoder::GetPacketType(uint32_t header)
{
    switch ((header >> TYPE_SHIFT) & TYPE_MASK) {
    case 0:
        return PacketType::Type0;

    case 1:
        return PacketType::Type1;

    case 2:
        return PacketType::Type2;

    case 3:
        return PacketType::Type3;

    default:
        return PacketType::Unknown;
    }
}

std::size_t Decoder::Type0Count(uint32_t header)
{
    return static_cast<std::size_t>(
        ((header >> 16) & TYPE0_COUNT_MASK) + 1
    );
}

std::size_t Decoder::Type1Count(uint32_t)
{
    // Type-1 format was not supplied.
    // Keep it opaque rather than inventing a layout.
    return 0;
}

std::size_t Decoder::Type2Count(uint32_t)
{
    return 1;
}

std::size_t Decoder::Type3Count(uint32_t header)
{
    return static_cast<std::size_t>(
        ((header >> 16) & TYPE3_COUNT_MASK) + 1
    );
}

PacketHeader Decoder::DecodeHeader(uint32_t header)
{
    PacketHeader result{};

    result.raw = header;
    result.type = GetPacketType(header);

    switch (result.type) {
    case PacketType::Type0:
        result.register_offset = header & TYPE0_REG_MASK;
        result.count =
            static_cast<uint32_t>(Type0Count(header));
        break;

    case PacketType::Type1:
        result.count =
            static_cast<uint32_t>(Type1Count(header));
        break;

    case PacketType::Type2:
        result.count =
            static_cast<uint32_t>(Type2Count(header));
        break;

    case PacketType::Type3:
        result.opcode =
            (header >> TYPE3_OPCODE_SHIFT) &
            TYPE3_OPCODE_MASK;

        result.compute =
            (header & (1u << 1)) != 0;

        result.count =
            static_cast<uint32_t>(Type3Count(header));
        break;

    default:
        break;
    }

    return result;
}

// ============================================================================
// Names
// ============================================================================

std::string Decoder::PacketTypeName(PacketType type)
{
    switch (type) {
    case PacketType::Type0: return "TYPE0";
    case PacketType::Type1: return "TYPE1";
    case PacketType::Type2: return "TYPE2";
    case PacketType::Type3: return "TYPE3";
    default: return "UNKNOWN";
    }
}

std::string Decoder::OpcodeName(uint32_t opcode)
{
    switch (opcode) {
    case OP_NOP:                       return "NOP";
    case OP_SET_BASE:                  return "SET_BASE";
    case OP_CLEAR_STATE:               return "CLEAR_STATE";
    case OP_INDEX_BUFFER_SIZE:         return "INDEX_BUFFER_SIZE";
    case OP_DISPATCH_DIRECT:           return "DISPATCH_DIRECT";
    case OP_DISPATCH_INDIRECT:         return "DISPATCH_INDIRECT";

    case OP_ALLOC_GDS:                 return "ALLOC_GDS";
    case OP_WRITE_GDS_RAM:             return "WRITE_GDS_RAM";
    case OP_ATOMIC_GDS:                return "ATOMIC_GDS";
    case OP_ATOMIC:                    return "ATOMIC";
    case OP_OCCLUSION_QUERY:           return "OCCLUSION_QUERY";

    case OP_SET_PREDICATION:           return "SET_PREDICATION";
    case OP_REG_RMW:                   return "REG_RMW";
    case OP_COND_EXEC:                 return "COND_EXEC";
    case OP_PRED_EXEC:                 return "PRED_EXEC";

    case OP_DRAW_INDIRECT:             return "DRAW_INDIRECT";
    case OP_DRAW_INDEX_INDIRECT:       return "DRAW_INDEX_INDIRECT";
    case OP_INDEX_BASE:                return "INDEX_BASE";
    case OP_DRAW_INDEX_2:              return "DRAW_INDEX_2";

    case OP_CONTEXT_CONTROL:           return "CONTEXT_CONTROL";
    case OP_INDEX_TYPE:                return "INDEX_TYPE";

    case OP_DRAW_INDIRECT_MULTI:       return "DRAW_INDIRECT_MULTI";
    case OP_DRAW_INDEX_AUTO:           return "DRAW_INDEX_AUTO";
    case OP_DRAW_INDEX_IMMD:           return "DRAW_INDEX_IMMD";
    case OP_NUM_INSTANCES:             return "NUM_INSTANCES";
    case OP_DRAW_INDEX_MULTI_AUTO:     return "DRAW_INDEX_MULTI_AUTO";

    case OP_INDIRECT_BUFFER_CONST:     return "INDIRECT_BUFFER_CONST";

    case OP_STRMOUT_BUFFER_UPDATE:     return "STRMOUT_BUFFER_UPDATE";
    case OP_DRAW_INDEX_OFFSET_2:       return "DRAW_INDEX_OFFSET_2";
    case OP_DRAW_INDEX_MULTI_ELEMENT:  return "DRAW_INDEX_MULTI_ELEMENT";

    case OP_WRITE_DATA:                return "WRITE_DATA";
    case OP_DRAW_INDEX_INDIRECT_MULTI: return "DRAW_INDEX_INDIRECT_MULTI";

    case OP_MEM_SEMAPHORE:             return "MEM_SEMAPHORE";
    case OP_MPEG_INDEX:                return "MPEG_INDEX";
    case OP_COPY_DW:                   return "COPY_DW";
    case OP_WAIT_REG_MEM:              return "WAIT_REG_MEM";
    case OP_MEM_WRITE:                 return "MEM_WRITE";

    case OP_INDIRECT_BUFFER:            return "INDIRECT_BUFFER";
    case OP_COPY_DATA:                  return "COPY_DATA";
    case OP_CP_DMA:                    return "CP_DMA";
    case OP_PFP_SYNC_ME:               return "PFP_SYNC_ME";
    case OP_SURFACE_SYNC:              return "SURFACE_SYNC";
    case OP_ME_INITIALIZE:             return "ME_INITIALIZE";
    case OP_COND_WRITE:                return "COND_WRITE";
    case OP_EVENT_WRITE:               return "EVENT_WRITE";
    case OP_EVENT_WRITE_EOP:           return "EVENT_WRITE_EOP";
    case OP_EVENT_WRITE_EOS:           return "EVENT_WRITE_EOS";
    case OP_RELEASE_MEM:               return "RELEASE_MEM";
    case OP_PREAMBLE_CNTL:             return "PREAMBLE_CNTL";

    case OP_ONE_REG_WRITE:             return "ONE_REG_WRITE";
    case OP_ACQUIRE_MEM:               return "ACQUIRE_MEM";

    case OP_LOAD_CONFIG_REG:           return "LOAD_CONFIG_REG";
    case OP_LOAD_CONTEXT_REG:          return "LOAD_CONTEXT_REG";
    case OP_LOAD_SH_REG:               return "LOAD_SH_REG";

    case OP_SET_CONFIG_REG:            return "SET_CONFIG_REG";
    case OP_SET_CONTEXT_REG:           return "SET_CONTEXT_REG";

    case OP_SET_CONTEXT_REG_INDIRECT:  return "SET_CONTEXT_REG_INDIRECT";
    case OP_SET_RESOURCE_INDIRECT:     return "SET_RESOURCE_INDIRECT";

    case OP_SET_SH_REG:                return "SET_SH_REG";
    case OP_SET_SH_REG_OFFSET:         return "SET_SH_REG_OFFSET";

    case OP_ME_WRITE:                  return "ME_WRITE";

    case OP_SCRATCH_RAM_WRITE:         return "SCRATCH_RAM_WRITE";
    case OP_SCRATCH_RAM_READ:          return "SCRATCH_RAM_READ";
    case OP_CE_WRITE:                  return "CE_WRITE";

    case OP_LOAD_CONST_RAM:            return "LOAD_CONST_RAM";
    case OP_WRITE_CONST_RAM:           return "WRITE_CONST_RAM";
    case OP_WRITE_CONST_RAM_OFFSET:    return "WRITE_CONST_RAM_OFFSET";
    case OP_DUMP_CONST_RAM:            return "DUMP_CONST_RAM";

    case OP_INCREMENT_CE_COUNTER:      return "INCREMENT_CE_COUNTER";
    case OP_INCREMENT_DE_COUNTER:      return "INCREMENT_DE_COUNTER";
    case OP_WAIT_ON_CE_COUNTER:        return "WAIT_ON_CE_COUNTER";
    case OP_WAIT_ON_DE_COUNTER:        return "WAIT_ON_DE_COUNTER";
    case OP_WAIT_ON_DE_COUNTER_DIFF:   return "WAIT_ON_DE_COUNTER_DIFF";
    case OP_SET_CE_DE_COUNTERS:        return "SET_CE_DE_COUNTERS";
    case OP_WAIT_ON_AVAIL_BUFFER:      return "WAIT_ON_AVAIL_BUFFER";
    case OP_SWITCH_BUFFER:             return "SWITCH_BUFFER";

    default:
        return "UNKNOWN_OPCODE";
    }
}

// ============================================================================
// Registers
// ============================================================================

std::string Decoder::RegisterName(uint32_t reg)
{
    switch (reg) {
    case 0xCA00: return "CIK_DIDT_IND_INDEX";
    case 0xCA04: return "CIK_DIDT_IND_DATA";

    case 0x65B0: return "CIK_DC_GPIO_HPD_MASK";
    case 0x65B4: return "CIK_DC_GPIO_HPD_A";
    case 0x65B8: return "CIK_DC_GPIO_HPD_EN";
    case 0x65BC: return "CIK_DC_GPIO_HPD_Y";

    case 0x6804: return "CIK_GRPH_CONTROL";

    case 0x6998: return "CIK_CUR_CONTROL";
    case 0x699C: return "CIK_CUR_SURFACE_ADDRESS";
    case 0x69A0: return "CIK_CUR_SIZE";
    case 0x69A4: return "CIK_CUR_SURFACE_ADDRESS_HIGH";
    case 0x69A8: return "CIK_CUR_POSITION";
    case 0x69AC: return "CIK_CUR_HOT_SPOT";
    case 0x69B0: return "CIK_CUR_COLOR1";
    case 0x69B4: return "CIK_CUR_COLOR2";
    case 0x69B8: return "CIK_CUR_UPDATE";

    case 0x6AF0: return "CIK_ALPHA_CONTROL";
    case 0x6B00: return "CIK_LB_DATA_FORMAT";
    case 0x6B0C: return "CIK_LB_DESKTOP_HEIGHT";

    case 0x8DE0: return "SQ_IND_INDEX";
    case 0x8DEC: return "SQ_CMD";
    case 0x8DE4: return "SQ_IND_DATA";

    case 0xC2D0: return "CPC_INT_CNTL";
    case 0xC970: return "CP_HQD_IQ_RPTR";
    case 0xD400: return "SDMA0_RLC0_RB_CNTL";

    default:
        return {};
    }
}

// ============================================================================
// Events
// ============================================================================

std::string Decoder::EventName(uint32_t event_type)
{
    switch (event_type) {
    case 0:
        return "GENERIC/VGT_FLUSH";

    case 4:
        return "VS_PARTIAL_FLUSH";

    case 7:
        return "CACHE_FLUSH/CACHE_FLUSH_AND_INV";

    default: {
        std::ostringstream ss;
        ss << "EVENT_" << event_type;
        return ss.str();
    }
    }
}

// ============================================================================
// Raw payload
// ============================================================================

void Decoder::DumpRawPayload(const Packet& packet,
                             std::string& out)
{
    std::ostringstream ss;

    ss << "payload[" << packet.payload.size() << "] =";

    for (std::size_t i = 0; i < packet.payload.size(); ++i)
        ss << " [" << i << "]=" << Hex(packet.payload[i]);

    out += ss.str();
}

// ============================================================================
// TYPE 0
// ============================================================================

void Decoder::DecodeType0(Packet& packet)
{
    const uint32_t base =
        packet.header_info.register_offset;

    std::ostringstream ss;

    ss << "TYPE0 register write: "
       << "start=" << Hex(base)
       << ", count=" << packet.payload.size();

    for (std::size_t i = 0; i < packet.payload.size(); ++i) {
        const uint32_t reg =
            base + static_cast<uint32_t>(i * 4);

        ss << "\n  "
           << Hex(reg);

        const std::string name =
            RegisterName(reg);

        if (!name.empty())
            ss << " (" << name << ")";

        ss << " = "
           << Hex(packet.payload[i]);
    }

    packet.description = ss.str();
}

// ============================================================================
// TYPE 1
// ============================================================================

void Decoder::DecodeType1(Packet& packet)
{
    packet.name = "TYPE1";

    packet.description =
        "TYPE1 packet format not supplied; "
        "preserving packet as opaque";
}

// ============================================================================
// TYPE 2
// ============================================================================

void Decoder::DecodeType2(Packet& packet)
{
    packet.name = "TYPE2";

    if (!packet.payload.empty()) {
        std::ostringstream ss;

        ss << "TYPE2 padding = "
           << Hex(packet.payload[0]);

        packet.description = ss.str();
    } else {
        packet.description = "TYPE2 padding";
    }
}

// ============================================================================
// WRITE_DATA
// ============================================================================

void Decoder::DecodeWriteData(Packet& packet)
{
    if (packet.payload.size() < 3) {
        packet.description =
            "WRITE_DATA: truncated";
        return;
    }

    const uint32_t control = packet.payload[0];

    const uint32_t dst_sel =
        (control >> WRITE_DATA_DST_SEL_SHIFT) &
        WRITE_DATA_DST_SEL_MASK;

    const uint32_t engine =
        (control >> WRITE_DATA_ENGINE_SEL_SHIFT) &
        WRITE_DATA_ENGINE_SEL_MASK;

    const uint64_t address =
        MakeAddress(packet.payload[1],
                    packet.payload[2],
                    16);

    std::ostringstream ss;

    ss << "WRITE_DATA"
       << ": dst_sel=" << dst_sel
       << ", engine=" << engine
       << ", one_addr="
       << ((control & WR_ONE_ADDR) != 0)
       << ", confirm="
       << ((control & WR_CONFIRM) != 0);

    if (dst_sel == 0) {
        ss << ", register="
           << Hex(packet.payload[1]);

        const std::string name =
            RegisterName(packet.payload[1]);

        if (!name.empty())
            ss << " (" << name << ")";
    } else {
        ss << ", address="
           << Hex64(address);
    }

    ss << ", data_dwords="
       << (packet.payload.size() - 3);

    for (std::size_t i = 3;
         i < packet.payload.size();
         ++i) {
        ss << "\n  data[" << (i - 3) << "]="
           << Hex(packet.payload[i]);
    }

    packet.description = ss.str();
}

// ============================================================================
// COPY_DATA
// ============================================================================

void Decoder::DecodeCopyData(Packet& packet)
{
    if (packet.payload.size() < 5) {
        packet.description =
            "COPY_DATA: truncated";
        return;
    }

    const uint32_t control = packet.payload[0];

    std::ostringstream ss;

    ss << "COPY_DATA"
       << ": control=" << Hex(control)
       << ", payload[1]=" << Hex(packet.payload[1])
       << ", payload[2]=" << Hex(packet.payload[2])
       << ", payload[3]=" << Hex(packet.payload[3])
       << ", payload[4]=" << Hex(packet.payload[4]);

    packet.description = ss.str();
}

// ============================================================================
// WAIT_REG_MEM
// ============================================================================

void Decoder::DecodeWaitRegMem(Packet& packet)
{
    if (packet.payload.size() < 6) {
        packet.description =
            "WAIT_REG_MEM: truncated";
        return;
    }

    const uint32_t operation = packet.payload[0];

    const uint32_t function =
        (operation >> WAIT_FUNCTION_SHIFT) &
        WAIT_FUNCTION_MASK;

    const uint32_t mem_space =
        (operation >> WAIT_MEM_SPACE_SHIFT) &
        WAIT_MEM_SPACE_MASK;

    const uint32_t engine =
        (operation >> WAIT_ENGINE_SHIFT) &
        WAIT_ENGINE_MASK;

    const uint64_t address =
        MakeAddress(packet.payload[1],
                    packet.payload[2],
                    32);

    std::ostringstream ss;

    ss << "WAIT_REG_MEM"
       << ": function=" << function
       << ", mem_space=" << mem_space
       << ", engine=" << engine
       << ", address=" << Hex64(address)
       << ", reference=" << Hex(packet.payload[3])
       << ", mask=" << Hex(packet.payload[4])
       << ", poll_interval=" << Hex(packet.payload[5]);

    packet.description = ss.str();
}

// ============================================================================
// CP_DMA
// ============================================================================

void Decoder::DecodeCpDma(Packet& packet)
{
    if (packet.payload.size() < 5) {
        packet.description =
            "CP_DMA: truncated";
        return;
    }

    const uint32_t control = packet.payload[1];
    const uint32_t command = packet.payload[4];

    const uint32_t dst_sel =
        (control >> CP_DMA_DST_SEL_SHIFT) &
        CP_DMA_DST_SEL_MASK;

    const uint32_t engine =
        (control >> CP_DMA_ENGINE_SHIFT) &
        CP_DMA_ENGINE_MASK;

    const uint32_t src_sel =
        (control >> CP_DMA_SRC_SEL_SHIFT) &
        CP_DMA_SRC_SEL_MASK;

    const uint64_t src_or_data =
        MakeAddress(packet.payload[0],
                    control & 0xFF,
                    8);

    const uint64_t dst =
        MakeAddress(packet.payload[2],
                    packet.payload[3],
                    8);

    const uint32_t byte_count =
        command & 0x1FFFFF;

    const uint32_t src_swap =
        (command >> CP_DMA_SRC_SWAP_SHIFT) & 0x3;

    const uint32_t dst_swap =
        (command >> CP_DMA_DST_SWAP_SHIFT) & 0x3;

    std::ostringstream ss;

    ss << "CP_DMA"
       << ": src_sel=" << src_sel
       << ", dst_sel=" << dst_sel
       << ", engine=" << engine
       << ", cp_sync="
       << ((control & CP_DMA_CP_SYNC) != 0)
       << ", src_or_data=" << Hex64(src_or_data)
       << ", dst=" << Hex64(dst)
       << ", byte_count=" << byte_count
       << ", src_swap=" << src_swap
       << ", dst_swap=" << dst_swap
       << ", dis_wc="
       << ((command & CP_DMA_DIS_WC) != 0)
       << ", sas="
       << ((command & CP_DMA_SAS) != 0)
       << ", das="
       << ((command & CP_DMA_DAS) != 0)
       << ", saic="
       << ((command & CP_DMA_SAIC) != 0)
       << ", daic="
       << ((command & CP_DMA_DAIC) != 0)
       << ", raw_wait="
       << ((command & CP_DMA_RAW_WAIT) != 0);

    packet.description = ss.str();
}

// ============================================================================
// SURFACE_SYNC
// ============================================================================

void Decoder::DecodeSurfaceSync(Packet& packet)
{
    if (packet.payload.size() < 4) {
        packet.description =
            "SURFACE_SYNC: truncated";
        return;
    }

    const uint32_t flags = packet.payload[0];

    std::ostringstream ss;

    ss << "SURFACE_SYNC"
       << ": TCL1="
       << ((flags & (1u << 22)) != 0)
       << ", TC="
       << ((flags & (1u << 23)) != 0)
       << ", SH_KCACHE="
       << ((flags & (1u << 27)) != 0)
       << ", SH_ICACHE="
       << ((flags & (1u << 29)) != 0)
       << ", coher_size="
       << Hex(packet.payload[1])
       << ", coher_base="
       << Hex(packet.payload[2])
       << ", poll_interval="
       << Hex(packet.payload[3]);

    packet.description = ss.str();
}

// ============================================================================
// ACQUIRE_MEM
// ============================================================================

void Decoder::DecodeAcquireMem(Packet& packet)
{
    if (packet.payload.size() < 6) {
        packet.description =
            "ACQUIRE_MEM: truncated";
        return;
    }

    const uint32_t flags = packet.payload[0];

    std::ostringstream ss;

    ss << "ACQUIRE_MEM"
       << ": TCL1="
       << ((flags & (1u << 22)) != 0)
       << ", TC="
       << ((flags & (1u << 23)) != 0)
       << ", SH_KCACHE="
       << ((flags & (1u << 27)) != 0)
       << ", SH_ICACHE="
       << ((flags & (1u << 29)) != 0)
       << ", coher_size="
       << Hex(packet.payload[1])
       << ", coher_size_hi="
       << Hex(packet.payload[2])
       << ", coher_base="
       << Hex(packet.payload[3])
       << ", coher_base_hi="
       << Hex(packet.payload[4])
       << ", poll_interval="
       << Hex(packet.payload[5]);

    packet.description = ss.str();
}

// ============================================================================
// EVENT_WRITE
// ============================================================================

void Decoder::DecodeEventWrite(Packet& packet)
{
    if (packet.payload.empty()) {
        packet.description =
            "EVENT_WRITE: missing payload";
        return;
    }

    const uint32_t value = packet.payload[0];

    const uint32_t event_type =
        (value >> EVENT_TYPE_SHIFT) &
        EVENT_TYPE_MASK;

    const uint32_t event_index =
        (value >> EVENT_INDEX_SHIFT) &
        EVENT_INDEX_MASK;

    std::ostringstream ss;

    ss << "EVENT_WRITE"
       << ": event_type=" << event_type
       << " (" << EventName(event_type) << ")"
       << ", event_index=" << event_index;

    packet.description = ss.str();
}

// ============================================================================
// EVENT_WRITE_EOP
// ============================================================================

void Decoder::DecodeEventWriteEop(Packet& packet)
{
    if (packet.payload.size() < 5) {
        packet.description =
            "EVENT_WRITE_EOP: truncated";
        return;
    }

    const uint32_t event_control =
        packet.payload[0];

    const uint32_t data_control =
        packet.payload[2];

    const uint32_t event_type =
        (event_control >> EVENT_TYPE_SHIFT) &
        EVENT_TYPE_MASK;

    const uint32_t event_index =
        (event_control >> EVENT_INDEX_SHIFT) &
        EVENT_INDEX_MASK;

    const uint32_t data_sel =
        (data_control >> DATA_SEL_SHIFT) &
        DATA_SEL_MASK;

    const uint32_t int_sel =
        (data_control >> INT_SEL_SHIFT) &
        INT_SEL_MASK;

    const uint64_t address =
        MakeAddress(packet.payload[1],
                    data_control & 0xFFFF,
                    16);

    const uint64_t sequence =
        (static_cast<uint64_t>(packet.payload[4]) << 32) |
        packet.payload[3];

    std::ostringstream ss;

    ss << "EVENT_WRITE_EOP"
       << ": event_type=" << event_type
       << " (" << EventName(event_type) << ")"
       << ", event_index=" << event_index
       << ", address=" << Hex64(address)
       << ", data_sel=" << data_sel
       << ", int_sel=" << int_sel
       << ", sequence=" << Hex64(sequence);

    packet.description = ss.str();
}

// ============================================================================
// RELEASE_MEM
// ============================================================================

void Decoder::DecodeReleaseMem(Packet& packet)
{
    if (packet.payload.size() < 6) {
        packet.description =
            "RELEASE_MEM: truncated";
        return;
    }

    const uint32_t event_control =
        packet.payload[0];

    const uint32_t data_control =
        packet.payload[1];

    const uint32_t event_type =
        (event_control >> EVENT_TYPE_SHIFT) &
        EVENT_TYPE_MASK;

    const uint32_t event_index =
        (event_control >> EVENT_INDEX_SHIFT) &
        EVENT_INDEX_MASK;

    const uint32_t data_sel =
        (data_control >> DATA_SEL_SHIFT) &
        DATA_SEL_MASK;

    const uint32_t int_sel =
        (data_control >> INT_SEL_SHIFT) &
        INT_SEL_MASK;

    const uint64_t address =
        MakeAddress(packet.payload[2],
                    packet.payload[3],
                    16);

    const uint64_t sequence =
        (static_cast<uint64_t>(packet.payload[5]) << 32) |
        packet.payload[4];

    std::ostringstream ss;

    ss << "RELEASE_MEM"
       << ": event_type=" << event_type
       << " (" << EventName(event_type) << ")"
       << ", event_index=" << event_index
       << ", data_sel=" << data_sel
       << ", int_sel=" << int_sel
       << ", address=" << Hex64(address)
       << ", sequence=" << Hex64(sequence);

    packet.description = ss.str();
}

// ============================================================================
// DISPATCH_DIRECT
// ============================================================================

void Decoder::DecodeDispatchDirect(Packet& packet)
{
    if (packet.payload.size() < 4) {
        packet.description =
            "DISPATCH_DIRECT: truncated";
        return;
    }

    std::ostringstream ss;

    ss << "DISPATCH_DIRECT"
       << ": x=" << packet.payload[0]
       << ", y=" << packet.payload[1]
       << ", z=" << packet.payload[2]
       << ", initiator=" << Hex(packet.payload[3]);

    packet.description = ss.str();
}

// ============================================================================
// COND_EXEC
// ============================================================================

void Decoder::DecodeCondExec(Packet& packet)
{
    if (packet.payload.size() < 4) {
        packet.description =
            "COND_EXEC: truncated";
        return;
    }

    const uint64_t address =
        MakeAddress(packet.payload[0],
                    packet.payload[1],
                    32);

    std::ostringstream ss;

    ss << "COND_EXEC"
       << ": address=" << Hex64(address)
       << ", control=" << Hex(packet.payload[2])
       << ", dummy=" << Hex(packet.payload[3]);

    packet.description = ss.str();
}

// ============================================================================
// INDIRECT_BUFFER
// ============================================================================

void Decoder::DecodeIndirectBuffer(Packet& packet)
{
    if (packet.payload.size() < 3) {
        packet.description =
            "INDIRECT_BUFFER: truncated";
        return;
    }

    const uint64_t address =
        MakeAddress(packet.payload[0],
                    packet.payload[1],
                    16);

    const uint32_t control =
        packet.payload[2];

    const uint32_t length =
        control & 0xFFFFF;

    const uint32_t vmid =
        (control >> 24) & 0xFF;

    std::ostringstream ss;

    ss << packet.name
       << ": address=" << Hex64(address)
       << ", length_dw=" << length
       << ", vmid=" << vmid
       << ", control=" << Hex(control);

    packet.description = ss.str();
}

// ============================================================================
// PREAMBLE_CNTL
// ============================================================================

void Decoder::DecodePreambleCntl(Packet& packet)
{
    if (packet.payload.empty()) {
        packet.description =
            "PREAMBLE_CNTL: missing payload";
        return;
    }

    const uint32_t value =
        packet.payload[0];

    const uint32_t mode =
        (value >> PREAMBLE_MODE_SHIFT) &
        PREAMBLE_MODE_MASK;

    std::ostringstream ss;

    ss << "PREAMBLE_CNTL"
       << ": mode=" << mode
       << ", raw=" << Hex(value);

    packet.description = ss.str();
}

// ============================================================================
// SET_BASE
// ============================================================================

void Decoder::DecodeSetBase(Packet& packet)
{
    if (packet.payload.size() < 3) {
        packet.description =
            "SET_BASE: truncated";
        return;
    }

    const uint32_t base_index =
        packet.payload[0];

    const uint64_t base =
        (static_cast<uint64_t>(packet.payload[2]) << 32) |
        packet.payload[1];

    std::string partition;

    if (base_index == GDS_PARTITION_BASE)
        partition = "GDS";
    else if (base_index == CE_PARTITION_BASE)
        partition = "CE";
    else
        partition = "UNKNOWN";

    std::ostringstream ss;

    ss << "SET_BASE"
       << ": base_index=" << base_index
       << " (" << partition << ")"
       << ", base=" << Hex64(base);

    packet.description = ss.str();
}

// ============================================================================
// ME_INITIALIZE
// ============================================================================

void Decoder::DecodeMeInitialize(Packet& packet)
{
    if (packet.payload.empty()) {
        packet.description =
            "ME_INITIALIZE: missing payload";
        return;
    }

    const uint32_t value =
        packet.payload[0];

    const uint32_t device_id =
        (value >> 16) & 0xFFFF;

    std::ostringstream ss;

    ss << "ME_INITIALIZE"
       << ": device_id=" << device_id
       << ", raw=" << Hex(value);

    packet.description = ss.str();
}

// ============================================================================
// CONTEXT_CONTROL
// ============================================================================

void Decoder::DecodeContextControl(Packet& packet)
{
    if (packet.payload.size() < 2) {
        packet.description =
            "CONTEXT_CONTROL: truncated";
        return;
    }

    std::ostringstream ss;

    ss << "CONTEXT_CONTROL"
       << ": control=" << Hex(packet.payload[0])
       << ", reserved=" << Hex(packet.payload[1]);

    packet.description = ss.str();
}

// ============================================================================
// SET_*_REG
// ============================================================================

void Decoder::DecodeSetReg(Packet& packet)
{
    uint32_t base = 0;

    switch (packet.header_info.opcode) {
    case OP_SET_CONFIG_REG:
        base = SET_CONFIG_REG_START;
        break;

    case OP_SET_CONTEXT_REG:
        base = SET_CONTEXT_REG_START;
        break;

    case OP_SET_SH_REG:
        base = SET_SH_REG_START;
        break;

    default:
        DumpRawPayload(packet, packet.description);
        return;
    }

    if (packet.payload.empty()) {
        packet.description =
            "SET_REG: missing offset";
        return;
    }

    const uint32_t offset =
        packet.payload[0];

    const uint32_t first_reg =
        base + offset;

    std::ostringstream ss;

    ss << packet.name
       << ": offset=" << Hex(offset)
       << ", first_reg=" << Hex(first_reg);

    const std::string first_name =
        RegisterName(first_reg);

    if (!first_name.empty())
        ss << " (" << first_name << ")";

    for (std::size_t i = 1;
         i < packet.payload.size();
         ++i) {

        const uint32_t reg =
            first_reg +
            static_cast<uint32_t>((i - 1) * 4);

        ss << "\n  "
           << Hex(reg);

        const std::string name =
            RegisterName(reg);

        if (!name.empty())
            ss << " (" << name << ")";

        ss << " = "
           << Hex(packet.payload[i]);
    }

    packet.description = ss.str();
}

// ============================================================================
// TYPE 3 opcode dispatch
// ============================================================================

void Decoder::DecodeType3Opcode(Packet& packet)
{
    switch (packet.header_info.opcode) {

    case OP_WRITE_DATA:
        DecodeWriteData(packet);
        break;

    case OP_COPY_DATA:
        DecodeCopyData(packet);
        break;

    case OP_WAIT_REG_MEM:
        DecodeWaitRegMem(packet);
        break;

    case OP_CP_DMA:
        DecodeCpDma(packet);
        break;

    case OP_SURFACE_SYNC:
        DecodeSurfaceSync(packet);
        break;

    case OP_ACQUIRE_MEM:
        DecodeAcquireMem(packet);
        break;

    case OP_EVENT_WRITE:
        DecodeEventWrite(packet);
        break;

    case OP_EVENT_WRITE_EOP:
        DecodeEventWriteEop(packet);
        break;

    case OP_RELEASE_MEM:
        DecodeReleaseMem(packet);
        break;

    case OP_DISPATCH_DIRECT:
        DecodeDispatchDirect(packet);
        break;

    case OP_COND_EXEC:
        DecodeCondExec(packet);
        break;

    case OP_INDIRECT_BUFFER:
    case OP_INDIRECT_BUFFER_CONST:
        DecodeIndirectBuffer(packet);
        break;

    case OP_PREAMBLE_CNTL:
        DecodePreambleCntl(packet);
        break;

    case OP_SET_BASE:
        DecodeSetBase(packet);
        break;

    case OP_ME_INITIALIZE:
        DecodeMeInitialize(packet);
        break;

    case OP_CONTEXT_CONTROL:
        DecodeContextControl(packet);
        break;

    case OP_SET_CONFIG_REG:
    case OP_SET_CONTEXT_REG:
    case OP_SET_SH_REG:
        DecodeSetReg(packet);
        break;

    case OP_NOP:
        packet.description =
            "NOP";
        break;

    case OP_CLEAR_STATE:
        packet.description =
            "CLEAR_STATE";
        break;

    case OP_PFP_SYNC_ME:
        packet.description =
            "PFP_SYNC_ME";
        break;

    case OP_SWITCH_BUFFER:
        packet.description =
            "SWITCH_BUFFER";
        break;

    default:
        DumpRawPayload(packet, packet.description);
        packet.description +=
            " (layout not field-decoded; payload preserved exactly)";
        break;
    }
}

// ============================================================================
// TYPE 3
// ============================================================================

void Decoder::DecodeType3(Packet& packet)
{
    packet.name =
        OpcodeName(packet.header_info.opcode);

    std::ostringstream prefix;

    if (packet.header_info.compute)
        prefix << "compute=1; ";

    DecodeType3Opcode(packet);

    if (!prefix.str().empty())
        packet.description =
            prefix.str() + packet.description;
}

// ============================================================================
// Main decode
// ============================================================================

std::size_t Decoder::Decode(const uint32_t* stream,
                            std::size_t dword_count,
                            std::size_t offset,
                            Packet& out) const
{
    out = Packet{};
    out.offset = offset;

    if (stream == nullptr) {
        out.valid = false;
        out.error = "null PM4 stream";
        return 0;
    }

    if (offset >= dword_count) {
        out.valid = false;
        out.error = "offset outside PM4 stream";
        return 0;
    }

    const uint32_t header =
        stream[offset];

    out.header = header;
    out.header_info =
        DecodeHeader(header);

    const PacketType type =
        out.header_info.type;

    std::size_t payload_count = 0;

    switch (type) {
    case PacketType::Type0:
        payload_count = Type0Count(header);
        break;

    case PacketType::Type1:
        payload_count = Type1Count(header);
        break;

    case PacketType::Type2:
        payload_count = Type2Count(header);
        break;

    case PacketType::Type3:
        payload_count = Type3Count(header);
        break;

    default:
        out.valid = false;
        out.error = "unknown PM4 packet type";
        return 1;
    }

    const std::size_t total_dwords =
        1 + payload_count;

    if (offset + total_dwords > dword_count) {
        out.valid = false;

        std::ostringstream ss;

        ss << "truncated PM4 packet: need "
           << total_dwords
           << " DWORDs, have "
           << (dword_count - offset);

        out.error = ss.str();

        return dword_count - offset;
    }

    out.payload.reserve(payload_count);

    for (std::size_t i = 0;
         i < payload_count;
         ++i) {
        out.payload.push_back(
            stream[offset + 1 + i]
        );
    }

    out.dword_count =
        total_dwords;

    switch (type) {
    case PacketType::Type0:
        DecodeType0(out);
        break;

    case PacketType::Type1:
        DecodeType1(out);
        break;

    case PacketType::Type2:
        DecodeType2(out);
        break;

    case PacketType::Type3:
        DecodeType3(out);
        break;

    default:
        break;
    }

    return total_dwords;
}

// ============================================================================
// Decode entire PM4 stream
// ============================================================================

std::vector<Packet> Decoder::DecodeStream(
    const uint32_t* stream,
    std::size_t dword_count) const
{
    std::vector<Packet> packets;

    if (stream == nullptr || dword_count == 0)
        return packets;

    std::size_t offset = 0;

    while (offset < dword_count) {
        Packet packet;

        const std::size_t consumed =
            Decode(stream,
                   dword_count,
                   offset,
                   packet);

        if (consumed == 0)
            break;

        packets.push_back(packet);

        offset += consumed;

        if (!packet.valid)
            break;
    }

    return packets;
}

} // namespace PM4  