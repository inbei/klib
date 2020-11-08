#pragma once

#ifndef _LOCKGUARD_HPP_
#define _LOCKGUARD_HPP_

#include "KException.h"
#include <pthread.h>
namespace klib {
    class KCondVariable;
    template<typename MutexType>
    class KLockGuard
    {
    public:
        KLockGuard(const MutexType& mtx)
            :m_tmtx(mtx)
        {
			m_tmtx.Lock();
			m_acquired = true;
        }

        ~KLockGuard()
        {
			if (m_acquired)
                m_tmtx.Unlock();
        }

        inline void Acquire() const
        {
			if (m_acquired)
                throw KException(__FILE__, __LINE__, "already acquired");
            m_tmtx.Lock();
			m_acquired = true;
        }

        inline bool TryAcquire() const
        {
			if (m_acquired)
                throw KException(__FILE__, __LINE__, "already acquired");
			m_acquired = m_tmtx.TryLock();
			return m_acquired;
        }

        inline void Release() const
        {
			if (!m_acquired)
                throw KException(__FILE__, __LINE__, "not acquired");
			m_tmtx.Unlock();
			m_acquired = false;
        }

        inline bool Acquired() const{ return m_acquired; }

    private:
        const MutexType& m_tmtx;
        mutable volatile bool m_acquired;
        friend class KCondVariable;
    };
};
#endif // !_LOCKGUARD_HPP_
