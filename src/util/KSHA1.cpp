#include "KSHA1.h"
namespace klib {
    KSHA1::KSHA1()
    {
        Reset();
    }

    KSHA1::~KSHA1()
    {

    }

    void KSHA1::Reset()
    {
        m_lengthLow = 0;
        m_lengthHigh = 0;
        m_messageBlockIndex = 0;

        m_cache[0] = 0x67452301;
        m_cache[1] = 0xEFCDAB89;
        m_cache[2] = 0x98BADCFE;
        m_cache[3] = 0x10325476;
        m_cache[4] = 0xC3D2E1F0;

        m_computed = false;
        m_corrupted = false;
    }

    bool KSHA1::Result(unsigned* message_digest_array)
    {
        int i;                                  // Counter

        if (m_corrupted)
        {
            return false;
        }

        if (!m_computed)
        {
            PadMessage();
            m_computed = true;
        }

        for (i = 0; i < 5; i++)
        {
            message_digest_array[i] = m_cache[i];
        }

        return true;
    }

    void KSHA1::Input(const unsigned char* message_array, unsigned length)
    {
        if (!length)
        {
            return;
        }

        if (m_computed || m_corrupted)
        {
            m_corrupted = true;
            return;
        }

        while (length-- && !m_corrupted)
        {
            m_messageBlock[m_messageBlockIndex++] = (*message_array & 0xFF);

            m_lengthLow += 8;
            m_lengthLow &= 0xFFFFFFFF;               // Force it to 32 bits
            if (m_lengthLow == 0)
            {
                m_lengthHigh++;
                m_lengthHigh &= 0xFFFFFFFF;          // Force it to 32 bits
                if (m_lengthHigh == 0)
                {
                    m_corrupted = true;               // Message is too long
                }
            }

            if (m_messageBlockIndex == 64)
            {
                ProcessMessageBlock();
            }

            message_array++;
        }
    }

    void KSHA1::ProcessMessageBlock()
    {
        const unsigned K[] = {               // Constants defined for SHA-1
                            0x5A827999,
                            0x6ED9EBA1,
                            0x8F1BBCDC,
                            0xCA62C1D6
        };
        int         t;                          // Loop counter
        unsigned    temp;                       // Temporary word value
        unsigned    W[80];                      // Word sequence
        unsigned    A, B, C, D, E;              // Word buffers

        /*
         *  Initialize the first 16 words in the array W
         */
        for (t = 0; t < 16; t++)
        {
            W[t] = ((unsigned)m_messageBlock[t * 4]) << 24;
            W[t] |= ((unsigned)m_messageBlock[t * 4 + 1]) << 16;
            W[t] |= ((unsigned)m_messageBlock[t * 4 + 2]) << 8;
            W[t] |= ((unsigned)m_messageBlock[t * 4 + 3]);
        }

        for (t = 16; t < 80; t++)
        {
            W[t] = CircularShift(1, W[t - 3] ^ W[t - 8] ^ W[t - 14] ^ W[t - 16]);
        }

        A = m_cache[0];
        B = m_cache[1];
        C = m_cache[2];
        D = m_cache[3];
        E = m_cache[4];

        for (t = 0; t < 20; t++)
        {
            temp = CircularShift(5, A) + ((B & C) | ((~B) & D)) + E + W[t] + K[0];
            temp &= 0xFFFFFFFF;
            E = D;
            D = C;
            C = CircularShift(30, B);
            B = A;
            A = temp;
        }

        for (t = 20; t < 40; t++)
        {
            temp = CircularShift(5, A) + (B ^ C ^ D) + E + W[t] + K[1];
            temp &= 0xFFFFFFFF;
            E = D;
            D = C;
            C = CircularShift(30, B);
            B = A;
            A = temp;
        }

        for (t = 40; t < 60; t++)
        {
            temp = CircularShift(5, A) +
                ((B & C) | (B & D) | (C & D)) + E + W[t] + K[2];
            temp &= 0xFFFFFFFF;
            E = D;
            D = C;
            C = CircularShift(30, B);
            B = A;
            A = temp;
        }

        for (t = 60; t < 80; t++)
        {
            temp = CircularShift(5, A) + (B ^ C ^ D) + E + W[t] + K[3];
            temp &= 0xFFFFFFFF;
            E = D;
            D = C;
            C = CircularShift(30, B);
            B = A;
            A = temp;
        }

        m_cache[0] = (m_cache[0] + A) & 0xFFFFFFFF;
        m_cache[1] = (m_cache[1] + B) & 0xFFFFFFFF;
        m_cache[2] = (m_cache[2] + C) & 0xFFFFFFFF;
        m_cache[3] = (m_cache[3] + D) & 0xFFFFFFFF;
        m_cache[4] = (m_cache[4] + E) & 0xFFFFFFFF;

        m_messageBlockIndex = 0;
    }

    void KSHA1::PadMessage()
    {
        /*
    *  Check to see if the current message block is too small to hold
    *  the initial padding bits and length.  If so, we will pad the
    *  block, process it, and then continue padding into a second block.
    */
        if (m_messageBlockIndex > 55)
        {
            m_messageBlock[m_messageBlockIndex++] = 0x80;
            while (m_messageBlockIndex < 64)
            {
                m_messageBlock[m_messageBlockIndex++] = 0;
            }

            ProcessMessageBlock();

            while (m_messageBlockIndex < 56)
            {
                m_messageBlock[m_messageBlockIndex++] = 0;
            }
        }
        else
        {
            m_messageBlock[m_messageBlockIndex++] = 0x80;
            while (m_messageBlockIndex < 56)
            {
                m_messageBlock[m_messageBlockIndex++] = 0;
            }

        }

        /*
         *  Store the message length as the last 8 octets
         */
        m_messageBlock[56] = (m_lengthHigh >> 24) & 0xFF;
        m_messageBlock[57] = (m_lengthHigh >> 16) & 0xFF;
        m_messageBlock[58] = (m_lengthHigh >> 8) & 0xFF;
        m_messageBlock[59] = (m_lengthHigh) & 0xFF;
        m_messageBlock[60] = (m_lengthLow >> 24) & 0xFF;
        m_messageBlock[61] = (m_lengthLow >> 16) & 0xFF;
        m_messageBlock[62] = (m_lengthLow >> 8) & 0xFF;
        m_messageBlock[63] = (m_lengthLow) & 0xFF;

        ProcessMessageBlock();
    }
};