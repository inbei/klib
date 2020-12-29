#ifndef _SINGLETON_HPP_
#define _SINGLETON_HPP_
#include "thread/KMutex.h"
#include "thread/KLockGuard.h"

namespace klib
{
    template<typename T>
    class KSingleton
    {
    public:
        static T& GetRef()
        {
            static klib::KMutex mtx;
            klib::KLockGuard<klib::KMutex> lock(mtx);
            static T t;
            return t;
        }
    private:
        KSingleton() {};
        KSingleton(const KSingleton& other) {};
        KSingleton& operator=(const KSingleton& other) {};
    };
};
#endif