#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace PM4 {

enum class PacketType : uint8_t {
    Type0,
    Type1,
    Type2,
    Type3,
    Unknown
};

struct PacketHeader {
    uint32_t raw{};
    PacketType type{PacketType::Unknown};

    // Number of payload DWORDs.
    uint32_t count{};

    uint32_t opcode{};
    uint32_t register_offset{};
    bool compute{};
};

struct Packet {
    std::size_t offset{};
    uint32_t header{};

    PacketHeader header_info{};

    std::vector<uint32_t> payload;

    std::size_t dword_count{};

    std::string name;
    std::string description;

    bool valid{true};
    std::string error;
};

class Decoder {
public:
    Decoder() = default;

    std::size_t Decode(const uint32_t* stream,
                       std::size_t dword_count,
                       std::size_t offset,
                       Packet& out) const;

    std::vector<Packet> DecodeStream(const uint32_t* stream,
                                     std::size_t dword_count) const;

private:
    static PacketHeader DecodeHeader(uint32_t header);
    static PacketType GetPacketType(uint32_t header);

    static std::size_t Type0Count(uint32_t header);
    static std::size_t Type1Count(uint32_t header);
    static std::size_t Type2Count(uint32_t header);
    static std::size_t Type3Count(uint32_t header);

    static std::string PacketTypeName(PacketType type);
    static std::string OpcodeName(uint32_t opcode);
    static std::string RegisterName(uint32_t reg);

    static std::string Hex(uint32_t value);
    static std::string Hex64(uint64_t value);

    static uint32_t ReadPayload(const Packet& packet,
                                std::size_t index);

    static uint64_t MakeAddress(uint32_t low,
                                uint32_t high,
                                unsigned high_bits = 16);

    static std::string EventName(uint32_t event_type);

    static void DumpRawPayload(const Packet& packet,
                               std::string& out);

    static void DecodeType0(Packet& packet);
    static void DecodeType1(Packet& packet);
    static void DecodeType2(Packet& packet);
    static void DecodeType3(Packet& packet);

    static void DecodeType3Opcode(Packet& packet);

    static void DecodeWriteData(Packet& packet);
    static void DecodeCopyData(Packet& packet);
    static void DecodeWaitRegMem(Packet& packet);
    static void DecodeCpDma(Packet& packet);

    static void DecodeSurfaceSync(Packet& packet);
    static void DecodeAcquireMem(Packet& packet);

    static void DecodeEventWrite(Packet& packet);
    static void DecodeEventWriteEop(Packet& packet);
    static void DecodeReleaseMem(Packet& packet);

    static void DecodeDispatchDirect(Packet& packet);
    static void DecodeCondExec(Packet& packet);
    static void DecodeIndirectBuffer(Packet& packet);

    static void DecodePreambleCntl(Packet& packet);
    static void DecodeSetBase(Packet& packet);
    static void DecodeMeInitialize(Packet& packet);
    static void DecodeContextControl(Packet& packet);

    static void DecodeSetReg(Packet& packet);
};

} // namespace PM4