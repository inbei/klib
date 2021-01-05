#include "tcp/KTcpWebsocket.h"

namespace klib
{
    template<>
    int ParsePacket(const KBuffer& dat, KWebsocketMessage& msg, KBuffer& left)
    {
        char* src = dat.GetData();
        size_t ssz = dat.GetSize();
        if (ssz < sizeof(uint16_t))
            return ShortHeader;

        KBuffer& payload = msg.payload;
        // 1st byte
        size_t offset = 0;
        uint8_t fbyte = src[offset++];
        msg.fin = fbyte >> 7;
        msg.reserved = (fbyte >> 4) & 0x7;
        msg.opcode = fbyte & 0xf;
        if (!msg.IsValid())
            return ProtocolError;

        // 2nd byte
        uint8_t sbyte = src[offset++];
        msg.mask = sbyte >> 7;
        msg.plen = sbyte & 0x7f;
        // 3rd 4th byte means payload length
        if (msg.plen == 126)// 2 bytes
        {
            size_t sz = sizeof(uint16_t);
            if (ssz < offset + sz)
                return ShortHeader;

            KEndian::FromNetwork(reinterpret_cast<const uint8_t*>(src + offset), msg.extplen.extplen2);
            offset += sz;
            if (msg.extplen.extplen2 < 126)
                return ProtocolError;
        }
        // 3rd - 10th byte means payload length
        else if (msg.plen == 127) //8 bytes
        {
            size_t sz = sizeof(uint64_t);
            if (ssz < offset + sz)
                return ShortHeader;
            KEndian::FromNetwork(reinterpret_cast<const uint8_t*>(src + offset), msg.extplen.extplen8);
            offset += sz;
            if (msg.extplen.extplen8 <= 65535)
                return ProtocolError;
        }

        // mask key
        if (msg.mask == 1)
        {
            size_t sz = sizeof(msg.maskkey);
            if (ssz < offset + sz)
                return ShortHeader;

            memcpy(msg.maskkey, src + offset, sz);
            offset += sz;
        }

        if (msg.plen > 0)
        {
            //copy payload
            size_t psz = msg.GetPayloadSize();
            if (ssz < offset + psz)
                return ShortPayload;

            char* tsrc = src + offset;
            bool masked = (msg.mask & 0x1);
            payload = KBuffer(psz);
            char* dst = payload.GetData();
            for (uint32_t i = 0; i < psz; ++i)
            {
                dst[i] = (masked ? (tsrc[i] ^ msg.maskkey[i % 4]) : tsrc[i]);
            }
            offset += psz;
            payload.SetSize(psz);
        }

        // left data
        if (offset < ssz)
        {
            KBuffer tmp(ssz - offset);
            tmp.ApendBuffer(src + offset, tmp.Capacity());
            left = tmp;
        }
        return ParseSuccess;
    };
};
