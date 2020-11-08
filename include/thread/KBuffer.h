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

        /************************************
        * Description: 重置缓存数据
        * Method:    Reset
        * FullName:  KBuffer::Reset
        * Access:    public
        * Returns:   void
        * Qualifier:
        ************************************/
        void Reset();

        /************************************
        * Description: 释放内存
        * Method:    Release
        * FullName:  KBuffer::Release
        * Access:    public
        * Returns:   void
        * Qualifier:
        ************************************/
        void Release();

        /************************************
        * Description: 将数据追加到缓存最后面
        * Method:    ApendBuffer
        * FullName:  KBuffer::ApendBuffer
        * Access:    public
        * Returns:   bool
        * Qualifier:
        * Parameter: const char * d
        * Parameter: size_t sz
        ************************************/
        bool ApendBuffer(const char* d, size_t sz);

        /************************************
        * Description: 将数据追加到缓存最前面
        * Method:    PrependBuffer
        * FullName:  KBuffer::PrependBuffer
        * Access:    public
        * Returns:   bool
        * Qualifier:
        * Parameter: const char * d
        * Parameter: size_t sz
        ************************************/
        bool PrependBuffer(const char* d, size_t sz);

        /************************************
        * Description: 缓存最大容量
        * Method:    Capacity
        * FullName:  KBuffer::Capacity
        * Access:    public
        * Returns:   size_t
        * Qualifier: const
        ************************************/
        size_t Capacity() const;

        /************************************
        * Description: 获取缓存数据的指针
        * Method:    GetData
        * FullName:  KBuffer::GetData
        * Access:    public
        * Returns:   char *
        * Qualifier: const
        ************************************/
        char* GetData() const;

        /************************************
        * Description: 获取缓存数据大小
        * Method:    GetSize
        * FullName:  KBuffer::GetSize
        * Access:    public
        * Returns:   size_t
        * Qualifier: const
        ************************************/
        size_t GetSize() const;

        /************************************
        * Description: 设置缓存数据大小
        * Method:    SetSize
        * FullName:  KBuffer::SetSize
        * Access:    public
        * Returns:   void
        * Qualifier:
        * Parameter: size_t sz
        ************************************/
        void SetSize(size_t sz);

    private:
        char* m_dat;
        mutable size_t m_size;
        mutable size_t m_capacity;
    };
};

#endif