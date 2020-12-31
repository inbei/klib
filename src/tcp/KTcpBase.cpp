#include "tcp/KTcpBase.h"
#include "util/KTime.h"
namespace klib {
    klib::KTcpBase::KTcpBase() :m_connected(false)
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

    klib::KTcpBase::~KTcpBase()
    {
#if defined(WIN32)
        WSACleanup();
#elif defined(AIX)
        pollset_destroy(m_pfd);
#elif defined(LINUX)
        close(m_pfd);
#endif
    }

    int klib::KTcpBase::PollSocket()
    {
        KLockGuard<KMutex> lock(m_fdsMtx);
        int rc = 0;
        if (m_fds.empty())
            return rc;
#if defined(WIN32)
        rc = WSAPoll(&m_fds[0], m_fds.size(), PollTimeOut);
        for (size_t i = 0; rc > 0 && i < m_fds.size(); ++i)
            OnSocketEvent(m_fds[i].fd, m_fds[i].revents);
#elif defined(HPUX)
        rc = ::poll(&m_fds[0], nfds_t(m_fds.size()), PollTimeOut);
        for (size_t i = 0; rc > 0 && i < m_fds.size(); ++i)
            OnSocketEvent(m_fds[i].fd, m_fds[i].revents);
#elif defined(LINUX)
        rc = epoll_wait(m_pfd, m_ps, MaxEvent, PollTimeOut);
        for (int i = 0; i < rc; ++i)
            OnSocketEvent(m_ps[i].data.fd, m_ps[i].events);
#elif defined(AIX)
        rc = pollset_poll(m_pfd, m_ps, MaxEvent, PollTimeOut);
        for (int i = 0; i < rc; ++i)
            OnSocketEvent(m_ps[i].fd, m_ps[i].revents);
#endif
        return rc;
    }

    void klib::KTcpBase::DeleteSocket(SocketType fd)
    {
        KLockGuard<KMutex> lock(m_fdsMtx);
        DeleteSocketNoLock(fd);
    }

    bool klib::KTcpBase::AddSocket(SocketType fd, const std::string& ipport)
    {
        KLockGuard<KMutex> lock(m_fdsMtx);
        OnOpen(fd, ipport);
        if (!SetSocketNonBlock(fd))
        {
            OnClose(fd);
            CloseSocket(fd);
            return false;
        }
#if defined(AIX)
        poll_ctl ev;
        ev.fd = fd;
        ev.events = POLLIN | POLLHUP | POLLERR;
        // PS_ADD PS_MOD PS_DELETE
        ev.cmd = PS_ADD;
        //int rc = pollset_ctl(pollset_t ps, struct poll_ctl* pollctl_array,int array_length)
        if (pollset_ctl(m_pfd, &ev, 1) < 0)
        {
            OnClose(fd);
            CloseSocket(fd);
            return false;
        }
#elif defined(LINUX)
        epoll_event ev;
        ev.data.fd = fd;
        ev.events = EPOLLIN | EPOLLET | EPOLLERR | EPOLLHUP;
        if (epoll_ctl(m_pfd, EPOLL_CTL_ADD, fd, &ev) < 0)
        {
            OnClose(fd);
            CloseSocket(fd);
            return false;
        }
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
    }

    void klib::KTcpBase::DeleteSocketNoLock(SocketType fd)
    {
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
                OnClose(fd);
                CloseSocket(fd);
                break;
            }
            ++it;
        }
    }

    void klib::KTcpBase::CloseSocket(SocketType fd) const
    {
#if defined(WIN32)
        closesocket(fd);
#else
        ::close(fd);
#endif
    }

    bool klib::KTcpBase::SetSocketNonBlock(SocketType fd) const
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

    int KTcpBase::WriteSocket(SocketType fd, const char* dat, size_t sz) const
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


    void KTcpBase::ReuseAddress(SocketType fd)
    {
        // set reuse address
        int on = 1;
        ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,
            reinterpret_cast<const char*>(&on), sizeof(on));
    }

    void KTcpBase::DisableNagle(SocketType fd)
    {
        // disable Nagle
        int on = 1;
        ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY,
            reinterpret_cast<const char*>(&on), sizeof(on));
    }

};
