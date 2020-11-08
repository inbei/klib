#ifndef _KTCPBASE_HPP_
#define _KTCPBASE_HPP_


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
#else
#error "WINDOWS AIX HPUX LINUX supported only"
#endif // defined(WIN32)

#include <cstdio>
#include <vector>
#include <string>
#include <assert.h>
#include "KBuffer.h"
#include "KAtomic.h"
#include "KMutex.h"
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

	class KTcpBase
	{
	public:
		KTcpBase();
		virtual ~KTcpBase();
		virtual bool IsConnected() const { return m_connected; }
		virtual SocketType GetFd() const { return 0; }

		void DelFd(SocketType fd);
		bool AddFd(SocketType fd, const std::string& ipport);

		int WriteSocket(SocketType fd, const char* dat, size_t sz) const;
		
    protected:
		virtual void OnSocketEvent(SocketType fd, short evt) = 0;
		virtual void OnOpen(SocketType fd, const std::string& ipport) { m_connected = true; }
		virtual void OnClose(SocketType fd) {m_connected = false;}

		void DelFdInternal(SocketType fd);
		void CloseSocket(SocketType fd) const;
		int Poll();

	private:
		bool SetSocketNonBlock(SocketType fd) const;

	private:
#if defined(AIX)
		int m_pfd;
		pollfd m_ps[MaxEvent];
#elif defined(LINUX)
		int m_pfd;
		epoll_event m_ps[MaxEvent];
#endif
		KMutex m_fdsMtx;
		std::vector<pollfd> m_fds;
		AtomicBool m_connected;
	};
};

#endif//_KTCPBASE_HPP_
