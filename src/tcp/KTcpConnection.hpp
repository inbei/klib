#pragma once
#if defined(WIN32)
#include <winsock2.h>
#include <WS2tcpip.h>
#elif defined(AIX)
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <sys/pollset.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#elif defined(HPUX)
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/mpctl.h>
#include <sys/poll.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#elif defined(LINUX)
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/poll.h>
#include <netinet/tcp.h>
#else
#error "WINDOWS AIX HPUX LINUX supported only"
#endif // defined(WIN32)

#include <cstdio>
#include <vector>
#include <string>
#include <assert.h>
#include "thread/KBuffer.h"
#include "thread/KAtomic.h"
#include "thread/KMutex.h"
#include "thread/KEventObject.h"
#include "util/KTime.h"

/**
tcp数据处理类
**/

namespace klib {

#ifndef SocketType
#if defined(WIN32)
#define SocketType SOCKET
#else
#define SocketType int
#endif
#endif

#if !defined(HPUX)
#define SocketLength socklen_t
#else
#define SocketLength int
#endif

#if defined(LINUX)
#define epollin EPOLLIN
#define epollout EPOLLOUT
#define epollhup EPOLLHUP
#define epollerr EPOLLERR
#else
#define epollin POLLIN
#define epollout POLLOUT
#define epollhup POLLHUP
#define epollerr POLLERR
#endif

#define PollTimeOut 500
#define BlockSize 40960
#define MaxEvent 40

    /**
    tcp 消息类
    **/
    class KTcpMessage// rewrite this class
    {
    public:
        /************************************
        * Method:    获取消息payload大小
        * Returns:   返回大小
        *************************************/
        virtual size_t GetPayloadSize() const { return 0; }
        /************************************
        * Method:    获取消息头大小
        * Returns:   返回大小
        *************************************/
        virtual size_t GetHeaderSize() const { return 0; }
        /************************************
        * Method:    判断消息是否有效
        * Returns:   有效返回true否则返回false
        *************************************/
        virtual bool IsValid() { return false; }
        /************************************
        * Method:    清理消息缓存
        * Returns:   
        *************************************/
        virtual void Clear() {  }
        /************************************
        * Method:    序列化消息
        * Returns:   
        * Parameter: result 序列化结果
        *************************************/
        virtual void Serialize(KBuffer& result) {}
    };

    // 协议错误、成功、头短、payload太短 //
    enum { ProtocolError = 0, ParseSuccess = 1, ShortHeader = 2, ShortPayload = 3 };

    /************************************
    * Method:    解析数据包
    * Returns:   解析返回协议错误、成功、头部太短和payload太短
    * Parameter: dat 数据包
    * Parameter: msg 消息
    * Parameter: left 数据包剩余数据
    *************************************/
    template<typename MessageType>
    int ParsePacket(const KBuffer& dat, MessageType& msg, KBuffer& left)
    {
        return ProtocolError;
    }

    /************************************
    * Method:    解析数据包
    * Returns:   
    * Parameter: dats 数据包
    * Parameter: msgs 消息
    * Parameter: remain 剩余数据
    * Parameter: autoRelease 自动释放
    *************************************/
    template<typename MessageType>
    void Parse(const std::vector<KBuffer>& dats, std::vector<MessageType>& msgs, KBuffer& remain, bool autoRelease = true)
    {
        std::vector<KBuffer>& bufs = const_cast<std::vector<KBuffer>&>(dats);
        if (remain.GetSize() > 0)
        {
            bufs.front().PrependBuffer(remain.GetData(), remain.GetSize());
            remain.Release();
        }
        std::vector<KBuffer>::iterator it = bufs.begin();
        while (it != bufs.end())
        {
            KBuffer dat = *it;
            KBuffer tail;
            MessageType msg;
            int rc = ParsePacket(dat, msg, tail);
            switch (rc)
            {
            case ProtocolError:
            {
                printf("protocol error, packet size:[%d]\n", it->GetSize());
                dat.Release();
                ++it;
                break;
            }
            case ParseSuccess:
            {
                msgs.push_back(msg);
                if (autoRelease)
                    dat.Release();
                if (tail.GetSize() > 0)
                    *it = tail;
                else
                    ++it;
                break;
            }
            case ShortHeader:
            {
                if (++it != bufs.end())
                {
                    it->PrependBuffer(dat.GetData(), dat.GetSize());
                    dat.Release();
                }
                else
                    remain = dat;
                break;
            }
            default:
            {
                size_t pl = msg.GetPayloadSize();
                std::vector<KBuffer>::iterator bit = it;
                size_t sz = 0;
                while (pl + msg.GetHeaderSize() > sz && it != bufs.end())
                {
                    sz += it->GetSize();
                    ++it;
                }

                if (it != bufs.end())
                    sz += it->GetSize();

                KBuffer tmp(sz);
                while (bit != it)
                {
                    tmp.ApendBuffer(bit->GetData(), bit->GetSize());
                    bit->Release();
                    ++bit;
                }

                if (it != bufs.end())
                {
                    tmp.ApendBuffer(it->GetData(), it->GetSize());
                    it->Release();
                    *it = tmp;
                }
                else
                {
                    if (pl + msg.GetHeaderSize() <= sz)
                    {
                        it = bufs.end() - 1;
                        *it = tmp;
                    }
                    else
                        remain = tmp;
                }
            }
            }
        }
    }

    /**
    授权信息
    **/
    struct Authorization
    {
        // 是否需要授权 // 
        bool need;
        // 发送授权是否成功 //
        bool authSent;
        // 接受授权是否成功 //
        bool authRecv;

        Authorization(bool need = false)
            :need(need), authSent(false), authRecv(false)
        {
        }

        inline void Reset() { authSent = false; authRecv = false; }
    };

    /**
    socket 事件
    **/
    struct SocketEvent
    {
        enum EventType
        {
            // 未知、接受数据、发送数据 //
            SeUndefined, SeRecv, SeSent
        };

        SocketEvent(SocketType f, EventType type)
            :fd(f), ev(type) {}

        SocketEvent()
            :fd(0), ev(SeUndefined) {}

        SocketType fd;
        EventType ev;
        // 二进制数据 //
        std::vector<KBuffer> binDat;
        // 字符串数据 //
        std::string strDat;
    };

    enum NetworkState
    {
        // 连接上，断开，就绪 //
        NsUndefined, NsPeerConnected, NsDisconnected, NsReadyToWork
    };

    enum NetworkMode
    {
        // 未知、客户端、服务端 //
        NmUndefined, NmClient, NmServer
    };

    class KTcpUtil
    {
    public:
        /************************************
        * Method:    写socket
        * Returns:
        * Parameter: fd socket
        * Parameter: dat 数据缓存
        * Parameter: sz 数据长度
        *************************************/
        static int WriteSocket(SocketType fd, const char* dat, size_t sz)
        {
            if (sz < 1 || dat == NULL || fd < 1)
                return 0;

            int sent = 0;
            while (sent != sz)
            {
                int rc = ::send(fd, dat + sent, sz - sent, 0/*MSG_DONTWAIT  MSG_WAITALL*/);
                if (rc > 0)
                    sent += rc;
                else if (rc == 0)
                    return -1;
                else
                {
#if defined(WIN32)
                    if (GetLastError() == WSAEINTR) // 写操作中断，需要重新读 // 
                        KTime::MSleep(3);
                    else if (GetLastError() == WSAEWOULDBLOCK) // 非阻塞模式，写缓冲已满，需要重新尝试写入 // 
                        KTime::MSleep(3);
#else
                    //printf("WriteSocket errno:[%d], errstr:[%s]\n", errno, strerror(errno));
                    if (errno == EINTR) // 写操作中断，需要重新读 // 
                        KTime::MSleep(3);
                    else if (errno == EWOULDBLOCK || errno == EAGAIN) // 非阻塞模式，写缓冲已满，需要重新尝试写入，hpux写缓冲已满时返回EAGAIN // 
                        KTime::MSleep(1);
#endif
                    else // 错误断开连接 // 
                        return -1;
                }
            }
            return sent;
        }

        /************************************
        * Method:    读socket
        * Returns:   返回读取字节数
        * Parameter: fd socket
        * Parameter: dat 数据
        *************************************/
        static int ReadSocket(SocketType fd, std::vector<KBuffer>& dat)
        {
            if (fd < 1)
                return 0;

            int bytes = 0;
            char buf[BlockSize] = { 0 };
            while (true)
            {
                int rc = ::recv(fd, buf, BlockSize, 0/*MSG_DONTWAIT  MSG_WAITALL*/);
                if (rc > 0)
                {
                    KBuffer b(rc);
                    b.ApendBuffer(buf, rc);
                    dat.push_back(b);
                    bytes += rc;
                }
                else if (rc == 0)
                    return -1;
                else
                {
#if defined(WIN32)
                    if (GetLastError() == WSAEINTR) // 读操作中断，需要重新读 // 
                        KTime::MSleep(3);
                    else if (GetLastError() == WSAEWOULDBLOCK) // 非阻塞模式，暂时无数据，不需要重新读 // 
                        break;
#else
                    //printf("ReadSocket errno:[%d], errstr:[%s]\n", errno, strerror(errno));
                    if (errno == EINTR) // 读操作中断，需要重新读 // 
                        KTime::MSleep(3);
                    else if (errno == EWOULDBLOCK || errno == EAGAIN) // 非阻塞模式，暂时无数据，不需要重新读 // 
                        break;
#endif
                    else // 错误断开连接 // 
                        return -1;
                }
            }
            return bytes;
        }


        /************************************
        * Method:    释放内存
        * Returns:
        * Parameter: bufs 待释放的内存
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
        * Method:    连接服务器
        * Returns:   返回socket ID
        * Parameter: ip 服务器IP
        * Parameter: port 服务器端口
        *************************************/
        static SocketType Connect(const std::string& ip, uint16_t port)
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
        * Method:    监听IP和port端口
        * Returns:   返回socket ID
        * Parameter: ip 待监听的IP
        * Parameter: port 待监听的端口
        *************************************/
        static SocketType Listen(const std::string& ip, uint16_t port)
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
        * Method:    关闭socket
        * Returns:
        * Parameter: fd socket ID
        *************************************/
        static void CloseSocket(SocketType fd)
        {
#if defined(WIN32)
            closesocket(fd);
#else
            ::close(fd);
#endif
        }

        /************************************
        * Method:    socket 设置为非阻塞模式
        * Returns:
        * Parameter: fd socket ID
        *************************************/
        static bool SetSocketNonBlock(SocketType fd)
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
        * Method:    socket 设置reuse属性
        * Returns:
        * Parameter: fd socket ID
        *************************************/
        static void ReuseAddress(SocketType fd)
        {
            // set reuse address
            int on = 1;
            ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,
                reinterpret_cast<const char*>(&on), sizeof(on));
        }

        /************************************
        * Method:    禁用nagle算法
        * Returns:
        * Parameter: fd socket id
        *************************************/
        static void DisableNagle(SocketType fd)
        {
            // disable Nagle
            int on = 1;
            ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY,
                reinterpret_cast<const char*>(&on), sizeof(on));
        }

        /*************************************
        * Method:    Setting SO_TCP KEEPALIVE
        * Returns:
        * Parameter: keep_idle 开始首次KeepAlive探测前的TCP空闭时间
        * Parameter: keep_interval 两次KeepAlive探测间的时间间隔
        * Parameter: keep_count 判定断开前的KeepAlive探测次数
        *************************************/
        static void SetKeepAlive(int fd, int keep_idle, int keep_interval, int keep_count)
        {
            int keep_alive = 1;
            {
#ifdef WIN32
#define SIO_KEEPALIVE_VALS _WSAIOW(IOC_VENDOR,4)
                struct tcp_keepalive {

                    u_long on;
                    u_long threshold;
                    u_long interval;

                } keepIn, keepOut;

                keepIn.interval = keep_interval * 1000;// 10s 每10S发送1包探测报文，发5次没有回应，就断开 // 
                keepIn.threshold = keep_count * keep_interval * 1000;// 60s 超过60S没有数据，就发送探测包 // 
                keepIn.on = keep_alive;

                u_long ulBytesReturn = 0;
                if (WSAIoctl(fd, SIO_KEEPALIVE_VALS, (LPVOID)&keepIn, sizeof(keepIn), (LPVOID)&keepOut, sizeof(keepOut), &ulBytesReturn, NULL, NULL) == SOCKET_ERROR)
                    printf("WSAIoctl error code:[%d]\n", WSAGetLastError());
#else
                if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (void*)&keep_alive, sizeof(keep_alive)) == -1)
                    printf("setsockopt SO_KEEPALIVE error:[%s]\n", strerror(errno));

                if (setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, (void*)&keep_idle, sizeof(keep_idle)) == -1)
                    printf("setsockopt TCP_KEEPIDLE error:[%s]\n", strerror(errno));

                if (setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, (void*)&keep_interval, sizeof(keep_interval)) == -1)
                    printf("setsockopt TCP_KEEPINTVL error:[%s]\n", strerror(errno));

                if (setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, (void*)&keep_count, sizeof(keep_count)) == -1)
                    printf("setsockopt TCP_KEEPCNT error:[%s]\n", strerror(errno));
#endif
            }
        }
    };

    

    template<typename MessageType>
    class KTcpNetwork;

    template<typename MessageType>
    class KTcpConnection:public KEventObject<SocketEvent>
    {
    public:
        KTcpConnection(KTcpNetwork<MessageType> *poller)
            :KEventObject<SocketEvent>("Socket event thread", 1000),
            m_state(NsUndefined), m_mode(NmUndefined), m_poller(poller), m_fd(0)
        {

        }

        ~KTcpConnection()
        {
            
        }

        /************************************
        * Method:    启动
        * Returns:   成功返回true失败false
        * Parameter: mode 模式
        * Parameter: ipport IP和端口
        * Parameter: fd socket
        * Parameter: needAuth 是否需要授权
        *************************************/
        bool Start(NetworkMode mode, const std::string& ip, const std::string& port, SocketType fd, bool needAuth = false)
        {
            m_mode = mode;
            m_auth.need = needAuth;
            if (KEventObject<SocketEvent>::Start())
            {
                Connect(ip, port, fd);
                return true;
            }
            return false;
        }
        

        /************************************
        * Method:    是否连接上
        * Returns:   是返回true否则返回false
        *************************************/
        inline bool IsConnected() const
        {
            switch (GetState())
            {
            case NsPeerConnected:
            case NsReadyToWork:
                return true;
            default:
                return false;
            }
        }

        /************************************
        * Method:    是否断开
        * Returns:   是返回true否则返回false
        *************************************/
        inline bool IsDisconnected() const { return m_state == NsDisconnected; }

        /************************************
        * Method:    获取socket
        * Returns:   socket
        *************************************/
        inline SocketType GetSocket() const { return m_fd; }

        /************************************
        * Method:    获取IP和端口
        * Returns:   返回IP和端口
        *************************************/
        inline  const std::string& GetAddress() const { return m_ipport; }

        inline const std::string& GetIP() const { return m_ip; }
        
    protected:
        /************************************
        * Method:    获取模式
        * Returns:   返回模式
        *************************************/
        inline NetworkMode GetMode() const { return static_cast<NetworkMode>(int32_t(m_mode)); }
        /************************************
        * Method:    获取状态
        * Returns:   返回状态
        *************************************/
        inline NetworkState GetState() const { return static_cast<NetworkState>(int32_t(m_state)); }
        /************************************
        * Method:    设置状态
        * Returns:   
        * Parameter: s 状态
        *************************************/
        inline void SetState(NetworkState s) const { m_state = s; }
        /************************************
        * Method:    连接触发操作
        * Returns:   
        * Parameter: mode 模式
        * Parameter: ipport IP和端口
        *************************************/
        virtual void OnConnected(NetworkMode mode, const std::string& ipport, SocketType fd)
        {
            printf("%s connected\n", ipport.c_str());
        }
        /************************************
        * Method:    断开触发操作
        * Returns:   
        * Parameter: mode 模式
        * Parameter: ipport IP和端口
        * Parameter: fd socket
        *************************************/
        virtual void OnDisconnected(NetworkMode mode, const std::string& ipport, SocketType fd)
        {
            printf("%s disconnected\n", ipport.c_str());
        }
        /************************************
        * Method:    新消息触发操作
        * Returns:   
        * Parameter: msgs 新消息
        *************************************/
        virtual void OnMessage(const std::vector<MessageType>& msgs)
        {
            printf("%s recv struct message, count:[%d]\n", msgs.size());
        }
        /************************************
        * Method:    二进制数据触发操作
        * Returns:   
        * Parameter: ev 数据
        *************************************/
        virtual void OnMessage(const std::vector<KBuffer>& ev)
        {
            printf("%s recv raw message, count:[%d]\n", ev.size());
        }
        /************************************
        * Method:    触发授权请求
        * Returns:   授权成功返回true否则返回false
        *************************************/
        virtual bool OnAuthRequest() const
        {
            return !m_auth.need;
        }
        /************************************
        * Method:    触发授权响应
        * Returns:   授权成功返回true否则返回false
        * Parameter: ev 授权请求数据
        *************************************/
        virtual bool OnAuthResponse(const std::vector<KBuffer>& ev) const
        {
            return !m_auth.need;
        }

    private:
        /************************************
        * Method:    处理事件
        * Returns:   
        * Parameter: ev 事件
        *************************************/
        virtual void ProcessEvent(const SocketEvent& ev)
        {
            std::vector<KBuffer>& bufs = const_cast<std::vector<KBuffer> &>(ev.binDat);
            if (IsConnected())
            {
                SocketType fd = ev.fd;
                switch (ev.ev)
                {
                case SocketEvent::SeSent:
                {
                    if (m_auth.need && !m_auth.authSent)
                    {
                        m_auth.authSent = OnAuthRequest();
                    }
                    else
                    {
                        const std::string& smsg = ev.strDat;
                        if (!smsg.empty())
                        {
                            int rc = KTcpUtil::WriteSocket(fd, smsg.c_str(), smsg.size());
                            if (rc < 0)
                            {
                                m_poller->Disconnect(fd);
                                KTcpUtil::Release(bufs);
                                break;
                            }
                        }

                        if (!bufs.empty())
                        {
                            std::vector<KBuffer >::iterator it = bufs.begin();
                            while (it != bufs.end())
                            {
                                KBuffer& buf = *it;
                                if (KTcpUtil::WriteSocket(fd, buf.GetData(), buf.GetSize()) < 0)
                                {
                                    m_poller->Disconnect(fd);
                                    break;
                                }
                                ++it;
                            }
                        }
                    }
                    KTcpUtil::Release(bufs);
                    break;
                }
                case SocketEvent::SeRecv:
                {
                    if (bufs.empty())
                    {
                        std::vector<KBuffer> buffers;
                        int rc = KTcpUtil::ReadSocket(fd, buffers);
                        if (rc < 0)
                            m_poller->Disconnect(fd);

                        if(!buffers.empty())
                            ParseData(fd, buffers);
                    }
                    else
                    {
                        ParseData(fd, bufs);
                    }
                    break;
                }
                default:
                    break;
                }
            }
            else
            {
                KTcpUtil::Release(bufs);
            }
        }
        /************************************
        * Method:    解析数据
        * Returns:   
        * Parameter: fd socket
        * Parameter: bufs 数据
        *************************************/
        void ParseData(SocketType fd, std::vector<KBuffer>& bufs)
        {
            if (m_auth.need && !m_auth.authRecv)
            {
                if (!(m_auth.authRecv = OnAuthResponse(bufs)))
                    m_poller->Disconnect(fd);
            }
            else
            {
                MessageType t;
                if (t.GetHeaderSize() > 0)
                {
                    std::vector<MessageType> msgs;
                    Parse(bufs, msgs, m_remain);
                    if (!msgs.empty())
                        OnMessage(msgs);
                }
                else
                {
                    OnMessage(bufs);
                }
            }
        }

        /************************************
        * Method:    连接
        * Returns:
        * Parameter: ipport IP和端口
        * Parameter: fd socket
        *************************************/
        void Connect(const std::string& ip, const std::string& port, SocketType fd)
        {
            if (GetMode() == NmServer)
                m_auth.authSent = true;
            m_ip = ip;
            m_port = port;
            m_ipport = ip + ":" + port;
            m_fd = fd;
            SetState(NsPeerConnected);

            OnConnected(GetMode(), m_ipport, fd);
        }

        /************************************
        * Method:    断开连接
        * Returns:
        * Parameter: fd socket
        *************************************/
        void Disconnect(SocketType fd)
        {
            m_auth.Reset();
            m_remain.Release();

            m_ip.clear();
            m_port.clear();
            std::string ipport = m_ipport;
            m_ipport.clear();
            m_fd = 0;

            std::vector<SocketEvent> events;
            Flush(events);
            std::vector<SocketEvent>::iterator it = events.begin();
            while (it != events.end())
            {
                KTcpUtil::Release(it->binDat);
                ++it;
            }
            SetState(NsDisconnected);

            OnDisconnected(GetMode(), ipport, fd);
        }

    protected:
        template<typename T>
        friend class KTcpNetwork;

        // 连接 //
        KTcpNetwork<MessageType>* m_poller;

    private:
        // IP端口 //
        std::string m_ip;
        std::string m_port;
        std::string m_ipport;
        // socket //
        volatile SocketType m_fd;
        // 模式 //
        AtomicInteger<int32_t> m_mode;
        // 剩余数据 //
        KBuffer m_remain;
        // 状态 //
        mutable AtomicInteger<int32_t> m_state;
        // 授权 //
        Authorization m_auth;
        
    };
};
