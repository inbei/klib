/*
tcp 连接操作类，包括连接绑定轮询读数据
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
        * Method:    构造函数
        * Returns:   
        *************************************/
        KTcpNetwork()
            :KEventObject<SocketType>("Poll thread", 50),m_connected(false), 
            m_isServer(false),m_needAuth(false),m_maxClient(100),m_ctx(NULL), m_sslEnabled(false), m_checkThread("Check alive thread")
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
        * Method:    析构函数
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
        * Method:    判断是否连接上
        * Returns:   连上返回true否则返回false
        *************************************/
        inline bool IsConnected() const { return m_connected; }

        /************************************
        * Method:    设置最大连接数
        * Returns:   
        * Parameter: mc 连接个数
        *************************************/
        inline void SetMaxClient(uint16_t mc) { m_maxClient = mc; }
        
        /************************************
        * Method:    启动
        * Returns:   成功返回true失败返回false
        * Parameter: ip 连接IP
        * Parameter: port 连接端口
        * Parameter: isServer 是否是服务器
        * Parameter: needAuth 是否需要授权
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
                try
                {
                    m_checkThread.Run(this, &KTcpNetwork::CheckAlive, 1);
                }
                catch (const std::exception& e)
                {
                    //printf("<%s> KTcpNetwork thread start failed, exception:[%s]\n", __FUNCTION__, e.what());
                    KEventObject<SocketType>::Stop();
                    KEventObject<SocketType>::WaitForStop();
#ifdef __OPEN_SSL__
            if (sslEnabled)
                KOpenSSL::DestroyCtx(&m_ctx);
#endif
                    return false;
                }

                PostForce(0);
                return true;
            }
#ifdef __OPEN_SSL__
            if (sslEnabled)
                KOpenSSL::DestroyCtx(&m_ctx);
#endif
            return false;
        }

        /************************************
        * Method:    停止
        * Returns:
        *************************************/
        virtual void Stop()
        {
            KEventObject<SocketType>::Stop();
        }

        /************************************
        * Method:    等待停止
        * Returns:
        *************************************/
        virtual void WaitForStop()
        {
            KEventObject<SocketType>::WaitForStop();
            m_checkThread.Join();
#ifdef __OPEN_SSL__
            KOpenSSL::DestroyCtx(&m_ctx);
#endif
        }

        /************************************
        * Method:    发送数据给自己
        * Returns:   发送成功返回true失败返回false
        * Parameter: bufs 待发送的数据
        *************************************/
        bool Send(const std::vector<KBuffer>& bufs)
        {
            if (m_isServer)
                return false;
            return SendClient(m_fd, SocketEvent::SeSent, bufs);
        }

        /************************************
        * Method:    发送数据给客户端
        * Returns:   发送成功返回true失败返回false
        * Parameter: fd 客户端ID
        * Parameter: et 事件类型
        * Parameter: bufs 发送的数据
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
        * Method:    获取自己的socket ID
        * Returns:   返回socket ID
        *************************************/
        inline SocketType GetSocket() const { return m_fd; }
        
        /************************************
        * Method:    获取连接IP
        * Returns:   返回IP
        * Parameter: fd 客户端ID
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
        * Method:    端口连接并清理资源
        * Returns:
        * Parameter: fd 客户端ID
        *************************************/
        void Disconnect(SocketType fd)
        {
            KLockGuard<KMutex> lock(m_connMtx);
            DeleteSocket(fd);
            typename std::map<SocketType, KTcpConnection<MessageType>*>::iterator it = m_connections.find(fd);
            if (it != m_connections.end())
            {
                KTcpConnection<MessageType>* c = it->second;
                std::string ip = c->GetIP();
                c->Disconnect(fd);
                std::map<std::string, int>::iterator pit = m_ipConnCount.find(ip);
                if (pit != m_ipConnCount.end() && --pit->second < 1)
                    m_ipConnCount.erase(pit);
            }
        }
		
		inline bool IsSslEnabled() const { return m_sslEnabled; }

    protected:        
        /************************************
        * Method:    创建连接
        * Returns:   返回连接对象
        * Parameter: fd 客户端ID
        * Parameter: ipport 客户端连接IP和端口
        *************************************/
        virtual KTcpConnection<MessageType>* NewConnection(SocketType fd, const std::string& ipport)
        {
            return new KTcpConnection<MessageType>(this);
        }

        /************************************
        * Method:    获取配置
        * Returns:   返回配置IP和端口
        *************************************/
        virtual std::pair<std::string, uint16_t> GetConfig() const {
            return std::pair<std::string, uint16_t>(m_ip, m_port);
        }

    private:
        /************************************
        * Method:    定时轮询或者重连
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

                        if (SetSocket(m_fd, false))
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
        * Method:    轮询socket ID
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
        * Method:    根据ID是否是自己
        * Returns:   
        * Parameter: fd
        *************************************/
        inline bool IsSelfSocket(SocketType fd) const { return m_fd == fd; }

        /************************************
        * Method:    处理socket 产生的event
        * Returns:   
        * Parameter: fd 客户端ID
        * Parameter: evt 事件
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
        * Method:    读socket
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
                rc = KTcpUtil::ReadSocket(fd, bufs);
            if (rc < 0)
                Disconnect(fd);

            if (!bufs.empty())
            {
                if (!SendClient(fd, SocketEvent::SeRecv, bufs))
                    KTcpUtil::Release(bufs);
            }
        }

        /************************************
        * Method:    删除socket 
        * Returns:   删除成功返回true否则返回false
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
        * Method:    接受连接
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
        * Method:    添加socket 到内存
        * Returns:   
        * Parameter: fd socket ID
        * Parameter: ipport IP和端口
        * Parameter: createConn 是否创建连接
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
					KTcpUtil::CloseSocket(fd);
					printf("CreateSSL failed\n");
					return false;
				}
            }
#endif
            if (recycle != m_connections.end())
            {
                KTcpConnection<MessageType>* c = recycle->second;
				c->SetSSL(ssl);
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
					c->SetSSL(ssl);
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
#ifdef __OPEN_SSL__
            			if (IsSslEnabled())
                			KOpenSSL::Disconnect(&ssl);
#endif
                        DeleteSocket(fd);
                        printf("New thread started failed\n");
                    }
                    
                    delete c;
                }
                else
                {
#ifdef __OPEN_SSL__
            		if (IsSslEnabled())
                		KOpenSSL::Disconnect(&ssl);
#endif
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

        bool SetSocket(SocketType fd, bool keep_alive = true)
        {
            if (keep_alive)
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

        SSL* GetSSL(SocketType fd)
        {
			KLockGuard<KMutex> lock(m_connMtx);
			typename std::map<SocketType, KTcpConnection<MessageType>*>::iterator it = m_connections.find(fd);
			if (it != m_connections.end())
			{
				KTcpConnection<MessageType>* c = it->second;
                return c->GetSSL();
			}
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
        // socket 互斥量 //
        KMutex m_fdsMtx;
        // socket 集合 //
        std::vector<pollfd> m_fds;
        // socket id //
        SocketType m_fd;
        // IP //
        std::string m_ip;
        // 端口 //
        int32_t m_port;
        // 是否是服务器 //
        volatile bool m_isServer;
        // 是否连接上 //
        volatile bool m_connected;
        // 是否需要授权 //
        volatile bool m_needAuth;
        // 最大连接个数 //
        uint16_t m_maxClient;
        // 连接对象互斥量 //
        KMutex m_connMtx;
        // 连接缓存 //
        std::map<SocketType, KTcpConnection<MessageType>*> m_connections;
        std::map<std::string, int> m_ipConnCount;
        KPthread m_checkThread;

        SSL_CTX* m_ctx;

        volatile bool m_sslEnabled;
    };
};

#endif//_KTCPBASE_HPP_
