/*
 *  sha1.h
 *
 *  Copyright (C) 1998, 2009
 *  Paul E. Jones <paulej@packetizer.com>
 *  All Rights Reserved.
 *
 *****************************************************************************
 *  $Id: sha1.h 12 2009-06-22 19:34:25Z paulej $
 *****************************************************************************
 *
 *  Description:
 *      This class implements the Secure Hashing Standard as defined
 *      in FIPS PUB 180-1 published April 17, 1995.
 *
 *      Many of the variable names in this class, especially the single
 *      character names, were used because those were the names used
 *      in the publication.
 *
 *      Please read the file sha1.cpp for more information.
 *
 */

#ifndef _SHA1_H_
#define _SHA1_H_

namespace klib {
    class KSHA1
    {
    public:

        KSHA1();
        virtual ~KSHA1();

        /*
         *  Re-initialize the class
         */
        void Reset();

        /*
         *  Returns the message digest
         */
        bool Result(unsigned* message_digest_array);

        /*
         *  Provide input to SHA1
         */
        void Input(const unsigned char* message_array,
            unsigned            length);
        void Input(const char* message_array,
            unsigned    length)
        {
            Input((unsigned char*)message_array, length);
        }
        void Input(unsigned char message_element)
        {
            Input(&message_element, 1);
        }
        void Input(char message_element)
        {
            Input((unsigned char*)&message_element, 1);
        }
        KSHA1& operator<<(const char* message_array)
        {
            const char* p = message_array;

            while (*p)
            {
                Input(*p);
                p++;
            }

            return *this;
        }
        KSHA1& operator<<(const unsigned char* message_array)
        {
            const unsigned char* p = message_array;

            while (*p)
            {
                Input(*p);
                p++;
            }

            return *this;
        }
        KSHA1& operator<<(const char message_element)
        {
            Input((unsigned char*)&message_element, 1);

            return *this;
        }
        KSHA1& operator<<(const unsigned char message_element)
        {
            Input(&message_element, 1);

            return *this;
        }

    private:

        /*
         *  Process the next 512 bits of the message
         */
        void ProcessMessageBlock();

        /*
         *  Pads the current message block to 512 bits
         */
        void PadMessage();

        /*
         *  Performs a circular left shift operation
         */
        inline unsigned CircularShift(int bits, unsigned word)
        {
            return ((word << bits) & 0xFFFFFFFF) | ((word & 0xFFFFFFFF) >> (32 - bits));
        }

        unsigned m_cache[5];                      // Message digest buffers

        unsigned m_lengthLow;                // Message length in bits
        unsigned m_lengthHigh;               // Message length in bits

        unsigned char m_messageBlock[64];    // 512-bit message blocks
        int m_messageBlockIndex;            // Index into message block array

        bool m_computed;                      // Is the digest computed?
        bool m_corrupted;                     // Is the message digest corruped?

    };
};
    
#endif // _SHA1_H_
