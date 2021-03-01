/*
tcp ���Ӳ����࣬�������Ӱ���ѯ������
*/
#ifndef _KTCPBASE_HPP_
#define _KTCPBASE_HPP_

#include "KTcpConnection.hpp"
namespace klib {
    template<typename MessageType>
    class KTcpNetwork: public KEventObject<SocketType>
    {
    public:
        /************************************
        * Method:    ���캯��
        * Returns:   
        *************************************/
        KTcpNetwork()
            :KEventObject<SocketType>("Poll thread", 50),m_connected(false), 
            m_isServer(false),m_needAuth(false),m_maxClient(50),m_ctx(NULL), m_sslEnabled(false)
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

        /************************************
        * Method:    ��������
        * Returns:   
        *************************************/
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

        /************************************
        * Method:    �ͷ��ڴ�
        * Returns:   
        * Parameter: bufs ���ͷŵ��ڴ�
        *************************************/
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

        /************************************
        * Method:    �ж��Ƿ�������
        * Returns:   ���Ϸ���true���򷵻�false
        *************************************/
        inline bool IsConnected() const { return m_connected; }

        /************************************
        * Method:    �������������
        * Returns:   
        * Parameter: mc ���Ӹ���
        *************************************/
        inline void SetMaxClient(uint16_t mc) { m_maxClient = mc; }
        
        /************************************
        * Method:    ����
        * Returns:   �ɹ�����trueʧ�ܷ���false
        * Parameter: ip ����IP
        * Parameter: port ���Ӷ˿�
        * Parameter: isServer �Ƿ��Ƿ�����
        * Parameter: needAuth �Ƿ���Ҫ��Ȩ
        *************************************/
        virtual bool Start(const std::string& ip, int32_t port, const KOpenSSLConfig &conf, bool isServer = true, bool needAuth = false, bool sslEnabled = false)
        {
#ifdef __OPEN_SSL__
            if (sslEnabled && !KOpenSSL::CreateCtx(isServer, conf, &m_ctx))
            {
                KOpenSSL::DestroyCtx(&m_ctx);
                return false;
            }
#endif
            m_ip = ip;
            m_port = port;
            m_isServer = isServer;
            m_needAuth = needAuth;
#ifdef __OPEN_SSL__
            m_sslEnabled = sslEnabled;
#endif
            if (KEventObject<SocketType>::Start())
            {
                PostForce(0);
                return true;
            }
#ifdef __OPEN_SSL__
            if (sslEnabled)
                KOpenSSL::DestroyCtx(&m_ctx);
#endif
            return false;
        }

        virtual void WaitForStop()
        {
            KEventObject<SocketType>::WaitForStop();
#ifdef __OPEN_SSL__
            KOpenSSL::DestroyCtx(&m_ctx);
#endif
        }

        /************************************
        * Method:    �������ݸ��Լ�
        * Returns:   ���ͳɹ�����trueʧ�ܷ���false
        * Parameter: bufs �����͵�����
        *************************************/
        bool SendDataToSelf(const std::vector<KBuffer>& bufs)
        {
            if (m_isServer)
                return false;
            return SendDataToConnection(m_fd, SocketEvent::SeSent, bufs);
        }

        /************************************
        * Method:    �������ݸ��ͻ���
        * Returns:   ���ͳɹ�����trueʧ�ܷ���false
        * Parameter: fd �ͻ���ID
        * Parameter: et �¼�����
        * Parameter: bufs ���͵�����
        *************************************/
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
                e.ssl = c->GetSSL();
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

        /************************************
        * Method:    ��ȡ�Լ���socket ID
        * Returns:   ����socket ID
        *************************************/
        inline SocketType GetSocket() const { return m_fd; }
        
        /************************************
        * Method:    ��ȡ����IP
        * Returns:   ����IP
        * Parameter: fd �ͻ���ID
        *************************************/
        const std::string& GetConnectionInfo(SocketType fd)
        {
            KLockGuard<KMutex> lock(m_connMtx);
            typename std::map<SocketType, KTcpConnection<MessageType>*>::iterator it = m_connections.find(fd);
            if (it != m_connections.end())
                return it->second->GetAddress();
            return std::string();
        }

        inline bool IsSslEnabled() const { return m_sslEnabled; }
    protected:        
        /************************************
        * Method:    ��������
        * Returns:   �������Ӷ���
        * Parameter: fd �ͻ���ID
        * Parameter: ipport �ͻ�������IP�Ͷ˿�
        *************************************/
        virtual KTcpConnection<MessageType>* NewConnection(SocketType fd, const std::string& ipport)
        {
            return new KTcpConnection<MessageType>(this);
        }

        /************************************
        * Method:    ��ȡ����
        * Returns:   ��������IP�Ͷ˿�
        *************************************/
        virtual std::pair<std::string, uint16_t> GetConfig() const {
            return std::pair<std::string, uint16_t>(m_ip, m_port);
        }

        /************************************
        * Method:    �˿����Ӳ�������Դ
        * Returns:   
        * Parameter: fd �ͻ���ID
        *************************************/
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
        /************************************
        * Method:    ��ʱ��ѯ��������
        * Returns:   
        * Parameter: ev
        *************************************/
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

                        if (SetSocketNonBlock(m_fd) && SetPollEvent(m_fd))
                            m_connected = true;
                        else
                            CloseSocket(m_fd);
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
        
        /************************************
        * Method:    ��ѯsocket ID
        * Returns:   
        *************************************/
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

        /************************************
        * Method:    ����ID�Ƿ����Լ�
        * Returns:   
        * Parameter: fd
        *************************************/
        inline bool IsSelfSocket(SocketType fd) const { return m_fd == fd; }

        /************************************
        * Method:    ����socket ������event
        * Returns:   
        * Parameter: fd �ͻ���ID
        * Parameter: evt �¼�
        *************************************/
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

        /************************************
        * Method:    ��socket
        * Returns:   
        * Parameter: fd socket ID
        *************************************/
        void ReadSocket2(SocketType fd)
        {
            std::vector<KBuffer> bufs;
            int rc = 0;
#ifdef __OPEN_SSL__
            if (IsSslEnabled())
                rc = KOpenSSL::ReadSocket(GetSSL(fd), bufs);
            else
#endif
                rc = ReadSocket(fd, bufs);
            if (rc < 0)
                DisconnectConnection(fd);

            if (!bufs.empty())
            {
                if (!SendDataToConnection(fd, SocketEvent::SeRecv, bufs))
                    Release(bufs);
            }
        }

        /************************************
        * Method:    ɾ��socket 
        * Returns:   ɾ���ɹ�����true���򷵻�false
        * Parameter: fd socket ID
        *************************************/
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

        /************************************
        * Method:    ��������
        * Returns:   
        * Parameter: fd socket ID
        *************************************/
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

        /************************************
        * Method:    ���socket ���ڴ�
        * Returns:   
        * Parameter: fd socket ID
        * Parameter: ipport IP�Ͷ˿�
        * Parameter: createConn �Ƿ񴴽�����
        *************************************/
        void AddSocket(SocketType fd, const std::string& ipport)
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

            if (!SetSocketNonBlock(fd))
            {
                CloseSocket(fd);
                return;
            }

            SSL* ssl = NULL;
#ifdef __OPEN_SSL__
            if (IsSslEnabled())
            {
				if (m_isServer)
					ssl = KOpenSSL::Accept(fd, m_ctx);
				else
					ssl = KOpenSSL::Connect(fd, m_ctx);

				if (ssl == NULL)
				{
					CloseSocket(fd);
					printf("CreateSSL failed\n");
					return;
				}
            }
#endif	
            if (!SetPollEvent(fd))
            {
                CloseSocket(fd);
                return;
            }

            if (recycle)
            {
                recycle->SetSSL(ssl);
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
                        c->SetSSL(ssl);
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

            if(IsSelfSocket(fd))
                m_connected = true;
        }

        bool SetPollEvent(SocketType fd)
        {
            KLockGuard<KMutex> lock(m_fdsMtx);
#if defined(AIX)
            poll_ctl ev;
            ev.fd = fd;
            ev.events = POLLIN | POLLHUP | POLLERR;
            // PS_ADD PS_MOD PS_DELETE
            ev.cmd = PS_ADD;
            //int rc = pollset_ctl(pollset_t ps, struct poll_ctl* pollctl_array,int array_length)
            if (pollset_ctl(m_pfd, &ev, 1) < 0)
            {
                CloseSocket(fd);
                return false;
            };
#elif defined(LINUX)
            epoll_event ev;
            ev.data.fd = fd;
            ev.events = EPOLLIN | EPOLLET | EPOLLERR | EPOLLHUP;
            if (epoll_ctl(m_pfd, EPOLL_CTL_ADD, fd, &ev) < 0)
            {
                CloseSocket(fd);
                return false;
            };
#endif
            pollfd p;
            p.fd = fd;
#if defined(WIN32)
            p.events = epollin;
#else
            p.events = epollin | epollhup | epollerr;
#endif
            m_fds.push_back(p);
            return true;
        };

        /************************************
        * Method:    ���ӷ�����
        * Returns:   ����socket ID
        * Parameter: ip ������IP
        * Parameter: port �������˿�
        *************************************/
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

        /************************************
        * Method:    ����IP��port�˿�
        * Returns:   ����socket ID
        * Parameter: ip ��������IP
        * Parameter: port �������Ķ˿�
        *************************************/
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

        /************************************
        * Method:    �ر�socket
        * Returns:   
        * Parameter: fd socket ID
        *************************************/
        void CloseSocket(SocketType fd) const
        {
#if defined(WIN32)
            closesocket(fd);
#else
            ::close(fd);
#endif
        }

        /************************************
        * Method:    socket ����Ϊ������ģʽ
        * Returns:   
        * Parameter: fd socket ID
        *************************************/
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

        /************************************
        * Method:    socket ����reuse����
        * Returns:   
        * Parameter: fd socket ID
        *************************************/
        void ReuseAddress(SocketType fd) const
        {
            // set reuse address
            int on = 1;
            ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,
                reinterpret_cast<const char*>(&on), sizeof(on));
        }

        /************************************
        * Method:    ����nagle�㷨
        * Returns:   
        * Parameter: fd socket id
        *************************************/
        void DisableNagle(SocketType fd) const
        {
            // disable Nagle
            int on = 1;
            ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY,
                reinterpret_cast<const char*>(&on), sizeof(on));
        }

        bool IsExist(SocketType fd) const
        {
            KLockGuard<KMutex> lock(m_connMtx);
            typename std::map<SocketType, KTcpConnection<MessageType>*>::const_iterator it = m_connections.find(fd);
            return (it != m_connections.end());
        }

        SSL* GetSSL(SocketType fd) const
        {
            KLockGuard<KMutex> lock(m_connMtx);
            typename std::map<SocketType, KTcpConnection<MessageType>*>::const_iterator it = m_connections.find(fd);
            if (it != m_connections.end())
                return it->second->GetSSL();
            return NULL;
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
        // socket ������ //
        KMutex m_fdsMtx;
        // socket ���� //
        std::vector<pollfd> m_fds;
        // socket id //
        SocketType m_fd;
        // IP //
        std::string m_ip;
        // �˿� //
        int32_t m_port;
        // �Ƿ��Ƿ����� //
        volatile bool m_isServer;
        // �Ƿ������� //
        volatile bool m_connected;
        // �Ƿ���Ҫ��Ȩ //
        volatile bool m_needAuth;
        // ������Ӹ��� //
        uint16_t m_maxClient;
        // ���Ӷ��󻥳��� //
        KMutex m_connMtx;
        // ���ӻ��� //
        std::map<SocketType, KTcpConnection<MessageType>*> m_connections;

        SSL_CTX* m_ctx;

        volatile bool m_sslEnabled;
    };
};

#endif//_KTCPBASE_HPP_
