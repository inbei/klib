#include "thread/KBuffer.h"
namespace klib {
    KBuffer::KBuffer() : m_size(0), m_dat(NULL), m_capacity(0)
    {

    }

    KBuffer::~KBuffer()
    {

    }

    void KBuffer::Reset()
    {
        if (m_dat)
        {
            memset(m_dat, 0, m_capacity);
            m_size = 0;
        }
    }

    void KBuffer::Release()
    {
        if (m_dat)
        {
            free(m_dat);
            m_dat = NULL;
            m_size = 0;
            m_capacity = 0;
        }
    }

    bool KBuffer::ApendBuffer(const char* d, size_t sz)
    {
        if (m_capacity - m_size < sz)
        {
            size_t tsz = m_size + sz;
            m_dat = (char*)realloc(m_dat, tsz);
            if (m_dat == NULL)
            {
                char* tmp = (char*)malloc(tsz);
                memset(tmp, 0, tsz);
                if (tmp == NULL)
                {
                    return false;
                }
                memmove(tmp, m_dat, m_size);
                free(m_dat);
                memmove(&tmp[m_size], d, sz);
                m_dat = tmp;
            }
            else
            {
                memmove(&m_dat[m_size], d, sz);
            }
            m_size = tsz;
            m_capacity = tsz;
        }
        else
        {
            memmove(&m_dat[m_size], d, sz);
            m_size += sz;
        }
        return true;
    }

    bool KBuffer::PrependBuffer(const char* d, size_t sz)
    {
        if (m_capacity - m_size < sz)
        {
            size_t tsz = m_size + sz;
            m_dat = (char*)realloc(m_dat, tsz);
            if (m_dat == NULL)
            {
                char* tmp = (char*)malloc(tsz);
                memset(tmp, 0, tsz);
                if (tmp == NULL)
                {
                    return false;
                }
                memmove(tmp, d, sz);
                memmove(&tmp[sz], m_dat, m_size);
                free(m_dat);
                m_dat = tmp;
            }
            else
            {
                memmove(&m_dat[sz], m_dat, m_size);
                memmove(m_dat, d, sz);
            }
            m_size = tsz;
            m_capacity = tsz;
        }
        else
        {
            memmove(&m_dat[sz], m_dat, m_size);
            memmove(m_dat, d, sz);
            m_size += sz;
        }
        return true;
    }

    size_t KBuffer::Capacity() const{ return m_capacity; }

    char* KBuffer::GetData() const{ return m_dat; }

    size_t KBuffer::GetSize() const{ return m_size; }

    void KBuffer::SetSize(size_t sz)
    {
        if (sz > m_capacity)
        {
            sz = m_capacity;
        }
        m_size = sz;
    }

    KBuffer::KBuffer(size_t sz) : m_size(0), m_capacity(sz)
    {
        m_dat = (char*)malloc(sz);
        memset(m_dat, 0, m_capacity);
    }
};