#include "util/KBase64.h"
namespace klib {
    const std::string WordChars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    const std::string Base64Chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string KBase64::Encode(const char* str, unsigned int length)
    {
        std::string ret;
        int i = 0;
        int j = 0;
        unsigned char buffer3[3];
        unsigned char buffer4[4];

        while (length--) {
            buffer3[i++] = *(str++);
            if (i == 3) {
                buffer4[0] = (buffer3[0] & 0xfc) >> 2;
                buffer4[1] = ((buffer3[0] & 0x03) << 4) + ((buffer3[1] & 0xf0) >> 4);
                buffer4[2] = ((buffer3[1] & 0x0f) << 2) + ((buffer3[2] & 0xc0) >> 6);
                buffer4[3] = buffer3[2] & 0x3f;

                for (i = 0; (i < 4); i++)
                    ret += Base64Chars[buffer4[i]];
                i = 0;
            }
        }

        if (i)
        {
            for (j = i; j < 3; j++)
                buffer3[j] = '\0';

            buffer4[0] = (buffer3[0] & 0xfc) >> 2;
            buffer4[1] = ((buffer3[0] & 0x03) << 4) + ((buffer3[1] & 0xf0) >> 4);
            buffer4[2] = ((buffer3[1] & 0x0f) << 2) + ((buffer3[2] & 0xc0) >> 6);
            buffer4[3] = buffer3[2] & 0x3f;

            for (j = 0; (j < i + 1); j++)
                ret += Base64Chars[buffer4[j]];

            while ((i++ < 3))
                ret += '=';
        }

        return ret;
    }

    std::string KBase64::Decode(const std::string& str)
    {
        size_t in_len = str.size();
        int i = 0;
        int j = 0;
        int index = 0;
        unsigned char buffer4[4], buffer3[3];
        std::string ret;

        while (in_len-- && (str[index] != '=') && IsBase64Char(str[index])) {
            buffer4[i++] = str[index]; index++;
            if (i == 4) {
                for (i = 0; i < 4; i++)
                    buffer4[i] = (unsigned char)Base64Chars.find(buffer4[i]);

                buffer3[0] = (buffer4[0] << 2) + ((buffer4[1] & 0x30) >> 4);
                buffer3[1] = ((buffer4[1] & 0xf) << 4) + ((buffer4[2] & 0x3c) >> 2);
                buffer3[2] = ((buffer4[2] & 0x3) << 6) + buffer4[3];

                for (i = 0; (i < 3); i++)
                    ret += buffer3[i];
                i = 0;
            }
        }

        if (i) {
            for (j = i; j < 4; j++)
                buffer4[j] = 0;

            for (j = 0; j < 4; j++)
                buffer4[j] = (unsigned char)Base64Chars.find(buffer4[j]);

            buffer3[0] = (buffer4[0] << 2) + ((buffer4[1] & 0x30) >> 4);
            buffer3[1] = ((buffer4[1] & 0xf) << 4) + ((buffer4[2] & 0x3c) >> 2);
            buffer3[2] = ((buffer4[2] & 0x3) << 6) + buffer4[3];

            for (j = 0; (j < i - 1); j++) ret += buffer3[j];
        }

        return ret;
    }

    bool KBase64::IsBase64Char(unsigned char c)
    {
        return (isalnum(c) || (c == '+') || (c == '/'));
    }
};