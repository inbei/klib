#include "KEndian.h"
namespace klib {
    bool KEndian::IsLittleEndian()
    {
        static char v = 0;
        if (v == 0)
        {
            union {
                int i; /* at least 16 bit */
                char c;
            }un;
            un.i = 0x01; /* 0x01 is LSB */
            v = ((un.c == 0x01) ? 1 : 2);
        }
        return (v & 0x01);
    }

#if !defined(AIX)
    uint64_t KEndian::ntohll(uint64_t val)
    {
        if (IsLittleEndian())
        {
            uint8_t buffer[sizeof(val)] = { 0 };
            ToLittleEndian(val, &buffer[0]);
            return uint64_t(*reinterpret_cast<uint64_t*>(&buffer[0]));
        }
        else
        {
            return val;
        }
    }

    uint64_t KEndian::htonll(uint64_t val)
    {
        if (IsLittleEndian())
        {
            uint8_t buffer[sizeof(val)] = { 0 };
            ToBigEndian(val, &buffer[0]);
            return uint64_t(*reinterpret_cast<uint64_t*>(&buffer[0]));
        }
        else
        {
            return val;
        }
    }
#endif
};

