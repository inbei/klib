#pragma once
#include "KTcpProcessor.hpp"
#include "KTcpWriter.h"
namespace klib {

    template<typename ProcessorType>
    class KTcpClient :protected KEventObject<SocketType>, public KTcpBase,
        protected ProcessorType, protected KTcpWriter
    {
    public:
        KTcpClient()
            :m_fd(0), KEventObject<SocketType>("KTcpClient Thread")
        {

        }

        bool Start(const std::string& ip, uint16_t port, bool autoReconnect = true)
        {
            m_ip = ip;
            m_port = port;
            m_autoReconnect = autoReconnect;
            if (!KTcpWriter::Start(this))
                return false;

            if (!ProcessorType::Start(this))
            {
                KTcpWriter::Stop();
                return false;
            }

            if (!KEventObject<SocketType>::Start())
            {
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

        virtual bool IsServer() const { return false; }

        virtual bool Handshake() { return ProcessorType::Handshake(); }

        virtual void Serialize(const typename ProcessorType::ProcessorMessageType& msg, KBuffer& result) const { ProcessorType::Serialize(msg, result); }

    private:
        virtual void ProcessEvent(const SocketType& ev)
        {
            if (!IsConnected())
            {
                if (!Connect(m_ip, m_port))
                    KTime::MSleep(1000);
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

        virtual SocketType GetSocket() const { return m_fd; }

        bool Connect(const std::string& ip, uint16_t port)
        {
            if ((m_fd = ::socket(AF_INET, SOCK_STREAM, 0)) < 0)
                return false;

            // set reuse address
            int on = 1;
            if (::setsockopt(m_fd, SOL_SOCKET, SO_REUSEADDR,
                reinterpret_cast<const char*>(&on), sizeof(on)) != 0)
            {
                CloseSocket(m_fd);
                return false;
            }

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
        std::string m_ip;
        uint16_t m_port;
        bool m_autoReconnect;
    };
};
