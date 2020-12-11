#ifndef _QUEUE_HPP_
#define _QUEUE_HPP_
#include <deque>
#include <assert.h>
#include "KMutex.h"
#include "KLockGuard.h"
#include "KCondVariable.h"
#include "KAtomic.h"

namespace klib {
    template<typename ElementType>
    class KQueue :private std::deque<ElementType>
    {
        typedef std::deque<ElementType> QueueBase;
    public:
		KQueue(size_t maxsize)
		{
			assert(maxsize > 0);
			m_queueMaxSize = maxsize;
		}

        ~KQueue()
        {
#if defined(AIX) || defined(HPUX)
			m_queueMutex.Unlock();
			m_emptyCond.NotifyAll();
			m_fullCond.NotifyAll();
#endif
			KLockGuard<KMutex> lock(m_queueMutex);
        }

        // 从后面批量追加元素，如果空间不够则返回false，否则放入元素返回true
        template<typename ContainerType>
        bool PushBackBatch(const ContainerType& dat)
        {
            bool qempty = false;
            {
                KLockGuard<KMutex> lock(m_queueMutex);
                if (dat.size() > m_queueMaxSize - QueueBase::size())
					return false;
                qempty = QueueBase::empty();
                QueueBase::insert(QueueBase::end(), dat.begin(), dat.end());
            }
            if (qempty)
				m_emptyCond.NotifyAll();
            return true;
        }

        /*
        * Description: 从后面追加一个元素， ms < 0 一直等待直到有空缺时再放进去并返回true，
        * ms >= 0 等待 ms 毫秒，期间如果一直没有空缺则返回false，否则追加元素返回true
        */
        bool PushBack(const ElementType& v, int ms = 0)
        {
			bool qempty = false;
			bool rc = false;
			{
				KLockGuard<KMutex> lock(m_queueMutex);
				qempty = QueueBase::empty();
				if (QueueBase::size() == m_queueMaxSize)
				{
					if (ms < 0)
					{
						while (QueueBase::size() == m_queueMaxSize)
							m_fullCond.Wait(lock);
					}
					else if ((ms > 0 && !m_fullCond.TimedWait(lock, ms))
						|| ms == 0)
						return false;
				}

				if (QueueBase::size() < m_queueMaxSize)
				{
					QueueBase::push_back(v);
					rc = true;
				}
			}
			if (qempty)
				m_emptyCond.NotifyAll();
			return rc;
        }

		/*
		* Description: 从前面追加一个元素， ms < 0 一直等待直到有空缺时再放进去并返回true，
		* ms >= 0 等待 ms 毫秒，期间如果一直没有空缺则返回false，否则追加元素返回true
		*/
		bool PushFront(const ElementType& v, int ms = 0)
		{
			bool qempty = false;
			bool rc = false;
			{
				KLockGuard<KMutex> lock(m_queueMutex);
				qempty = QueueBase::empty();
				if (QueueBase::size() == m_queueMaxSize)
				{
					if (ms < 0)
					{
						while (QueueBase::size() == m_queueMaxSize)
							m_fullCond.Wait(lock);
					}
					else if ((ms > 0 && !m_fullCond.TimedWait(lock, ms))
						|| ms == 0)
					{
						return false;
					}
				}

				if (QueueBase::size() < m_queueMaxSize)
				{
					QueueBase::push_front(v);
					rc = true;
				}
			}
			if (qempty)
				m_emptyCond.NotifyAll();
			return rc;
		}

        void PushBackForce(const ElementType& v)
        {
			bool qempty = false;
			{
				KLockGuard<KMutex> lock(m_queueMutex);
				qempty = QueueBase::empty();
				if (QueueBase::size() == m_queueMaxSize)
					QueueBase::pop_front();
				QueueBase::push_back(v);
			}

			if (qempty)
				m_emptyCond.NotifyAll();
        }

        void PushFrontForce(const ElementType& v)
        {
			bool qempty = false;
			{
				KLockGuard<KMutex> lock(m_queueMutex);
				qempty = QueueBase::empty();
				if (QueueBase::size() == m_queueMaxSize)
					QueueBase::pop_front();
				QueueBase::push_front(v);
			}

			if (qempty)
				m_emptyCond.NotifyAll();
        }

        /*
        * Description: 从前面取出一个元素， ms < 0 一直等待直到有元素时再取并返回true，
        * ms >= 0 等待 ms 毫秒，期间如果一直没有元素则返回false，否则取出元素返回true
        */
        bool PopFront(ElementType& v, int ms = 500)
        {
			bool qfull = false;
			bool rc = false;
			{
				KLockGuard<KMutex> lock(m_queueMutex);
				qfull = (QueueBase::size() == m_queueMaxSize);
				if (QueueBase::empty())
				{
					if (ms < 0)
					{
						while (QueueBase::empty())
							m_emptyCond.Wait(lock);
					}
					else if ((ms > 0 && !m_emptyCond.TimedWait(lock, ms)) || ms == 0)
						return false;
				}

				if (!QueueBase::empty())
				{
					v = QueueBase::front();
					QueueBase::pop_front();
					rc = true;
				}
			}
			if (qfull)
				m_fullCond.NotifyAll();
			return rc;
        }

		inline size_t Size() const
        {
			KLockGuard<KMutex> lock(m_queueMutex);
			return QueueBase::size();
        }

		inline void Clear()
        {
			KLockGuard<KMutex> lock(m_queueMutex);
			QueueBase::clear();
        }

        // 查看指定的元素
		inline ElementType& Peek(size_t i)
        {
			KLockGuard<KMutex> lock(m_queueMutex);
			return QueueBase::operator [](i);
        }

        // 查看所有元素
        template<typename ContainerType>
		inline void PeekAll(ContainerType& dat)
        {
            KLockGuard<KMutex> lock(m_queueMutex);
			if (!QueueBase::empty())
				ContainerType(QueueBase::begin(), QueueBase::end()).swap(dat);
        }

		// 取出所有元素
		template<typename ContainerType>
		inline void GetAll(ContainerType& dat)
		{
			KLockGuard<KMutex> lock(m_queueMutex);
			if (!QueueBase::empty())
			{
				ContainerType(QueueBase::begin(), QueueBase::end()).swap(dat);
				QueueBase::clear();
			}
		}

		// 取出部分元素
		template<typename ContainerType>
		inline void GetPart(size_t count, ContainerType& dat)
		{
			KLockGuard<KMutex> lock(m_queueMutex);
			if (!QueueBase::empty())
			{
				count = (QueueBase::size() > count ? count : QueueBase::size());
				typename QueueBase::iterator it = QueueBase::begin();
				std::advance(it, count);

				ContainerType(QueueBase::begin(), it).swap(dat);
				QueueBase::erase(QueueBase::begin(), it);
			}
		}

		inline bool IsEmpty() const
        {
			KLockGuard<KMutex> lock(m_queueMutex);
			return QueueBase::empty();
        }

        inline bool IsFull() const
        {
			KLockGuard<KMutex> lock(m_queueMutex);
			return QueueBase::size() == m_queueMaxSize;
        }

    private:
        size_t m_queueMaxSize;
        KMutex m_queueMutex;
        KCondVariable m_emptyCond;
        KCondVariable m_fullCond;
    };
};
#endif // !_QUEUE_HPP_

