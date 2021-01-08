#ifndef _KTCPBASE_HPP_
#define _KTCPBASE_HPP_

#include "KTcpConnection.hpp"
namespace klib {
    template<typename MessageType>
    class KTcpNetwork: public KEventObject<SocketType>
    {
    public:
        KTcpNetwork()
            :KEventObject<SocketType>("Poll thread", 50),m_connected(false), 
            m_isServer(false),m_needAuth(false),m_maxClient(50)
        {
#if defined(WIN32)
            WSADATA wsd;
            assert(WSAStartup(MAKEWORD(2, 2), &wsd) == 0);
#elif defined(AIX)
            m_pfd = pollset_create(50);
#elif defined(LINUX)
            m_pfd = epoll_create1(0);
#endif
        }
        virtual ~KTcpNetwork()
        {
#if defined(WIN32)
            WSACleanup();
#elif defined(AIX)
            pollset_destroy(m_pfd);
#elif defined(LINUX)
            close(m_pfd);
#endif
        }

        static  void Release(std::vector<KBuffer>& bufs)
        {
            std::vector<KBuffer>::iterator it = bufs.begin();
            while (it != bufs.end())
            {
                it->Release();
                ++it;
            }
            bufs.clear();
        }

        inline bool IsConnected() const { return m_connected; }

        inline void SetMaxClient(uint16_t mc) { m_maxClient = mc; }
        
        virtual bool Start(const std::string& ip, int32_t port, bool isServer = true, bool needAuth = false)
        {
            m_ip = ip;
            m_port = port;
            m_isServer = isServer;
            m_needAuth = needAuth;
            if (KEventObject<SocketType>::Start())
            {
                PostForce(0);
                return true;
            }
            return false;
        }

        bool SendDataToSelf(const std::vector<KBuffer>& bufs)
        {
            if (m_isServer)
                return false;
            return SendDataToConnection(m_fd, SocketEvent::SeSent, bufs);
        }

        bool SendDataToConnection(SocketType fd, SocketEvent::EventType et,const std::vector<KBuffer>& bufs)
        {
            KLockGuard<KMutex> lock(m_connMtx);
            typename std::map<SocketType, KTcpConnection<MessageType>*>::iterator it = m_connections.find(fd);
            if (it != m_connections.end())
            {
                KTcpConnection<MessageType>* c = it->second;
                SocketEvent e;
                e.fd = fd;
                e.ev = et;
                e.dat1 = bufs;
                if (c->IsConnected())
                {
                    if (!c->Post(e))
                        printf("Send data to connection failed, fd:[%d]\n", fd);
                    else
                        return true;
                }
            }
            return false;
        }

        inline SocketType GetSocket() const { return m_fd; }
        
        const std::string& GetConnectionInfo(SocketType fd)
        {
            KLockGuard<KMutex> lock(m_connMtx);
            typename std::map<SocketType, KTcpConnection<MessageType>*>::iterator it = m_connections.find(fd);
            if (it != m_connections.end())
                return it->second->GetAddress();
            return std::string();
        }
    protected:        
        virtual KTcpConnection<MessageType>* NewConnection(SocketType fd, const std::string& ipport)
        {
            return new KTcpConnection<MessageType>(this);
        }

        virtual std::pair<std::string, uint16_t> GetConfig() const {
            return std::pair<std::string, uint16_t>(m_ip, m_port);
        }

        void DisconnectConnection(SocketType fd)
        {
            KLockGuard<KMutex> lock(m_connMtx);
            typename std::map<SocketType, KTcpConnection<MessageType>*>::iterator it = m_connections.find(fd);
            if (it != m_connections.end())
            {
                KTcpConnection<MessageType>* c = it->second;
                c->Disconnect(fd);
            }
        }

    private:
        virtual void ProcessEvent(const SocketType& ev)
        {
            if (m_isServer)
            {
                if (m_connected)
                {
                    PollSocket();
                }
                else
                {
                    std::pair<std::string, uint16_t> conf = GetConfig();
                    if ((m_fd = Listen(conf.first, conf.second)) > 0)
                    {
                        std::ostringstream os;
                        os << conf.first << ":" << conf.second;
                        AddSocket(m_fd, os.str(), false);
                    }
                    else
                        KTime::MSleep(1000);
                }
            }
            else
            {
                if (m_connected)
                {
                    if (PollSocket() < 1)
                        ReadSocket2(m_fd);
                }
                else
                {
                    std::pair<std::string, uint16_t> conf = GetConfig();
                    if ((m_fd = Connect(conf.first, conf.second)) > 0)
                    {
                        std::ostringstream os;
                        os << conf.first << ":" << conf.second;
                        AddSocket(m_fd, os.str());
                    }
                    else
                        KTime::MSleep(1000);
                }
            }
            PostForce(0);
        }

        void CreateConnection(SocketType fd, const std::string& ipport)
        {
            KLockGuard<KMutex> lock(m_connMtx);
            KTcpConnection<MessageType>* recycle = NULL;
            typename std::map<SocketType, KTcpConnection<MessageType>*>::iterator it = m_connections.begin();
            while (it != m_connections.end())
            {
                if (it->second->IsDisconnected())
                {
                    recycle = it->second;
                    m_connections.erase(it);
                    break;
                }
                ++it;
            }
            if (recycle)
            {
                recycle->Connect(ipport, fd);
                m_connections[fd] = recycle;
                printf("Recycle connection started success\n");
            }
            else
            {
                if (m_connections.size() < m_maxClient)
                {
                    KTcpConnection<MessageType>* c = NewConnection(fd, ipport);
                    if (c->Start(m_isServer ? NmServer : NmClient, ipport, fd, m_needAuth))
                    {
                        m_connections[fd] = c;
                        printf("New connection started success\n");
                    }
                    else
                    {
                        DeleteSocket(fd);
                        delete c;
                        printf("Connection started failed\n");
                    }
                }
                else
                {
                    DeleteSocket(fd);
                    printf("Reach max client count:[%d]\n", m_maxClient);
                }
            }
        }        
        
        int PollSocket()
        {
            int rc = 0;
#if defined(WIN32)
            std::vector<pollfd> fds;
            {
                KLockGuard<KMutex> lock(m_fdsMtx);
                fds = m_fds;
            }
            if (!fds.empty())
            {
                rc = WSAPoll(&fds[0], fds.size(), PollTimeOut);
                for (size_t i = 0; rc > 0 && i < fds.size(); ++i)
                    ProcessSocketEvent(fds[i].fd, fds[i].revents);
            }
#elif defined(HPUX)
            std::vector<pollfd> fds;
            {
                KLockGuard<KMutex> lock(m_fdsMtx);
                fds = m_fds;
            }
            if (!fds.empty())
            {
                rc = ::poll(&fds[0], nfds_t(fds.size()), PollTimeOut);
                for (size_t i = 0; rc > 0 && i < fds.size(); ++i)
                    ProcessSocketEvent(fds[i].fd, fds[i].revents);
            }
#elif defined(LINUX)
            rc = epoll_wait(m_pfd, m_ps, MaxEvent, PollTimeOut);
            for (int i = 0; i < rc; ++i)
                ProcessSocketEvent(m_ps[i].data.fd, m_ps[i].events);
#elif defined(AIX)
            rc = pollset_poll(m_pfd, m_ps, MaxEvent, PollTimeOut);
            for (int i = 0; i < rc; ++i)
                ProcessSocketEvent(m_ps[i].fd, m_ps[i].revents);
#endif
            return rc;
        }

        inline bool IsSelfSocket(SocketType fd) const { return m_fd == fd; }

        void ProcessSocketEvent(SocketType fd, short evt)
        {
            if (evt & epollin)
            {
                if (m_isServer && IsSelfSocket(fd))
                    AcceptSocket(fd);
                else
                    ReadSocket2(fd);
            }
            else if (evt & epollhup || evt & epollerr)
            {
                DisconnectConnection(fd);
            }
        }

        void ReadSocket2(SocketType fd)
        {
            std::vector<KBuffer> bufs;
            if (ReadSocket(fd, bufs) < 0)
                DisconnectConnection(fd);

            if (!bufs.empty())
            {
                if (!SendDataToConnection(fd, SocketEvent::SeRecv, bufs))
                    Release(bufs);
            }
        }

        bool DeleteSocket(SocketType fd)
        {
            bool rc = false;
            {
                KLockGuard<KMutex> lock(m_fdsMtx);
                std::vector<pollfd>::iterator it = m_fds.begin();
                while (it != m_fds.end())
                {
                    if (it->fd == fd)
                    {
                        m_fds.erase(it);
#if defined(AIX)
                        poll_ctl ev;
                        ev.fd = fd;
                        // PS_ADD PS_MOD PS_DELETE
                        ev.cmd = PS_DELETE;
                        //int rc = pollset_ctl(pollset_t ps, struct poll_ctl* pollctl_array,int array_length)
                        pollset_ctl(m_pfd, &ev, 1);
#elif defined(LINUX)
                        epoll_event ev;
                        ev.data.fd = fd;
                        epoll_ctl(m_pfd, EPOLL_CTL_DEL, fd, &ev);
#endif
                        rc = true;
                        CloseSocket(fd);
                        break;
                    }
                    ++it;
                }
            }

            if (IsSelfSocket(fd))
                m_connected = false;
            return rc;
        }

        void AcceptSocket(SocketType fd)
        {
            SocketType nfd = 0;
            sockaddr_in caddr = { 0 };
            SocketLength addrlen = sizeof(caddr);
            std::map<SocketType, std::string> socks;
#if defined(WIN32)
            while ((nfd = ::accept(fd, (struct sockaddr*)&caddr, &addrlen)) != INVALID_SOCKET)
#else
            while ((nfd = ::accept(fd, (struct sockaddr*)&caddr, &addrlen)) > 0)
#endif
            {
                std::ostringstream os;
                os << inet_ntoa(caddr.sin_addr) << ":" << ntohs(caddr.sin_port);
                socks[nfd] = os.str();
            }

            std::map<SocketType, std::string>::const_iterator it = socks.begin();
            while (it != socks.end())
            {
                AddSocket(it->first, it->second);
                ++it;
            }
        }

        void AddSocket(SocketType fd, const std::string& ipport, bool createConn = true)
        {
            {
                KLockGuard<KMutex> lock(m_fdsMtx);
                if (!SetSocketNonBlock(fd))
                    return CloseSocket(fd);
#if defined(AIX)
                poll_ctl ev;
                ev.fd = fd;
                ev.events = POLLIN | POLLHUP | POLLERR;
                // PS_ADD PS_MOD PS_DELETE
                ev.cmd = PS_ADD;
                //int rc = pollset_ctl(pollset_t ps, struct poll_ctl* pollctl_array,int array_length)
                if (pollset_ctl(m_pfd, &ev, 1) < 0)
                    return CloseSocket(fd);
#elif defined(LINUX)
                epoll_event ev;
                ev.data.fd = fd;
                ev.events = EPOLLIN | EPOLLET | EPOLLERR | EPOLLHUP;
                if (epoll_ctl(m_pfd, EPOLL_CTL_ADD, fd, &ev) < 0)
                    return CloseSocket(fd);
#endif
                pollfd p;
                p.fd = fd;
#if defined(WIN32)
                p.events = epollin;
#else
                p.events = epollin | epollhup | epollerr;
#endif
                m_fds.push_back(p);
            }

            if (createConn)
                CreateConnection(fd, ipport);

            if(IsSelfSocket(fd))
                m_connected = true;
        }

        SocketType Connect(const std::string& ip, uint16_t port) const
        {
            int fd = -1;
            if ((fd = ::socket(AF_INET, SOCK_STREAM, 0)) < 0)
                return -1;

            DisableNagle(fd);

            sockaddr_in server;
            server.sin_family = AF_INET;
            server.sin_port = htons(port);
            server.sin_addr.s_addr = inet_addr(ip.c_str());
            if (::connect(fd, (sockaddr*)(&server), sizeof(server)) != 0)
            {
                CloseSocket(fd);
                return 0;
            }
            return fd;
        }

        SocketType Listen(const std::string& ip, uint16_t port) const
        {
            int fd = -1;
            if ((fd = ::socket(AF_INET, SOCK_STREAM, 0)) < 0)
                return -1;

            ReuseAddress(fd);
            DisableNagle(fd);

            sockaddr_in server;
            server.sin_family = AF_INET;
            server.sin_port = htons(port);
            server.sin_addr.s_addr = inet_addr(ip.c_str()); // htonl(INADDR_ANY);
            if (::bind(fd, (sockaddr*)(&server), sizeof(server)) != 0)
            {
                CloseSocket(fd);
                return 0;
            }

            if (::listen(fd, 200) != 0)
            {
                CloseSocket(fd);
                return -2;
            }
            return fd;
        }

        void CloseSocket(SocketType fd) const
        {
#if defined(WIN32)
            closesocket(fd);
#else
            ::close(fd);
#endif
        }

        bool SetSocketNonBlock(SocketType fd) const
        {
#ifdef WIN32
            // set non block
            u_long nonblock = 1;
            return ioctlsocket(fd, FIONBIO, &nonblock) == NO_ERROR;
#else
            // set non block
            int flags = fcntl(fd, F_GETFL, 0);
            if (flags < 0)
                return false;
            return fcntl(fd, F_SETFL, flags | O_NONBLOCK) >= 0;
#endif // WIN32
        }

        void ReuseAddress(SocketType fd) const
        {
            // set reuse address
            int on = 1;
            ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,
                reinterpret_cast<const char*>(&on), sizeof(on));
        }

        void DisableNagle(SocketType fd) const
        {
            // disable Nagle
            int on = 1;
            ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY,
                reinterpret_cast<const char*>(&on), sizeof(on));
        }

    private:
        template<typename T>
        friend class KTcpConnection;
#if defined(AIX)
        int m_pfd;
        pollfd m_ps[MaxEvent];
#elif defined(LINUX)
        int m_pfd;
        epoll_event m_ps[MaxEvent];
#endif
        
        KMutex m_fdsMtx;
        std::vector<pollfd> m_fds;
        
        SocketType m_fd;
        std::string m_ip;
        int32_t m_port;
        volatile bool m_isServer;
        volatile bool m_connected;
        volatile bool m_needAuth;
        uint16_t m_maxClient;
        KMutex m_connMtx;
        std::map<SocketType, KTcpConnection<MessageType>*> m_connections;
    };
};

#endif//_KTCPBASE_HPP_
