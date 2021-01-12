#ifndef _MUTEX_HPP_
#define _MUTEX_HPP_

#include <pthread.h>
/**
互斥量
**/
namespace klib {
    class KCondVariable;
    class KMutex
    {
    public:
        KMutex();

        ~KMutex();

        /************************************
        * Method:    上锁
        * Returns:   
        *************************************/
        void Lock() const;

        /************************************
        * Method:    解锁
        * Returns:   
        *************************************/
        void Unlock() const;

        /************************************
        * Method:    尝试加锁
        * Returns:   
        *************************************/
        bool TryLock() const;

    private:
        mutable pthread_mutex_t m_pmtx;
        friend class KCondVariable;
    };
};
#endif // !_MUTEX_HPP_
