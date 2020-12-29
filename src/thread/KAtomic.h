#ifndef _ATOMIC_HPP_
#define _ATOMIC_HPP_

#if defined(WIN32)
#include <windows.h>
#endif
#include <map>

#include "thread/KMutex.h"
#include "thread/KLockGuard.h"

namespace klib {
    template<typename VariantType>
    class AtomicVariant
    {
    public:
        AtomicVariant(const VariantType& d)
            :m_dat(d) {}

        inline void Assign(const VariantType& d)
        {
			KLockGuard<KMutex> lock(m_vtMtx);
			m_dat = d;
        }

        inline VariantType Get() const
        {
			KLockGuard<KMutex> lock(m_vtMtx);
			return m_dat;
        }

        inline operator VariantType() const
        {
			KLockGuard<KMutex> lock(m_vtMtx);
			return m_dat;
        }

        inline AtomicVariant& operator=(const VariantType& dt)
        {
            KLockGuard<KMutex> lock(m_vtMtx);
            m_dat = dt;
            return *this;
        }

    private:
        VariantType m_dat;
        KMutex m_vtMtx;
    };

    template<typename KeyType, typename ValueType>
    class AtomicMap
    {
    public:
        inline void Assign(const KeyType& ky, const ValueType& vt)
        {
			KLockGuard<KMutex> lock(m_mpMtx);
			m_dat[ky] = vt;
        }

        inline void Assign(const std::map<KeyType, ValueType>& d)
        {
			KLockGuard<KMutex> lock(m_mpMtx);
			m_dat = d;
        }

        inline bool Get(const KeyType& ky, ValueType& val)
        {
			KLockGuard<KMutex> lock(m_mpMtx);
			if (m_dat.find(ky) != m_dat.end())
			{
				val = m_dat[ky];
				return true;
			}
			return false;
        }

        inline void Get(std::map<KeyType, ValueType>& d) const
        {
            KLockGuard<KMutex> lock(m_mpMtx);
            d = m_dat;
        }

        inline void Erase(const KeyType& ky)
        {
			KLockGuard<KMutex> lock(m_mpMtx);
			m_dat.erase(ky);
        }

        inline void Clear()
        {
			KLockGuard<KMutex> lock(m_mpMtx);
			m_dat.clear();
        }

        inline bool Empty() const
        {
			KLockGuard<KMutex> lock(m_mpMtx);
			return m_dat.empty();
        }

        inline size_t Size() const
        {
			KLockGuard<KMutex> lock(m_mpMtx);
			return m_dat.size();
        }

    private:
        std::map<KeyType, ValueType> m_dat;
        KMutex m_mpMtx;
    };

    template<typename IntegerType>
    class AtomicInteger
    {
    public:
        AtomicInteger(IntegerType v = 0) :m_ival(v) {}

        inline operator IntegerType() const { return Load(); }
        inline AtomicInteger& operator=(IntegerType v){ Exchange(v); return *this; }
        inline IntegerType operator++() { return FetchAdd(1) + 1; }//prefix
        inline IntegerType operator--() { return FetchSub(1) - 1; }//prefix
        inline IntegerType operator++(int) { return FetchAdd(1); }//suffix
        inline IntegerType operator--(int) { return FetchSub(1); }//suffix
        inline bool operator==(IntegerType v){ return Load() == v; }
        inline bool operator==(const AtomicInteger& rh){ return Load() == rh.Load(); }

    private:
        inline IntegerType FetchAdd(IntegerType val)
        {
#if defined(WIN32)
			KLockGuard<KMutex> lock(m_intMtx);
			IntegerType tmp = m_ival;
			m_ival += val;
			return tmp;
#else
			return __sync_fetch_and_add(&m_ival, val);
#endif
        }

        inline IntegerType FetchSub(IntegerType val)
        {
#if defined(WIN32)
            KLockGuard<KMutex> lock(m_intMtx);
            IntegerType tmp = m_ival;
            m_ival -= val;
            return tmp;
#else
            return __sync_fetch_and_sub(&m_ival, val);
#endif
        }

        inline IntegerType Load() const
        {
#if defined(WIN32)
			KLockGuard<KMutex> lock(m_intMtx);
			return m_ival;
#else
			return __sync_fetch_and_add(const_cast<IntegerType*>(&m_ival), 0);
#endif
        }

        inline IntegerType Exchange(IntegerType val)
        {
#if defined(WIN32)
			KLockGuard<KMutex> lock(m_intMtx);
			IntegerType tmp = m_ival;
			m_ival = val;
			return tmp;
#else
			__sync_synchronize();
			return __sync_lock_test_and_set(&m_ival, val);
#endif
        }

    private:
        IntegerType m_ival;
#if defined(WIN32)
        KMutex m_intMtx;
#endif
    };

    class AtomicBool
    {
    public:
        AtomicBool(bool v = false):m_dat(v ? 1 : 0){}
        inline operator bool() const{ return (int(m_dat) != 0 ? true : false); }
        inline AtomicBool& operator=(bool v){ m_dat = (v ? 1 : 0); return *this; }
        inline bool operator==(bool v) const{ return (operator bool() == v); }
        inline bool operator==(const AtomicBool& rh) const{ return (m_dat == rh.m_dat); }

    private:
        AtomicInteger<int> m_dat;
    };
};

#endif // !_ATOMIC_HPP_
