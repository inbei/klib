#ifndef __KWEBSOCKETCLIENT_HPP__
#define __KWEBSOCKETCLIENT_HPP__
#if defined(WIN32)
#include <WS2tcpip.h>
#endif
#include "tcp/KTcpClient.hpp"
#include "tcp/KTcpWebsocket.h"

namespace klib
{
    class KWebsocketClient :public klib::KTcpClient<klib::KWebsocketMessage>
    {
    public:


    protected:
        virtual KTcpConnection<KWebsocketMessage>* NewConnection(SocketType fd, const std::string& ipport)
        {
            return new KTcpWebsocket(this);
        }
    };
};

#endif
