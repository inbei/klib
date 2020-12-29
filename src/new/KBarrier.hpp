#pragma once
#ifndef _BARRIER_HPP_
#define _BARRIER_HPP_

#if !defined(HPUX)
#include <pthread.h>
#include "thread/KException.h"
#include "thread/KError.h"
namespace klib
{
    class KBarrier 
    {
    public:
        KBarrier(int count) 
        {
            int rc = pthread_barrier_init(&m_barrier, NULL, count);
            if (rc != 0)
            {
                throw KException(__FILE__, __LINE__, KError::StdErrorStr(rc).c_str());
            }
        }

        ~KBarrier() 
        {
            int rc = pthread_barrier_destroy(&m_barrier);
            if (rc != 0)
            {
                throw KException(__FILE__, __LINE__, KError::StdErrorStr(rc).c_str());
            }
        }

        void Wait() const 
        {
            int rc = pthread_barrier_wait(&m_barrier);
            if (rc != 0)
            {
                throw KException(__FILE__, __LINE__, KError::StdErrorStr(rc).c_str());
            }
        }

    private:
        mutable pthread_barrier_t m_barrier;
    };
};
#endif
#endif // _BARRIER_HPP_
