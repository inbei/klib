#include "thread/KMutex.h"
namespace klib {
    KMutex::KMutex()
    {
        pthread_mutexattr_t attr;
        int rc = pthread_mutexattr_init(&attr);
        if (rc != 0)
        {
            pthread_mutexattr_destroy(&attr);
            throw KException(__FILE__, __LINE__, KError::StdErrorStr(rc).c_str());
        }

        rc = pthread_mutex_init(&m_pmtx, &attr);
        if (rc != 0)
        {
            pthread_mutexattr_destroy(&attr);
            throw KException(__FILE__, __LINE__, KError::StdErrorStr(rc).c_str());
        }

        rc = pthread_mutexattr_destroy(&attr);
        if (rc != 0)
        {
            throw KException(__FILE__, __LINE__, KError::StdErrorStr(rc).c_str());
        }
    }

    KMutex::~KMutex()
    {
        int rc = pthread_mutex_destroy(&m_pmtx);
        if (rc != 0)
        {
            throw KException(__FILE__, __LINE__, KError::StdErrorStr(rc).c_str());
        }
    }

    void KMutex::Lock() const
    {
        int rc = pthread_mutex_lock(&m_pmtx);
        if (rc != 0)
        {
            throw KException(__FILE__, __LINE__, KError::StdErrorStr(rc).c_str());
        }
    }

    void KMutex::Unlock() const
    {
        int rc = pthread_mutex_unlock(&m_pmtx);
        if (rc != 0)
        {
            throw KException(__FILE__, __LINE__, KError::StdErrorStr(rc).c_str());
        }
    }

    bool KMutex::TryLock() const
    {
        int rc = pthread_mutex_trylock(&m_pmtx);
        if (rc != 0 && rc != EBUSY)
        {
            throw KException(__FILE__, __LINE__, KError::StdErrorStr(rc).c_str());
        }
        return (rc == 0);
    }
};