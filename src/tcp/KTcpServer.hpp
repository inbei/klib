#pragma once
#if defined(WIN32)
#include <WS2tcpip.h>
#endif
#include <sstream>
#include "thread/KEventObject.h"
#include "tcp/KTcpBase.h"
#include "tcp/KTcpConn.hpp"

namespace klib {

    template<typename ProcessorType>
    class KTcpServer :public KEventObject<SocketType>, public KTcpBase
    {
    public:
        KTcpServer()
            :m_fd(0), KEventObject<SocketType>("KTcpServer Thread"), m_maxClient(50)
        {

        }

        bool Start(const std::string& ip, uint16_t port, bool autoReconnect = true)
        {
            m_ip = ip;
            m_port = port;
            m_autoReconnect = autoReconnect;

            if (!KEventObject<SocketType>::Start())
                return false;
            KEventObject<SocketType>::Post(0);
            return true;
        }

        void Stop()
        {
            KEventObject<SocketType>::Stop();
            CleanConnections();
        }

        bool Post(int cid, const KBuffer& dat)
        {
            KLockGuard<KMutex>  lock(m_connMtx);
            typename std::map<int, KTcpConnection<ProcessorType>* >::iterator it = m_connections.find(cid);
            if (it != m_connections.end())
                return it->second && it->second->IsConnected() && it->second->Send(dat);
            return false;
        }

        bool Post(int cid, const std::string& dat)
        {
            KLockGuard<KMutex>  lock(m_connMtx);
            typename std::map<int, KTcpConnection<ProcessorType>* >::iterator it = m_connections.find(cid);
            if (it != m_connections.end())
                return it->second && it->second->IsConnected() && it->second->Send(dat);
            return false;
        }

        virtual void SetMaxClient(uint32_t c)
        {
            m_maxClient = c;
        }

    private:
        virtual void ProcessEvent(const SocketType& ev)
        {
            if (!IsConnected())
            {
                if (!Listen(m_ip, m_port))
                    KTime::MSleep(1000);
            }
            else
            {
                if (PollSocket() <= 0)
                    CleanZombies();
            }
            KEventObject<SocketType>::Post(1);
        }

        virtual void OnSocketEvent(SocketType fd, short evt)
        {
            std::cout << __FUNCTION__  << m_connections.size() << std::endl;
            if (evt & epollin)
                AcceptSocket(fd);
        }

        virtual SocketType GetSocket() const { return m_fd; }

        virtual KTcpConnection<ProcessorType>* NewConnection() { return new KTcpConnection<ProcessorType>; }

    private:
        bool Listen(const std::string& ip, uint16_t port)
        {
            m_fd = ::socket(AF_INET, SOCK_STREAM, 0);
            if (m_fd < 0)
                return false;

            ReuseAddress(m_fd);
            DisableNagle(m_fd);

            sockaddr_in server;
            server.sin_family = AF_INET;
            server.sin_port = htons(port);
            server.sin_addr.s_addr = inet_addr(ip.c_str()); // htonl(INADDR_ANY);
            if (::bind(m_fd, (sockaddr*)(&server), sizeof(server)) != 0)
            {
                CloseSocket(m_fd);
                return false;
            }

            if (::listen(m_fd, 200) != 0)
            {
                CloseSocket(m_fd);
                return false;
            }

            std::ostringstream os;
            os << ip << ":" << port;
            return AddSocket(m_fd, os.str());
        }

        void AcceptSocket(SocketType fd)
        {
            SocketType s = 0;
            sockaddr_in caddr = { 0 };
            SocketLength addrlen = sizeof(caddr);
            std::cout << __FUNCTION__ << "START" << m_connections.size() << std::endl;
#if defined(WIN32)
            while ((s = ::accept(fd, (struct sockaddr*)&caddr, &addrlen)) != INVALID_SOCKET)
#else
            while ((s = ::accept(fd, (struct sockaddr*)&caddr, &addrlen)) > 0)
#endif
            {
                std::ostringstream os;
                os << inet_ntoa(caddr.sin_addr) << ":" << ntohs(caddr.sin_port);
                std::string ipport = os.str();
                std::cout << ipport << " connected" << std::endl;
                AddConnection(s, ipport);
            }
            std::cout << __FUNCTION__ << "end" << m_connections.size() << std::endl;
        }

        void AddConnection(SocketType fd, const std::string& ipport)
        {
            //std::cout << __FUNCTION__ << "start" << m_connections.size() << std::endl;
            KLockGuard<KMutex>  lock(m_connMtx);
            if (m_connections.size() < m_maxClient)
            {
                KTcpConnection<ProcessorType>* c = NewConnection();
                if (c->Start(ipport, fd))
                    m_connections[fd] = c;
                else
                {
                    printf("Create connection thread failed\n");
                    delete c;
                }
            }
            else
            {
                printf("Max connection count is %d\n", m_maxClient);
                klib::KTime::MSleep(100);
                CloseSocket(fd);
            }
            //std::cout << __FUNCTION__ << "end" << m_connections.size() << std::endl;
        }

        void CleanConnections()
        {
            KLockGuard<KMutex>  lock(m_connMtx);
            typename std::map<int, KTcpConnection<ProcessorType>* >::iterator it = m_connections.begin();
            if (it != m_connections.end())
                it->second->Stop();

            it = m_connections.begin();
            if (it != m_connections.end())
            {
                it->second->WaitForStop();
                delete it->second;
            }
            m_connections.clear();
        }

        void CleanZombies()
        {
            KLockGuard<KMutex>  lock(m_connMtx);
            std::vector<KTcpConnection<ProcessorType>* > corpses;
            typename std::map<int, KTcpConnection<ProcessorType>* >::iterator it = m_connections.begin();
            while (it != m_connections.end())
            {
                KTcpConnection<ProcessorType>* c = it->second;
                if (c->IsConnected())
                    ++it;
                else
                {
                    corpses.push_back(c);
                    m_connections.erase(it++);
                    c->Stop();
                }
            }
            typename std::vector<KTcpConnection<ProcessorType>* >::const_iterator cit = corpses.begin();
            while (cit != corpses.end())
            {
                KTcpConnection<ProcessorType>* c = *cit;
                c->WaitForStop();
                delete c;
                ++cit;
            }
        }

    private:
        SocketType m_fd;
        std::string m_ip;
        uint16_t m_port;
        bool m_autoReconnect;
        uint32_t m_maxClient;
        KMutex m_connMtx;
        std::map<int, KTcpConnection<ProcessorType>*> m_connections;
    };
};
