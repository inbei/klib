#pragma once
#ifndef _RWLOCK_HPP_
#define _RWLOCK_HPP_

#if !defined(HPUX)
#include <pthread.h>
#include "KException.h"
#include "KTime.h"
#include "KError.h"
namespace klib
{
    class KReadWriteLock
    {
    public:
        KReadWriteLock() 
        {
            pthread_rwlockattr_t attr;
            int rc = pthread_rwlockattr_init(&attr);
            if (rc != 0)
            {
                pthread_rwlockattr_destroy(&attr);
                throw KException(__FILE__, __LINE__, KError::StdErrorStr(rc).c_str());
            }

            rc = pthread_rwlockattr_setpshared(&attr, PTHREAD_PROCESS_SHARED );
            if (rc != 0)
            {
                pthread_rwlockattr_destroy(&attr);
                throw KException(__FILE__, __LINE__, KError::StdErrorStr(rc).c_str());
            }

            rc = pthread_rwlock_init(&m_lock, &attr);
            if (rc != 0)
            {
                pthread_rwlockattr_destroy(&attr);
                throw KException(__FILE__, __LINE__, KError::StdErrorStr(rc).c_str());
            }

            rc = pthread_rwlockattr_destroy(&attr);
            if (rc != 0)
            {
                throw KException(__FILE__, __LINE__, KError::StdErrorStr(rc).c_str());
            }
        }

        ~KReadWriteLock()
        {
            int rc = pthread_rwlock_destroy(&m_lock);
            if (rc != 0)
            {
                throw KException(__FILE__, __LINE__, KError::StdErrorStr(rc).c_str());
            }
        }

        void RdLock() const 
        {
            int rc = pthread_rwlock_rdlock(&m_lock);
            if (rc != 0)
            {
                throw KException(__FILE__, __LINE__, KError::StdErrorStr(rc).c_str());
            }
        }

        bool TimedRdLock(int ms) const 
        {
            timespec ts;
            KTime::GetTime(ts, ms);
            int rc = pthread_rwlock_timedrdlock(&m_lock, &ts);
#if defined(WIN32)
            if (rc != 0 && rc != WSAETIMEDOUT)
            {
                throw KException(__FILE__, __LINE__, KError::WinErrorStr(rc).c_str());
            }
#else
            if (rc != 0 && rc != ETIMEDOUT)
            {
                throw KException(__FILE__, __LINE__, KError::StdErrorStr(rc).c_str());
            }
#endif
            return rc == 0;
        }

        bool TryRdLock() const 
        {
            int rc = pthread_rwlock_tryrdlock(&m_lock);
            if (rc != 0 && rc != EBUSY)
            {
                throw KException(__FILE__, __LINE__, KError::StdErrorStr(rc).c_str());
            }
            return (rc == 0);
        }

        void WRLock() const 
        {
            int rc = pthread_rwlock_wrlock(&m_lock);
            if (rc != 0)
            {
                throw KException(__FILE__, __LINE__, KError::StdErrorStr(rc).c_str());
            }
        }

        bool TimedWRLock(int ms) const 
        {
            timespec ts;
            KTime::GetTime(ts, ms);
            int rc = pthread_rwlock_timedwrlock(&m_lock, &ts);
#if defined(WIN32)
            if (rc != 0 && rc != WSAETIMEDOUT)
            {
                throw KException(__FILE__, __LINE__, KError::WinErrorStr(rc).c_str());
            }
#else
            if (rc != 0 && rc != ETIMEDOUT)
            {
                throw KException(__FILE__, __LINE__, KError::StdErrorStr(rc).c_str());
            }
#endif
            return rc == 0;
        }

        bool TryWRLock() const 
        {
            int rc = pthread_rwlock_trywrlock(&m_lock);
            if (rc != 0 && rc != EBUSY)
            {
                throw KException(__FILE__, __LINE__, KError::StdErrorStr(rc).c_str());
            }
            return (rc == 0);
        }

        void Unlock() const 
        {
            int rc = pthread_rwlock_unlock(&m_lock);
            if (rc != 0)
            {
                throw KException(__FILE__, __LINE__, KError::StdErrorStr(rc).c_str());
            }
        }

    private:
        mutable pthread_rwlock_t m_lock;
    };
};
#endif // HPUX
#endif // _RWLOCK_HPP_
