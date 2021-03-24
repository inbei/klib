#ifndef _PTHREAD_HPP_
#define _PTHREAD_HPP_
#include <iostream>
#include <pthread.h>
#include <string>
#include <stdint.h>
#include "thread/KMutex.h"
#include "thread/KLockGuard.h"
#include "thread/KException.h"
#include "thread/KError.h"
#include "thread/KAtomic.h"
/**
线程类
**/
namespace klib {
    class KPthread
    {
    public:
        enum ThreadError { AlreadRunning, InvalidArgument, CreateFailed, Success };
        KPthread(const std::string &name)
            :m_running(false),m_name(name)
        {
            memset(&m_tid, 0, sizeof(m_tid));
            int rc = pthread_attr_init(&m_attr);
            if (rc != 0)
            {
                throw KException(__FILE__, __LINE__, KError::StdErrorStr(rc).c_str());
            }
        }

        virtual ~KPthread()
        {
            int rc = pthread_attr_destroy(&m_attr);
            if (rc != 0)
            {
                throw KException(__FILE__, __LINE__, KError::StdErrorStr(rc).c_str());
            }
        }

        /************************************
        * Method:    启动线程执行成员函数
        * Returns:   
        * Parameter: obj 对象指针
        * Parameter: rf 函数指针
        * Parameter: arg 函数参数
        * Parameter: lf 记录日志的成员函数
        * Parameter: ret 返回值指针
        *************************************/
        template<typename ObjectType, typename RetType, typename ArgType>
         int Run(ObjectType* obj, RetType(ObjectType::* rf)(ArgType), ArgType arg, void(ObjectType::* lf)(const std::string&) = NULL, RetType* ret = NULL)
        {
            KLockGuard<KMutex> lock(m_tmtx);
            if (m_running)
            {
                return AlreadRunning;
            }

            if (!obj || !rf)
            {
                return InvalidArgument;
            }
            typedef RetType(ObjectType::* RunFunc)(ArgType);
            typedef void(ObjectType::* LogFunc)(const std::string&);
            struct PthreadData
            {
                ObjectType* obj;
                RunFunc rf;
                LogFunc lf;
                ArgType arg;
                RetType* ret;
                KPthread* self;
            };
            PthreadData* dat = new PthreadData;
            dat->obj = obj;
            dat->rf = rf;
            dat->lf = lf;
            dat->arg = arg;
            dat->ret = ret;
            dat->self = this;

            int rc = pthread_attr_setscope(&m_attr, PTHREAD_SCOPE_SYSTEM);
            if (rc != 0)
            {
                throw KException(__FILE__, __LINE__, KError::StdErrorStr(rc).c_str());
            }
            //detach thread
            //pthread_attr_setdetachstate(&_attr, PTHREAD_CREATE_DETACHED);
            rc = pthread_create(&m_tid, &m_attr, Work<ObjectType, RetType, ArgType>, dat);
            if (rc != 0)
            {
                delete dat;
                return CreateFailed;
            }
            return Success;
        }

        /************************************
        * Method:    启动线程执行全局函数或静态函数
        * Returns:   
        * Parameter: rf 函数指针
        * Parameter: arg 函数参数
        * Parameter: lf 记录日志的函数
        * Parameter: ret 返回值
        *************************************/
        template<typename RetType, typename ArgType>
        int Run(RetType(*rf)(ArgType), ArgType arg, void(*lf)(const std::string&) = NULL, RetType* ret = NULL)
        {
            KLockGuard<KMutex> lock(m_tmtx);
            if (m_running)
                return AlreadRunning;

            if (!rf)
                return InvalidArgument;

            typedef RetType(*RunFunc)(ArgType); 
            typedef void(*LogFunc)(const std::string&);
            struct PthreadData
            {
                RunFunc rf;
                LogFunc lf;
                ArgType arg;
                RetType* ret;
                KPthread* self;
            };
            PthreadData* dat = new PthreadData;
            dat->rf = rf;
            dat->lf = lf;
            dat->arg = arg;
            dat->ret = ret;
            dat->self = this;

            int rc = pthread_attr_setscope(&m_attr, PTHREAD_SCOPE_SYSTEM);
            if (rc != 0)
                throw KException(__FILE__, __LINE__, KError::StdErrorStr(rc).c_str());
            //detach thread
            //pthread_attr_setdetachstate(&_attr, PTHREAD_CREATE_DETACHED);
            rc = pthread_create(&m_tid, &m_attr, Work<RetType, ArgType>, dat);
            if (rc != 0)
            {
                delete dat;
                return CreateFailed;
            }
            return Success;
        }

        /************************************
        * Method:    等待线程结束
        * Returns:   
        *************************************/
        void Join()
        {
            //pthread_cancel(m_tid);
            int rc = 0;
            if ((rc = pthread_join(m_tid, NULL)) != 0)
                printf("thread exit failed, error:[%s]\n", GetErrStr(rc));
            else
                printf("thread exit success\n");
        }

        /************************************
        * Method:    线程是否运行中
        * Returns:   
        *************************************/
        inline bool IsRunning() const
        {
            KLockGuard<KMutex> lock(m_tmtx);
            return m_running;;
        }

        /************************************
        * Method:    获取当前线程ID
        * Returns:   
        *************************************/
        static uint32_t GetThreadId()
        {
#ifdef WIN32
            return pthread_getw32threadid_np(pthread_self());
#else
            return pthread_self();
#endif
        }

        /************************************
        * Method:    获取线程ID
        * Returns:   
        * Parameter: pid
        *************************************/
        static uint32_t GetThreadId(pthread_t pid)
        {
#ifdef WIN32
            return pthread_getw32threadid_np(pid);
#else
            return pid;
#endif
        }

        /************************************
        * Method:    测试取消点
        * Returns:   
        *************************************/
        static void TestCancel()
        {
            pthread_testcancel();
        }

    private:
        /************************************
        * Method:    执行成员函数
        * Returns:   
        * Parameter: arg
        *************************************/
        template<typename ObjectType, typename RetType, typename ArgType>
        static void* Work(void* arg)
        {
            //pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
            typedef RetType(ObjectType::* RunFunc)(ArgType);
            typedef void(ObjectType::* LogFunc)(const std::string&);
            struct PthreadData
            {
                ObjectType* obj;
                RunFunc rf;
                LogFunc lf;
                ArgType arg;
                RetType* ret;
                KPthread* self;
            };
            PthreadData* dat = reinterpret_cast<PthreadData*>(arg);
            {
                KLockGuard<KMutex> lock(dat->self->m_tmtx);
                dat->self->m_running = true;
            }
            if (dat->lf)
                (dat->obj->*dat->lf)(dat->self->m_name + " Running");

            if (dat->ret)
                *dat->ret = (dat->obj->*dat->rf)(dat->arg);
            else
                (dat->obj->*dat->rf)(dat->arg);
            {
                KLockGuard<KMutex> lock(dat->self->m_tmtx);
                dat->self->m_running = false;
            }
            
            if (dat->lf)
                (dat->obj->*dat->lf)(dat->self->m_name + " Stopped");

            delete dat;
            return 0;
        }

        /************************************
        * Method:    执行全局或静态函数
        * Returns:   
        * Parameter: arg
        *************************************/
        template<typename RetType, typename ArgType>
        static void* Work(void* arg)
        {
            //pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
            typedef RetType(*RunFunc)(ArgType);
            typedef void(*LogFunc)(const std::string&);
            struct PthreadData
            {
                RunFunc rf;
                LogFunc lf;
                ArgType arg;
                RetType* ret;
                KPthread* self;
            };
            PthreadData* dat = reinterpret_cast<PthreadData*>(arg);
            {
                KLockGuard<KMutex> lock(dat->self->m_tmtx);
                dat->self->m_running = true;
            }
            if (dat->lf)
                (*dat->lf)(dat->self->m_name + " Running");

            if(dat->ret)
                *dat->ret = (*dat->rf)(dat->arg);
            else
                (*dat->rf)(dat->arg);
            {
                KLockGuard<KMutex> lock(dat->self->m_tmtx);
                dat->self->m_running = false;
            }
            
            if (dat->lf)
                (*dat->lf)(dat->self->m_name + " Stopped");

            delete dat;
            return 0;
        }

        /************************************
        * Method:    获取错误信息
        * Returns:   
        * Parameter: rc
        *************************************/
        const char* GetErrStr(int rc) const
        {
            switch (rc)
            {
            case EDEADLK:
                return "A deadlock was detected (e.g., two threads tried to join with each other);"
                    " or thread specifies the calling thread";
            case EINVAL:
                return "Another thread is already waiting to join with this thread";
            case ESRCH:
                return "No thread with the ID thread could be found";
            default:
                return "Unknown thread error";
            }
        }

    private:
        pthread_attr_t m_attr;
        pthread_t m_tid;
        std::string m_name;
        KMutex m_tmtx;
        volatile bool m_running;
    };
};
#endif // !_PTHREAD_HPP_
