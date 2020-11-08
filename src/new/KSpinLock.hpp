#pragma once
#ifndef _SPINLOCK_HPP_
#define _SPINLOCK_HPP_

#if !defined(HPUX)
#include <pthread.h>
#include "KException.h"
#include "KError.h"
namespace klib
{
    class KSpinLock 
    {
    public:
        KSpinLock() 
        {
            int rc = pthread_spin_init(&m_lock, PTHREAD_PROCESS_PRIVATE);
            if (rc != 0)
            {
                throw KException(__FILE__, __LINE__, KError::StdErrorStr(rc).c_str());
            }
        }

        ~KSpinLock() 
        {
            int rc = pthread_spin_destroy(&m_lock);
            if (rc != 0)
            {
                throw KException(__FILE__, __LINE__, KError::StdErrorStr(rc).c_str());
            }
        }

        void Lock() const 
        {
            int rc = pthread_spin_lock(&m_lock);
            if (rc != 0)
            {
                throw KException(__FILE__, __LINE__, KError::StdErrorStr(rc).c_str());
            }
        }

        void Unlock() const 
        {
            int rc = pthread_spin_unlock(&m_lock);
            if (rc != 0)
            {
                throw KException(__FILE__, __LINE__, KError::StdErrorStr(rc).c_str());
            }
        }

        bool TryLock() const 
        {
            int rc = pthread_spin_trylock(&m_lock);
            if (rc != 0 && rc != EBUSY)
            {
                throw KException(__FILE__, __LINE__, KError::StdErrorStr(rc).c_str());
            }
            return (rc == 0);
        }

    private:
        mutable pthread_spinlock_t m_lock;
    };
};
#endif // HPUX
#endif // _SPINLOCK_HPP_
