#pragma once
#if defined(WIN32)
#include <WS2tcpip.h>
#elif defined(AIX)
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <sys/pollset.h>
#elif defined(HPUX)
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/mpctl.h>
#include <sys/poll.h>
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
namespace klib {

#if defined(WIN32)
#define SocketType SOCKET
#else
#define SocketType int
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

#define PollTimeOut 100
#define BlockSize 40960
#define MaxEvent 40

    class KTcpMessage// rewrite this class
    {
    public:
        virtual size_t GetPayloadSize() const { return 0; }
        virtual size_t GetHeaderSize() const { return 0; }
        virtual bool IsValid() { return false; }
        virtual void Clear() {  }
    };

    enum { ProtocolError = 0, ParseSuccess = 1, ShortHeader = 2, ShortPayload = 3 };

    template<typename MessageType>
    int ParsePacket(const KBuffer& dat, MessageType& msg, KBuffer& left)
    {
        return ProtocolError;
    }

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

    struct Authorization
    {
        bool need;
        bool authSent;
        bool authRecv;

        Authorization(bool need = false)
            :need(need), authSent(false), authRecv(false)
        {
        }

        inline void Reset() { authSent = false; authRecv = false; }
    };

    struct SocketEvent
    {
        enum EventType
        {
            SeUndefined, SeRecv, SeSent
        };

        SocketType fd;
        EventType ev;
        std::vector<KBuffer> dat1;
        std::string dat2;
    };

    enum NetworkState
    {
        NsUndefined, NsThreadStarted, NsPeerConnected, NsReadyToWork
    };

    enum NetworkMode
    {
        NmUndefined, NmClient, NmServer
    };

    static int WriteSocket(SocketType fd, const char* dat, size_t sz)
    {
        if (sz < 1 || dat == NULL || fd < 1)
            return 0;

        int sent = 0;
        while (sent != sz && fd > 0)
        {
            int rc = ::send(fd, dat + sent, sz - sent, 0/*MSG_DONTWAIT  MSG_WAITALL*/);
            if (rc > 0)
                sent += rc;
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
        return sent;
    }

    static int ReadSocket(SocketType fd, std::vector<KBuffer>& dat)
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

    template<typename MessageType>
    class KTcpNetwork;

    template<typename MessageType>
    class KTcpConnection:public KEventObject<SocketEvent>
    {
    public:
        KTcpConnection(KTcpNetwork<MessageType> *poller)
            :KEventObject<SocketEvent>("Socket event thread"),
            m_state(NsUndefined), m_mode(NmUndefined), m_poller(poller)
        {

        }

        ~KTcpConnection()
        {
            
        }

        bool Start(NetworkMode mode, const std::string& ipport, SocketType fd, bool needAuth = false)
        {
            m_mode = mode;
            m_auth.need = needAuth;
            if (KEventObject<SocketEvent>::Start())
            {
                Connect(ipport, fd);
                return true;
            }
            return false;
        }

        void Connect(const std::string& ipport, SocketType fd)
        {
            m_ipport = ipport;
            m_fd = fd;
            OnConnected(GetMode(), ipport);
        }

        void Disconnect(SocketType fd)
        {
            OnDisconnected(GetMode(), m_ipport, fd);
        }

        inline bool IsConnected() const { return m_state >= NsPeerConnected; }

        inline SocketType GetSocket() const { return m_fd; }
        
    protected:
        inline NetworkMode GetMode() const { return static_cast<NetworkMode>(int32_t(m_mode)); }
        inline NetworkState GetState() const { return static_cast<NetworkState>(m_state); }
        inline void SetState(NetworkState s) { m_state = s; }

        virtual void OnConnected(NetworkMode mode, const std::string& ipport)
        {
            SetState(NsPeerConnected);
            printf("%s connected\n", ipport.c_str());
        }

        virtual void OnDisconnected(NetworkMode mode, const std::string& ipport, SocketType fd)
        {
            m_auth.Reset();
            m_remain.Release();
            SetState(NsThreadStarted);
            m_poller->DeleteSocket(fd);
            // clear data
            printf("%s disconnected\n", ipport.c_str());
        }

        virtual void OnMessage(const std::vector<MessageType>& msgs)
        {

        }
        virtual void OnMessage(const std::vector<KBuffer>& ev)
        {

        }

        virtual bool OnAuthRequest() const
        {
            return !m_auth.need;
        }
        virtual bool OnAuthResponse(const std::vector<KBuffer>& ev) const
        {
            return !m_auth.need;
        }

    private:
        virtual void ProcessEvent(const SocketEvent& ev)
        {
            switch (ev.ev)
            {
            case SocketEvent::SeSent:
            {
                if (!m_auth.authSent)
                    m_auth.authSent = OnAuthRequest();
                else
                {
                    if (!ev.dat2.empty() && WriteSocket(ev.fd, ev.dat2.c_str(), ev.dat2.size()) < 0)
                        OnDisconnected(GetMode(), m_ipport, ev.fd);
                    else if (!ev.dat1.empty())
                    {
                        bool disconnected = false;
                        std::vector<KBuffer >& bufs = const_cast<std::vector<KBuffer > &>(ev.dat1);
                        std::vector<KBuffer >::iterator it = bufs.begin();
                        while (it != bufs.end())
                        {
                            KBuffer& buf = *it;
                            if (!disconnected && WriteSocket(ev.fd, buf.GetData(), buf.GetSize()) < 0)
                            {
                                OnDisconnected(GetMode(), m_ipport, ev.fd);
                                disconnected = true;
                            }
                            buf.Release();
                            ++it;
                        }
                    }
                }
                break;
            }
            case SocketEvent::SeRecv:
            {
                if (ev.dat1.empty())
                {
                    std::vector<KBuffer> buffers;
                    if (ReadSocket(ev.fd, buffers) < 0)
                        OnDisconnected(GetMode(), m_ipport, ev.fd);

                    if (!buffers.empty())
                    {
                        if (!m_auth.authRecv)
                            if (!(m_auth.authRecv = OnAuthResponse(buffers)))
                                Disconnect(ev.fd);
                        else
                        {
                            MessageType t;
                            if (t.GetHeaderSize() > 0)
                            {
                                std::vector<MessageType> msgs;
                                Parse(buffers, msgs, m_remain);
                                if (!msgs.empty())
                                    OnMessage(msgs);
                            }
                            else
                            {
                                OnMessage(ev.dat1);
                            }
                        }
                    }
                }
                else
                {
                    if (!m_auth.authRecv)
                        if (!(m_auth.authRecv = OnAuthResponse(ev.dat1)))
                            Disconnect(ev.fd);
                    else
                    {
                        MessageType t;
                        if (t.GetHeaderSize() > 0)
                        {
                            std::vector<MessageType> msgs;
                            Parse(ev.dat1, msgs, m_remain);
                            if (!msgs.empty())
                                OnMessage(msgs);
                        }
                        else
                        {
                            OnMessage(ev.dat1);
                        }
                    }
                }
                break;
            }
            default:
                break;
            }
        }

    private:
        std::string m_ipport;
        SocketType m_fd;

        AtomicInteger<int32_t> m_mode;

        KBuffer m_remain;
        AtomicInteger<int32_t> m_state;
        Authorization m_auth;

        KTcpNetwork<MessageType>* m_poller;
    };
};
