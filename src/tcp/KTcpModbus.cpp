#include "tcp/KTcpModbus.h"

namespace klib
{
    template<>
    int ParsePacket(const KBuffer& dat, KModbusMessage& msg, KBuffer& left)
    {
        size_t hsz = msg.GetHeaderSize() + sizeof(msg.dev) + sizeof(msg.func);
        if (dat.GetSize() < hsz)
            return ShortHeader;

        uint8_t* src = (uint8_t*)dat.GetData();
        size_t offset = 0;
        KEndian::FromNetwork(src + offset, msg.seq);
        offset += sizeof(msg.seq);
        KEndian::FromNetwork(src + offset, msg.ver);
        offset += sizeof(msg.ver);
        KEndian::FromNetwork(src + offset, msg.len);
        offset += sizeof(msg.len);

        msg.dev = src[offset++];
        msg.func = src[offset++];

        if (!msg.IsValid())
        {
            printf("invalid modbus packet\n");
            return ProtocolError;
        }

        size_t ssz = dat.GetSize();
        if (ssz < msg.GetHeaderSize() + msg.GetPayloadSize())
            return ShortPayload;

        size_t psz = msg.GetPayloadSize() - sizeof(msg.dev) - sizeof(msg.func);
        msg.payload = KBuffer(psz);
        msg.payload.ApendBuffer((const char*)src + offset, psz);
        offset += psz;

        // left data
        if (offset < ssz)
        {
            KBuffer tmp(ssz - offset);
            tmp.ApendBuffer((const char*)src + offset, tmp.Capacity());
            left = tmp;
        }
        return ParseSuccess;
    }
};
