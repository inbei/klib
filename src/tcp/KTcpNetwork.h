/*
tcp ���Ӳ����࣬�������Ӱ���ѯ������
*/
#ifndef _KTCPBASE_HPP_
#define _KTCPBASE_HPP_
#include "util/KStringUtility.h"
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
            m_isServer(false),m_needAuth(false),m_maxClient(100), m_checkThread("Check alive thread")
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
        virtual bool Start(const std::string& ip, int32_t port, bool isServer = true, bool needAuth = false)
        {
            m_ip = ip;
            m_port = port;
            m_isServer = isServer;
            m_needAuth = needAuth;
            if (KEventObject<SocketType>::Start())
            {
                try
                {
                    m_checkThread.Run(this, &KTcpNetwork::CheckAlive, 1);
                }
                catch (const std::exception& e)
                {
                    printf("<%s> KTcpNetwork thread start failed, exception:[%s]\n", __FUNCTION__, e.what());
                    KEventObject<SocketType>::Stop();
                    KEventObject<SocketType>::WaitForStop();
                    return false;
                }

                PostForce(0);
                return true;
            }
            return false;
        }

        /************************************
        * Method:    ֹͣ
        * Returns:
        *************************************/
        virtual void Stop()
        {
            KEventObject<SocketType>::Stop();
        }

        /************************************
        * Method:    �ȴ�ֹͣ
        * Returns:
        *************************************/
        virtual void WaitForStop()
        {
            KEventObject<SocketType>::WaitForStop();
            m_checkThread.Join();
        }

        /************************************
        * Method:    �������ݸ��Լ�
        * Returns:   ���ͳɹ�����trueʧ�ܷ���false
        * Parameter: bufs �����͵�����
        *************************************/
        bool Send(const std::vector<KBuffer>& bufs)
        {
            if (m_isServer)
                return false;
            return SendClient(m_fd, SocketEvent::SeSent, bufs);
        }

        /************************************
        * Method:    �������ݸ��ͻ���
        * Returns:   ���ͳɹ�����trueʧ�ܷ���false
        * Parameter: fd �ͻ���ID
        * Parameter: et �¼�����
        * Parameter: bufs ���͵�����
        *************************************/
        bool SendClient(SocketType fd, SocketEvent::EventType et,const std::vector<KBuffer>& bufs)
        {
            KLockGuard<KMutex> lock(m_connMtx);
            typename std::map<SocketType, KTcpConnection<MessageType>*>::iterator it = m_connections.find(fd);
            if (it != m_connections.end())
            {
                KTcpConnection<MessageType>* c = it->second;
                SocketEvent e(fd, et);
                e.binDat = bufs;
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

        /************************************
        * Method:    �˿����Ӳ�������Դ
        * Returns:
        * Parameter: fd �ͻ���ID
        *************************************/
        void Disconnect(SocketType fd)
        {
            KLockGuard<KMutex> lock(m_connMtx);
            typename std::map<SocketType, KTcpConnection<MessageType>*>::iterator it = m_connections.find(fd);
            if (it != m_connections.end())
            {
                KTcpConnection<MessageType>* c = it->second;
                std::string ip = c->GetIP();
                DeleteSocket(fd);
                c->Disconnect(fd);
                std::map<std::string, int>::iterator pit = m_ipConnCount.find(ip);
                if (pit != m_ipConnCount.end() && --pit->second < 1)
                    m_ipConnCount.erase(pit);
            }
        }

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

    private:
        /************************************
        * Method:    ��ʱ��ѯ��������
        * Returns:   
        * Parameter: ev
        *************************************/
        virtual void ProcessEvent(const SocketType& ev)
        {
            if (m_connected)
            {
                PollSocket();
            }
            else
            {
                if (m_isServer)
                {
                    std::pair<std::string, uint16_t> conf = GetConfig();
                    if ((m_fd = KTcpUtil::Listen(conf.first, conf.second)) > 0)
                    {
                        std::ostringstream os;
                        os << conf.first << ":" << conf.second;

                        if (SetSocket(m_fd))
                            m_connected = true;
                        else
                        {
                            KTcpUtil::CloseSocket(m_fd);
                            KTime::MSleep(1000);
                        }
                    }
                    else
                        KTime::MSleep(1000);
                }
                else
                {
                    std::pair<std::string, uint16_t> conf = GetConfig();
                    if ((m_fd = KTcpUtil::Connect(conf.first, conf.second)) > 0)
                    {
                        if(AddSocket(m_fd, conf.first, KStringUtility::Int32ToString(conf.second)))
                            m_connected = true;
                        else
                            KTime::MSleep(1000);
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
                if (m_isServer)
                {
                    if (IsSelfSocket(fd))
                        AcceptSocket(fd);
                    else
                        ReadSocket2(fd);
                }
                else
                {
                    ReadSocket2(fd);
                }
            }
            else if (evt & epollhup || evt & epollerr)
            {
                Disconnect(fd);
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
            if (KTcpUtil::ReadSocket(fd, bufs) < 0)
                Disconnect(fd);

            if (!bufs.empty())
            {
                if (!SendClient(fd, SocketEvent::SeRecv, bufs))
                    KTcpUtil::Release(bufs);
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
                        KTcpUtil::CloseSocket(fd);
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
#if defined(WIN32)
            while ((nfd = ::accept(fd, (struct sockaddr*)&caddr, &addrlen)) != INVALID_SOCKET)
#else
            while ((nfd = ::accept(fd, (struct sockaddr*)&caddr, &addrlen)) > 0)
#endif
            {
                AddSocket(nfd, inet_ntoa(caddr.sin_addr), KStringUtility::Int32ToString(ntohs(caddr.sin_port)));
            }
        }

        /************************************
        * Method:    ���socket ���ڴ�
        * Returns:   
        * Parameter: fd socket ID
        * Parameter: ipport IP�Ͷ˿�
        * Parameter: createConn �Ƿ񴴽�����
        *************************************/
        bool AddSocket(SocketType fd, const std::string& ip, const std::string &port)
        {
            KLockGuard<KMutex> lock(m_connMtx);
            typename std::map<SocketType, KTcpConnection<MessageType>*>::iterator recycle = m_connections.end();
            typename std::map<SocketType, KTcpConnection<MessageType>*>::iterator it = m_connections.begin();
            while (it != m_connections.end())
            {
                KTcpConnection<MessageType>* t = it->second;
                if (t->IsDisconnected())
                {
                    recycle = it;
                    break;
                }
                ++it;
            }

            if (recycle != m_connections.end())
            {
                KTcpConnection<MessageType>* c = recycle->second;
                c->Connect(ip, port, fd);
                if (SetSocket(fd))
                {
                    int& count = m_ipConnCount[ip];
                    if (count < 50)
                    {
                        ++count;
                        m_connections.erase(recycle);
                        m_connections[fd] = c;
                        printf("Recycle connection started success\n");
                        return true;
                    }
                    else
                    {
                        printf("Recycle one ip has more than 5 connections \n");
                    }
                }
                else
                {
                    printf("Recycle set socket failed\n");
                }
                c->Disconnect(fd);
            }
            else
            {
                if (m_connections.size() < m_maxClient)
                {
                    KTcpConnection<MessageType>* c = NewConnection(fd, ip + ":" + port);
                    if (c->Start(m_isServer ? NmServer : NmClient, ip, port, fd, m_needAuth))
                    {
                        if (SetSocket(fd))
                        {
                            int& count = m_ipConnCount[ip];
                            if (count < 5)
                            {
                                ++count;
                                m_connections[fd] = c;
                                printf("New connection started success\n");
                                return true;
                            }
                            else
                            {
                                printf("New one ip has more than 5 connections \n");
                            }
                        }
                        else
                        {
                            printf("New set socket failed\n");
                        }
                        c->Disconnect(fd);
                        c->Stop();
                        c->WaitForStop();
                    }
                    else
                    {
                        DeleteSocket(fd);
                        printf("New thread started failed\n");
                    }
                    
                    delete c;
                }
                else
                {
                    DeleteSocket(fd);
                    printf("New reach max client count:[%d]\n", m_maxClient);
                }
                
            }
            
            return false;
        }


        int CheckAlive(int)
        {
            while (IsRunning())
            {
                {
                    KLockGuard<KMutex> lock(m_connMtx);
                    typename std::map<SocketType, KTcpConnection<MessageType>*>::iterator it = m_connections.begin();
                    while (it != m_connections.end())
                    {
                        KTcpConnection<MessageType>* t = it->second;
                        if (t->IsConnected() && t->IsEmpty())
                        {
                            t->Post(SocketEvent(t->GetSocket(), SocketEvent::SeRecv));
                            //printf("check client alive, id:[%s]\n", t->GetAddress().c_str());
                        }
                        ++it;
                    }
                }

                KTime::MSleep(3000);
            }
            return 0;
        }

        bool SetSocket(SocketType fd)
        {
            KTcpUtil::SetKeepAlive(fd, 10, 3, 3);
            return KTcpUtil::SetSocketNonBlock(fd) && SetPollEvent(fd);
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
                KTcpUtil::CloseSocket(fd);
                return false;
            };
#elif defined(LINUX)
            epoll_event ev;
            ev.data.fd = fd;
            ev.events = EPOLLIN | EPOLLET | EPOLLERR | EPOLLHUP;
            if (epoll_ctl(m_pfd, EPOLL_CTL_ADD, fd, &ev) < 0)
            {
                KTcpUtil::CloseSocket(fd);
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
        std::map<std::string, int> m_ipConnCount;
        KPthread m_checkThread;
    };
};

#endif//_KTCPBASE_HPP_
