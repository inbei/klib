#pragma once

#ifndef _CONDVARIABLE_HPP_
#define _CONDVARIABLE_HPP_

#include "KException.h"
#include "util/KTime.h"
#include "KError.h"
#include <pthread.h>

namespace klib {
    class KCondVariable
    {
    public:
        KCondVariable();

        ~KCondVariable();

        /************************************
        * Description: 通知单个等待的条件变量
        * Method:    Notify
        * FullName:  KCondVariable::Notify
        * Access:    public
        * Returns:   void
        * Qualifier: const
        ************************************/
        void Notify() const;

        /************************************
        * Description: 通知所有等待的条件变量
        * Method:    NotifyAll
        * FullName:  KCondVariable::NotifyAll
        * Access:    public
        * Returns:   void
        * Qualifier: const
        ************************************/
        void NotifyAll() const;

        /************************************
        * Description: 一直等待通知信号
        * Method:    Wait
        * FullName:  KCondVariable::Wait
        * Access:    public
        * Returns:   void
        * Qualifier: const
        * Parameter: const Lock & lock
        ************************************/
        template <typename Lock>
        void Wait(const Lock& lock) const;

        /************************************
        * Description: 在指定时长内等待通知信号
        * Method:    TimedWait
        * FullName:  KCondVariable::TimedWait
        * Access:    public
        * Returns:   bool
        * Qualifier: const
        * Parameter: const Lock & lock
        * Parameter: const size_t & ms
        ************************************/
        template <typename Lock>
        bool TimedWait(const Lock& lock, const size_t& ms) const;

    private:
        mutable pthread_cond_t m_pcond;
    };

    template <typename Lock>
    bool KCondVariable::TimedWait(const Lock& lock, const size_t& ms) const
    {
		if (!lock.Acquired())
            throw KException(__FILE__, __LINE__, "not acquired");

		timespec ts;
		KTime::GetTime(ts, ms);
		int rc = pthread_cond_timedwait(&m_pcond, &lock.m_tmtx.m_pmtx, &ts);
#if defined(WIN32)
		if (rc != 0 && rc != WSAETIMEDOUT && rc != 138)// 138 means timeout
            throw KException(__FILE__, __LINE__, KError::WinErrorStr(rc).c_str());
#else
		if (rc != 0 && rc != ETIMEDOUT)
            throw KException(__FILE__, __LINE__, KError::StdErrorStr(rc).c_str());
#endif
		return rc == 0;
    };

    template <typename Lock>
    void KCondVariable::Wait(const Lock& lock) const
    {
        if (!lock.Acquired())
            throw KException(__FILE__, __LINE__, "not acquired");
        int rc = pthread_cond_wait(&m_pcond, &lock.m_tmtx.m_pmtx);
        if (rc != 0)
            throw KException(__FILE__, __LINE__, KError::StdErrorStr(rc).c_str());
    };
};
#endif // !_CONDVARIABLE_HPP_

