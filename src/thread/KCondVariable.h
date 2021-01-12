#ifndef _CONDVARIABLE_HPP_
#define _CONDVARIABLE_HPP_

#include "thread/KException.h"
#include "util/KTime.h"
#include "thread/KError.h"
#include <pthread.h>
/**
条件变量类
**/
namespace klib {
    class KCondVariable
    {
    public:
        KCondVariable();

        ~KCondVariable();

        /************************************
        * Method:    通知
        * Returns:   
        *************************************/
        void Notify() const;

        /************************************
        * Method:    通知所有
        * Returns:   
        *************************************/
        void NotifyAll() const;

        /************************************
        * Method:    等待
        * Returns:   
        * Parameter: lock
        *************************************/
        template <typename Lock>
        void Wait(const Lock& lock) const;

        /************************************
        * Method:    等待ms毫秒
        * Returns:   
        * Parameter: lock
        * Parameter: ms
        *************************************/
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
        //pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
        pthread_testcancel();
		int rc = pthread_cond_timedwait(&m_pcond, &lock.m_tmtx.m_pmtx, &ts);
        pthread_testcancel();
        //pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
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
        //pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
        pthread_testcancel();
        int rc = pthread_cond_wait(&m_pcond, &lock.m_tmtx.m_pmtx);
        pthread_testcancel();
        //pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
        if (rc != 0)
            throw KException(__FILE__, __LINE__, KError::StdErrorStr(rc).c_str());
    };
};
#endif // !_CONDVARIABLE_HPP_

