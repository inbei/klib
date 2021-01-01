#pragma once
#if defined(WIN32)
#include <WS2tcpip.h>
#endif
#include "tcp/KTcpProcessor.hpp"
#include "tcp/KTcpWriter.h"
#include "util/KStringUtility.h"
namespace klib {

    template<typename ProcessorType>
    class KTcpClient :protected KEventObject<SocketType>, public KTcpBase,
        protected ProcessorType, protected KTcpWriter
    {
    public:
        KTcpClient()
            :m_fd(0), KEventObject<SocketType>("KTcpClient Thread"),m_autoReconnect(false)
        {

        }

        bool Start(const std::string& hosts, bool autoReconnect = true)
        {
            std::vector<std::string> brokers;
            klib::KStringUtility::SplitString(hosts, ",", brokers);
            std::vector<std::string>::const_iterator it = brokers.begin();
            while (it != brokers.end())
            {
                const std::string& broker = *it;
                std::vector<std::string> ipandport;
                klib::KStringUtility::SplitString(broker, ":", ipandport);
                if (ipandport.size() != 2)
                {
                    printf("invalid broker:[%s]\n", broker.c_str());
                }
                else
                {
                    uint16_t port = atoi(ipandport[1].c_str());
                    if (port == 0)
                        printf("invalid broker:[%s]\n", broker.c_str());
                    else
                        m_hostip[ipandport[0]] = port;
                }
                ++it;
            }

            if (m_hostip.empty())
            {
                printf("invalid brokers:[%s]\n", hosts.c_str());
                return false;
            }

            m_it = m_hostip.begin();
            m_autoReconnect = autoReconnect;
            if (!KTcpWriter::Start(this))
                return false;

            if (!ProcessorType::Start(this))
            {
                KTcpWriter::Stop();
                KTcpWriter::WaitForStop();
                return false;
            }

            if (!KEventObject<SocketType>::Start())
            {
                ProcessorType::Stop();
                KTcpWriter::Stop();
                ProcessorType::WaitForStop();
                KTcpWriter::WaitForStop();
                return false;
            }

            KEventObject<SocketType>::Post(0);
            return true;
        }

        virtual void Stop()
        {
            KEventObject<SocketType>::Stop();
            KTcpWriter::Stop();
            ProcessorType::Stop();
        }

        virtual void WaitForStop()
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

        virtual bool IsServer() const { return false; }

        virtual bool Handshake() { return ProcessorType::Handshake(); }

        virtual void Serialize(const typename ProcessorType::ProcessorMessageType& msg, KBuffer& result) const { ProcessorType::Serialize(msg, result); }

    private:
        virtual void ProcessEvent(const SocketType& ev)
        {
            if (!IsConnected())
            {
                if (m_autoReconnect && !Connect(m_it->first, m_it->second))
                {
                    if (++m_it == m_hostip.end())
                        m_it = m_hostip.begin();
                }
                KTime::MSleep(5000);
            }
            else
            {
                PollSocket();
            }
            KEventObject<SocketType>::Post(1);
        }

        virtual void OnSocketEvent(SocketType fd, short evt)
        {
            if (evt & epollin)
                ReadSocket(fd);
            else if (evt & epollhup || evt & epollerr)
                DeleteSocketNoLock(fd);
        }

        virtual SocketType GetSocket() const { return m_fd; }

        bool Connect(const std::string& ip, uint16_t port)
        {
            if ((m_fd = ::socket(AF_INET, SOCK_STREAM, 0)) < 0)
                return false;

            DisableNagle(m_fd);

            sockaddr_in server;
            server.sin_family = AF_INET;
            server.sin_port = htons(port);
            server.sin_addr.s_addr = inet_addr(ip.c_str());
            if (::connect(m_fd, (sockaddr*)(&server), sizeof(server)) != 0)
            {
                CloseSocket(m_fd);
                return false;
            }

            std::ostringstream os;
            os << ip << ":" << port;
            return AddSocket(m_fd, os.str());
        }

        void ReadSocket(SocketType fd)
        {
            std::vector<KBuffer> dat;
            if (ReadSocket(fd, dat) < 0)
                DeleteSocketNoLock(fd);

            if (!dat.empty() && !ProcessorType::Post(dat))
            {
                std::vector<KBuffer>::iterator it = dat.begin();
                while (it != dat.end())
                {
                    it->Release();
                    ++it;
                }
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
                        KTime::MSleep(6);
                    else if (GetLastError() == WSAEWOULDBLOCK) // 非阻塞模式，暂时无数据，不需要重新读
                        break;
#else
                    if (errno == EINTR) // 读操作中断，需要重新读
                        KTime::MSleep(6);
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
        bool m_autoReconnect;
        std::map<std::string, uint16_t> m_hostip;
        std::map<std::string, uint16_t>::const_iterator m_it;
    };
};
