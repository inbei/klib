#pragma once

#ifndef _MUTEX_HPP_
#define _MUTEX_HPP_

#include "KException.h"
#include "KError.h"
#include <pthread.h>

namespace klib {
    class KCondVariable;
    class KMutex
    {
    public:
        KMutex();

        ~KMutex();

        void Lock() const;

        void Unlock() const;

        bool TryLock() const;

    private:
        mutable pthread_mutex_t m_pmtx;
        friend class KCondVariable;
    };
};
#endif // !_MUTEX_HPP_
