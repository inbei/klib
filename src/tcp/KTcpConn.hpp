﻿#pragma once
#if defined(WIN32)
#include <WS2tcpip.h>
#endif
#include "tcp/KTcpProcessor.hpp"
#include "tcp/KTcpWriter.h"
namespace klib {

    template<typename ProcessorType>
    class KTcpConnection :protected KEventObject<SocketType>, public KTcpBase,
        protected ProcessorType, protected KTcpWriter
    {
    public:
        KTcpConnection()
            :m_fd(0), KEventObject<SocketType>("KTcpConnection Thread")
        {

        }

        ~KTcpConnection()
        {
            std::cout << m_ipport << " disconnected" << std::endl;
        }

        bool Start(const std::string& ipport, SocketType fd)
        {
            m_ipport = ipport;
            m_fd = fd;

            if (!AddSocket(fd, ipport))
                return false;

            if (!KTcpWriter::Start(this))
            {
                DeleteSocket(fd);
                return false;
            }

            if (!ProcessorType::Start(this))
            {
                DeleteSocket(fd);
                KTcpWriter::Stop();
                return false;
            }

            if (!KEventObject<SocketType>::Start())
            {
                DeleteSocket(fd);
                ProcessorType::Stop();
                KTcpWriter::Stop();
                return false;
            }

            KEventObject<SocketType>::Post(0);
            return true;
        }

        void Stop()
        {
            KEventObject<SocketType>::Stop();
            while (!KTcpWriter::IsEmpty())
                KTime::MSleep(3);
            KTcpWriter::Stop();
            while (!ProcessorType::IsEmpty())
                KTime::MSleep(3);
            ProcessorType::Stop();
        }

        void WaitForStop()
        {
            KEventObject<SocketType>::WaitForStop();
            KTcpWriter::WaitForStop();
            ProcessorType::WaitForStop();
        }

        bool Send(const KBuffer& msg)
        {
            if (IsConnected())
                return KTcpWriter::Post(msg);
            return false;
        }

        virtual bool IsServer() const { return true; }

        virtual SocketType GetSocket() const { return m_fd; }

    private:
        virtual void ProcessEvent(const SocketType& ev)
        {
            if (IsConnected())
            {
                PollSocket();
                KEventObject<SocketType>::Post(1);
            }
        }

        virtual void OnSocketEvent(SocketType fd, short evt)
        {
            if (evt & epollin)
            {
                std::vector<KBuffer> dat;
                if (ReadSocket(fd, dat) < 0)
                    DeleteSocketNoLock(fd);
                else if (!ProcessorType::Post(dat))
                {
                    std::vector<KBuffer>::iterator it = dat.begin();
                    while (it != dat.end())
                    {
                        it->Release();
                        ++it;
                    }
                }
            }
            else if (evt & epollhup || evt & epollerr)
            {
                DeleteSocketNoLock(fd);
            }
        }

        int ReadSocket(SocketType fd, std::vector<KBuffer>& dat) const
        {
            int bytes = 0;
            int rc = 1;
            char buf[BlockSize] = { 0 };
            while (rc > 0 && fd > 0)
            {
                rc = ::recv(fd, buf, BlockSize, 0/*MSG_DONTWAIT  MSG_WAITALL*/);
                if (rc > 0)
                {
                    KBuffer b(rc);
                    b.ApendBuffer(buf, rc);
                    dat.push_back(b);
                    bytes += rc;
                }
                else
                {
#if defined(WIN32)
                    if (GetLastError() == WSAEINTR) // 读操作中断，需要重新读
                        KTime::MSleep(3);
                    else if (GetLastError() == WSAEWOULDBLOCK) // 非阻塞模式，暂时无数据，不需要重新读
                        break;
#else
                    if (errno == EINTR) // 读操作中断，需要重新读
                        KTime::MSleep(3);
                    else if (errno == EWOULDBLOCK) // 非阻塞模式，暂时无数据，不需要重新读
                        break;
#endif
                    else // 错误断开连接
                        return -1;
                }
            }
            return bytes;
        }

    private:
        SocketType m_fd;
        std::string m_ipport;
    };
};