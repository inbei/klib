#pragma once

#ifndef _ENDIAN_HPP_
#define _ENDIAN_HPP_
#include <stdint.h>
#include <string.h>
/**
大小端类
**/
namespace klib {
    class KEndian
    {
    public:
        /************************************
        * Method:    判断本机大小端
        * Returns:   
        *************************************/
        static bool IsLittleEndian();

        /************************************
        * Method:    数字转换成大端
        * Returns:   
        * Parameter: num
        * Parameter: dest
        *************************************/
        template<typename NumType> static void ToBigEndian(const NumType& num, uint8_t* dest);

        /************************************
        * Method:    数字转换成小端
        * Returns:   
        * Parameter: num
        * Parameter: dest
        *************************************/
        template<typename NumType>
        static void ToLittleEndian(const NumType& num, uint8_t* dest);

        /************************************
        * Method:    将网络字节流转换成数字
        * Returns:   
        * Parameter: src
        * Parameter: num
        *************************************/
        template<typename NumType>
        static void FromNetwork(const uint8_t* src, NumType& num);
#if !defined(AIX)
        static uint64_t ntohll(uint64_t val);

        static uint64_t htonll(uint64_t val);
#endif
    };

    template<typename NumType>
    void KEndian::FromNetwork(const uint8_t* src, NumType& num)
    {
        size_t sz = sizeof(num);
        if (IsLittleEndian())
        {
            uint8_t* dst = reinterpret_cast<uint8_t*>(&num) + sz - 1;
            for (size_t i = 0; i < sz; ++i)
            {
                *dst-- = *src++;
            }
        }
        else
        {
            memmove(&num, src, sz);
        }
    };

    template<typename NumType>
    void KEndian::ToLittleEndian(const NumType& num, uint8_t* dest)
    {
        size_t sz = sizeof(num);
        if (IsLittleEndian())
        {
            memmove(dest, reinterpret_cast<uint8_t*>(const_cast<NumType*>(&num)), sz);
        }
        else
        {
            uint8_t* tmp = dest;
            uint8_t* src = reinterpret_cast<uint8_t*>(const_cast<NumType*>(&num)) + sz - 1;
            for (size_t i = 0; i < sz; ++i)
            {
                *tmp++ = *src--;
            }
        }
    };

    template<typename NumType>
    void KEndian::ToBigEndian(const NumType& num, uint8_t* dest)
    {
        size_t sz = sizeof(num);
        if (IsLittleEndian())
        {
            uint8_t* tmp = dest;
            uint8_t* src = reinterpret_cast<uint8_t*>(const_cast<NumType*>(&num)) + sz - 1;
            for (size_t i = 0; i < sz; ++i)
            {
                *tmp++ = *src--;
            }
        }
        else
        {
            memmove(dest, reinterpret_cast<uint8_t*>(const_cast<NumType*>(&num)), sz);
        }
    };
};
#endif // !_ENDIAN_HPP_
