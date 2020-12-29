#ifndef _BUFFER_HPP_
#define _BUFFER_HPP_

#include <cstdio>
#include <cstdlib>
#include <cstring>
namespace klib {
    class KBuffer
    {
    public:
        KBuffer();

        KBuffer(size_t sz);

        ~KBuffer();

        // 重置缓存数据
        void Reset();

        // 释放内存
        void Release();

        // 将数据追加到缓存最后面
        bool ApendBuffer(const char* d, size_t sz);

        // 将数据追加到缓存最前面
        bool PrependBuffer(const char* d, size_t sz);

        // 缓存最大容量
        size_t Capacity() const;

        // 获取缓存数据的指针
        char* GetData() const;

        // 获取缓存数据大小
        size_t GetSize() const;

        // 设置缓存数据大小
        void SetSize(size_t sz);

    private:
        char* m_dat;
        mutable size_t m_size;
        mutable size_t m_capacity;
    };
};

#endif