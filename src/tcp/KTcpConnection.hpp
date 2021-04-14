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
#elif defined(HPUX)
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/mpctl.h>
#include <sys/poll.h>
#include <netinet/tcp.h>
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
tcp���ݴ�����
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
    tcp ��Ϣ��
    **/
    class KTcpMessage// rewrite this class
    {
    public:
        /************************************
        * Method:    ��ȡ��Ϣpayload��С
        * Returns:   ���ش�С
        *************************************/
        virtual size_t GetPayloadSize() const { return 0; }
        /************************************
        * Method:    ��ȡ��Ϣͷ��С
        * Returns:   ���ش�С
        *************************************/
        virtual size_t GetHeaderSize() const { return 0; }
        /************************************
        * Method:    �ж���Ϣ�Ƿ���Ч
        * Returns:   ��Ч����true���򷵻�false
        *************************************/
        virtual bool IsValid() { return false; }
        /************************************
        * Method:    ������Ϣ����
        * Returns:   
        *************************************/
        virtual void Clear() {  }
        /************************************
        * Method:    ���л���Ϣ
        * Returns:   
        * Parameter: result ���л����
        *************************************/
        virtual void Serialize(KBuffer& result) {}
    };

    // Э����󡢳ɹ���ͷ�̡�payload̫�� //
    enum { ProtocolError = 0, ParseSuccess = 1, ShortHeader = 2, ShortPayload = 3 };

    /************************************
    * Method:    �������ݰ�
    * Returns:   ��������Э����󡢳ɹ���ͷ��̫�̺�payload̫��
    * Parameter: dat ���ݰ�
    * Parameter: msg ��Ϣ
    * Parameter: left ���ݰ�ʣ������
    *************************************/
    template<typename MessageType>
    int ParsePacket(const KBuffer& dat, MessageType& msg, KBuffer& left)
    {
        return ProtocolError;
    }

    /************************************
    * Method:    �������ݰ�
    * Returns:   
    * Parameter: dats ���ݰ�
    * Parameter: msgs ��Ϣ
    * Parameter: remain ʣ������
    * Parameter: autoRelease �Զ��ͷ�
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
    ��Ȩ��Ϣ
    **/
    struct Authorization
    {
        // �Ƿ���Ҫ��Ȩ // 
        bool need;
        // ������Ȩ�Ƿ�ɹ� //
        bool authSent;
        // ������Ȩ�Ƿ�ɹ� //
        bool authRecv;

        Authorization(bool need = false)
            :need(need), authSent(false), authRecv(false)
        {
        }

        inline void Reset() { authSent = false; authRecv = false; }
    };

    /**
    socket �¼�
    **/
    struct SocketEvent
    {
        enum EventType
        {
            // δ֪���������ݡ��������� //
            SeUndefined, SeRecv, SeSent
        };

        SocketEvent(SocketType f, EventType type)
            :fd(f), ev(type) {}

        SocketEvent()
            :fd(0), ev(SeUndefined) {}

        SocketType fd;
        EventType ev;
        // ���������� //
        std::vector<KBuffer> binDat;
        // �ַ������� //
        std::string strDat;
    };

    enum NetworkState
    {
        // �����ϣ��Ͽ������� //
        NsUndefined, NsPeerConnected, NsDisconnected, NsReadyToWork
    };

    enum NetworkMode
    {
        // δ֪���ͻ��ˡ������ //
        NmUndefined, NmClient, NmServer
    };

    class KTcpUtil
    {
    public:
        /************************************
        * Method:    дsocket
        * Returns:
        * Parameter: fd socket
        * Parameter: dat ���ݻ���
        * Parameter: sz ���ݳ���
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
                    if (GetLastError() == WSAEINTR) // д�����жϣ���Ҫ���¶� // 
                        KTime::MSleep(3);
                    else if (GetLastError() == WSAEWOULDBLOCK) // ������ģʽ��д������������Ҫ���³���д�� // 
                        KTime::MSleep(3);
#else
                    //printf("WriteSocket errno:[%d], errstr:[%s]\n", errno, strerror(errno));
                    if (errno == EINTR) // д�����жϣ���Ҫ���¶� // 
                        KTime::MSleep(3);
                    else if (errno == EWOULDBLOCK || errno == EAGAIN) // ������ģʽ��д������������Ҫ���³���д�룬hpuxд��������ʱ����EAGAIN // 
                        KTime::MSleep(1);
#endif
                    else // ����Ͽ����� // 
                        return -1;
                }
            }
            return sent;
        }

        /************************************
        * Method:    ��socket
        * Returns:   ���ض�ȡ�ֽ���
        * Parameter: fd socket
        * Parameter: dat ����
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
                    if (GetLastError() == WSAEINTR) // �������жϣ���Ҫ���¶� // 
                        KTime::MSleep(3);
                    else if (GetLastError() == WSAEWOULDBLOCK) // ������ģʽ����ʱ�����ݣ�����Ҫ���¶� // 
                        break;
#else
                    //printf("ReadSocket errno:[%d], errstr:[%s]\n", errno, strerror(errno));
                    if (errno == EINTR) // �������жϣ���Ҫ���¶� // 
                        KTime::MSleep(3);
                    else if (errno == EWOULDBLOCK || errno == EAGAIN) // ������ģʽ����ʱ�����ݣ�����Ҫ���¶� // 
                        break;
#endif
                    else // ����Ͽ����� // 
                        return -1;
                }
            }
            return bytes;
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
        * Method:    ���ӷ�����
        * Returns:   ����socket ID
        * Parameter: ip ������IP
        * Parameter: port �������˿�
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
        * Method:    ����IP��port�˿�
        * Returns:   ����socket ID
        * Parameter: ip ��������IP
        * Parameter: port �������Ķ˿�
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
        * Method:    �ر�socket
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
        * Method:    socket ����Ϊ������ģʽ
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
        * Method:    socket ����reuse����
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
        * Method:    ����nagle�㷨
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
        * Parameter: keep_idle ��ʼ�״�KeepAlive̽��ǰ��TCP�ձ�ʱ��
        * Parameter: keep_interval ����KeepAlive̽����ʱ����
        * Parameter: keep_count �ж��Ͽ�ǰ��KeepAlive̽�����
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

                keepIn.interval = keep_interval * 1000;// 10s ÿ10S����1��̽�ⱨ�ģ���5��û�л�Ӧ���ͶϿ� // 
                keepIn.threshold = keep_count * keep_interval * 1000;// 60s ����60Sû�����ݣ��ͷ���̽��� // 
                keepIn.on = keep_alive;

                u_long ulBytesReturn = 0;
                if (WSAIoctl(fd, SIO_KEEPALIVE_VALS, (LPVOID)&keepIn, sizeof(keepIn), (LPVOID)&keepOut, sizeof(keepOut), &ulBytesReturn, NULL, NULL) == SOCKET_ERROR)
                    printf("WSAIoctl error code:[%d]\n", WSAGetLastError());
#else
                if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (void*)&keep_alive, sizeof(keep_alive)) == -1)
                    printf("setsockopt SO_KEEPALIVE error:[%s]\n", strerror(errno));

                if (setsockopt(fd, SOL_TCP, TCP_KEEPIDLE, (void*)&keep_idle, sizeof(keep_idle)) == -1)
                    printf("setsockopt TCP_KEEPIDLE error:[%s]\n", strerror(errno));

                if (setsockopt(fd, SOL_TCP, TCP_KEEPINTVL, (void*)&keep_interval, sizeof(keep_interval)) == -1)
                    printf("setsockopt TCP_KEEPINTVL error:[%s]\n", strerror(errno));

                if (setsockopt(fd, SOL_TCP, TCP_KEEPCNT, (void*)&keep_count, sizeof(keep_count)) == -1)
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
        * Method:    ����
        * Returns:   �ɹ�����trueʧ��false
        * Parameter: mode ģʽ
        * Parameter: ipport IP�Ͷ˿�
        * Parameter: fd socket
        * Parameter: needAuth �Ƿ���Ҫ��Ȩ
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
        * Method:    �Ƿ�������
        * Returns:   �Ƿ���true���򷵻�false
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
        * Method:    �Ƿ�Ͽ�
        * Returns:   �Ƿ���true���򷵻�false
        *************************************/
        inline bool IsDisconnected() const { return m_state == NsDisconnected; }

        /************************************
        * Method:    ��ȡsocket
        * Returns:   socket
        *************************************/
        inline SocketType GetSocket() const { return m_fd; }

        /************************************
        * Method:    ��ȡIP�Ͷ˿�
        * Returns:   ����IP�Ͷ˿�
        *************************************/
        inline  const std::string& GetAddress() const { return m_ipport; }

        inline const std::string& GetIP() const { return m_ip; }
        
    protected:
        /************************************
        * Method:    ��ȡģʽ
        * Returns:   ����ģʽ
        *************************************/
        inline NetworkMode GetMode() const { return static_cast<NetworkMode>(int32_t(m_mode)); }
        /************************************
        * Method:    ��ȡ״̬
        * Returns:   ����״̬
        *************************************/
        inline NetworkState GetState() const { return static_cast<NetworkState>(int32_t(m_state)); }
        /************************************
        * Method:    ����״̬
        * Returns:   
        * Parameter: s ״̬
        *************************************/
        inline void SetState(NetworkState s) const { m_state = s; }
        /************************************
        * Method:    ���Ӵ�������
        * Returns:   
        * Parameter: mode ģʽ
        * Parameter: ipport IP�Ͷ˿�
        *************************************/
        virtual void OnConnected(NetworkMode mode, const std::string& ipport, SocketType fd)
        {
            printf("%s connected\n", ipport.c_str());
        }
        /************************************
        * Method:    �Ͽ���������
        * Returns:   
        * Parameter: mode ģʽ
        * Parameter: ipport IP�Ͷ˿�
        * Parameter: fd socket
        *************************************/
        virtual void OnDisconnected(NetworkMode mode, const std::string& ipport, SocketType fd)
        {
            printf("%s disconnected\n", ipport.c_str());
        }
        /************************************
        * Method:    ����Ϣ��������
        * Returns:   
        * Parameter: msgs ����Ϣ
        *************************************/
        virtual void OnMessage(const std::vector<MessageType>& msgs)
        {
            printf("%s recv struct message, count:[%d]\n", msgs.size());
        }
        /************************************
        * Method:    ���������ݴ�������
        * Returns:   
        * Parameter: ev ����
        *************************************/
        virtual void OnMessage(const std::vector<KBuffer>& ev)
        {
            printf("%s recv raw message, count:[%d]\n", ev.size());
        }
        /************************************
        * Method:    ������Ȩ����
        * Returns:   ��Ȩ�ɹ�����true���򷵻�false
        *************************************/
        virtual bool OnAuthRequest() const
        {
            return !m_auth.need;
        }
        /************************************
        * Method:    ������Ȩ��Ӧ
        * Returns:   ��Ȩ�ɹ�����true���򷵻�false
        * Parameter: ev ��Ȩ��������
        *************************************/
        virtual bool OnAuthResponse(const std::vector<KBuffer>& ev) const
        {
            return !m_auth.need;
        }

    private:
        /************************************
        * Method:    �����¼�
        * Returns:   
        * Parameter: ev �¼�
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
        * Method:    ��������
        * Returns:   
        * Parameter: fd socket
        * Parameter: bufs ����
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
        * Method:    ����
        * Returns:
        * Parameter: ipport IP�Ͷ˿�
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
        * Method:    �Ͽ�����
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

        // ���� //
        KTcpNetwork<MessageType>* m_poller;

    private:
        // IP�˿� //
        std::string m_ip;
        std::string m_port;
        std::string m_ipport;
        // socket //
        volatile SocketType m_fd;
        // ģʽ //
        AtomicInteger<int32_t> m_mode;
        // ʣ������ //
        KBuffer m_remain;
        // ״̬ //
        mutable AtomicInteger<int32_t> m_state;
        // ��Ȩ //
        Authorization m_auth;
        
    };
};
