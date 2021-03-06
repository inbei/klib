#ifndef _EVENTOBJECT_HPP_
#define _EVENTOBJECT_HPP_
#include "thread/KQueue.h"
#include "thread/KPthread.h"
#include "thread/KAtomic.h"
#include "thread/KMutex.h"
#include "thread/KLockGuard.h"

#include <cassert>
#include <map>
#include <vector>

namespace klib {
    /*
    事件循环类
    */

    class KEventBase
    {
    public:
        KEventBase(const std::string& name)
            :m_eventThread(name), m_running(false)
        {
            KLockGuard<KMutex> lock(s_eobjmtx);
            m_objectID = ++s_eobjid;
            s_eobjmap[m_objectID] = this;
        }

        virtual ~KEventBase()
        {
            KLockGuard<KMutex> lock(s_eobjmtx);
            std::map<uint32_t, KEventBase*>::iterator it = s_eobjmap.find(m_objectID);
            if (it != s_eobjmap.end())
                s_eobjmap.erase(it);
        }

        /************************************
        * Method:    启动
        * Returns:   
        *************************************/
        virtual bool Start()
        {
            KLockGuard<KMutex> lock(m_wkMtx);
            return (m_running = (m_eventThread.Run(this, &KEventBase::EventLoop, 0, &KEventBase::Log)
                == KPthread::Success));
        }

        /************************************
        * Method:    停止
        * Returns:   
        *************************************/
        virtual void Stop()
        {
            KLockGuard<KMutex> lock(m_wkMtx);
            m_running = false;
        }

        /************************************
        * Method:    等待停止
        * Returns:   
        *************************************/
        virtual void WaitForStop()
        {
            m_eventThread.Join();
        }

        /************************************
        * Method:    是否运行中
        * Returns:   
        *************************************/
        inline bool IsRunning() const
        {
            KLockGuard<KMutex> lock(m_wkMtx);
            return m_running;
        }

        /************************************
        * Method:    是否就绪
        * Returns:   
        *************************************/
        virtual bool IsReady() const { return true; }

        inline uint32_t GetID() const
        {
            return m_objectID;
        }        
    protected:
        virtual int EventLoop(int)
        {
            return 0;
        }

        virtual void Log(const std::string& msg)
        {

        }

    protected:
        KPthread m_eventThread;
        AtomicInteger<uint32_t> m_objectID;
        KMutex m_wkMtx;
        volatile bool m_running;

        static KMutex s_eobjmtx;
        static AtomicInteger<uint32_t> s_eobjid;
        static std::map<uint32_t, KEventBase*> s_eobjmap;
    };

    template<typename EventType>
    class KEventObject:public KEventBase
    {
    public:
        KEventObject(const std::string& name, size_t maxSize = 50)
            :m_eventQueue(maxSize),KEventBase(name)
        {

        }

        /************************************
        * Method:    取出所有数据
        * Returns:   
        * Parameter: events
        *************************************/
        inline void Flush(typename std::vector<EventType>& events)
        {
            m_eventQueue.GetAll(events);
        }

        /************************************
        * Method:    清空数据
        * Returns:   
        *************************************/
        inline void Clear()
        {
            m_eventQueue.Clear();
        }

        /************************************
        * Method:    是否是空的
        * Returns:   
        *************************************/
        inline bool IsEmpty() const
        {
            return m_eventQueue.IsEmpty();
        }

        /************************************
        * Method:    是否满了
        * Returns:   
        *************************************/
        inline bool IsFull() const
        {
            return m_eventQueue.IsFull();
        }

        /************************************
        * Method:    队列大小
        * Returns:   
        *************************************/
        inline size_t Size() const
        {
            return m_eventQueue.Size();
        }

        /************************************
        * Method:    消息入队
        * Returns:   
        * Parameter: ev 事件
        *************************************/
        virtual bool Post(const EventType& ev)
        {
            if(IsRunning())
                return m_eventQueue.PushBack(ev);
            return false;
        }

        /************************************
        * Method:    强势插入队列
        * Returns:   
        * Parameter: ev 事件
        *************************************/
        virtual void PostForce(const EventType& ev)
        {
            if(IsRunning())
                m_eventQueue.PushBackForce(ev);
        }

        /************************************
        * Method:    消息入队
        * Returns:   
        * Parameter: id
        * Parameter: ev 事件
        *************************************/
        static bool Post(uint32_t id, const EventType& ev)
        {
            KLockGuard<KMutex> lock(s_eobjmtx);
            std::map<uint32_t, KEventBase*>::iterator it = s_eobjmap.find(id);
            if (it != s_eobjmap.end() && it->second->IsRunning())
            {
                KEventBase* b = it->second;
                return dynamic_cast<KEventObject<EventType>*>(b)->Post(ev);
            }
            return false;
        }

        /************************************
        * Method:    强势插入队列
        * Returns:   
        * Parameter: id
        * Parameter: ev 事件
        *************************************/
        static void PostForce(uint32_t id, const EventType& ev)
        {
            KLockGuard<KMutex> lock(s_eobjmtx);
            std::map<uint32_t, KEventBase*>::iterator it = s_eobjmap.find(id);
            if (it != s_eobjmap.end() && it->second->IsRunning())
            {
                KEventBase* b = it->second;
                return dynamic_cast<KEventObject<EventType>*>(b)->PostForce(ev);
            }
            return false;
        }

    protected:
        virtual void ProcessEvent(const EventType& ev) = 0;

    private:
        int EventLoop(int)
        {
            while (IsRunning())
            {
                if (IsReady())
                {
                    EventType evt;
                    while (IsRunning() && IsReady() && m_eventQueue.PopFront(evt))
                    {
                        try
                        {
                            ProcessEvent(evt);
                        }
                        catch (const std::exception& e)
                        {
                            printf("KEventObject exception:[%s]\n", e.what());
                        }
                        catch (...)
                        {
                            assert(false);
                            printf("KEventObject unknown exception\n");
                        }
                    }
                }
                else
                    KTime::MSleep(100);
            }
            return 0;
        }

    private:
        KQueue<EventType> m_eventQueue;
    };
};
#endif // !_EVENTOBJECT_HPP_

