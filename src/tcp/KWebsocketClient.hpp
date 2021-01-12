#ifndef __KWEBSOCKETCLIENT_HPP__
#define __KWEBSOCKETCLIENT_HPP__
#if defined(WIN32)
#include <WS2tcpip.h>
#endif
#include "tcp/KTcpClient.hpp"
#include "tcp/KTcpWebsocket.h"
/**
websocket客户端
**/
namespace klib
{
    class KWebsocketClient :public KTcpClient<KWebsocketMessage>
    {
    public:


    protected:
        /************************************
        * Method:    创建连接
        * Returns:   
        * Parameter: fd
        * Parameter: ipport
        *************************************/
        virtual KTcpConnection<KWebsocketMessage>* NewConnection(SocketType fd, const std::string& ipport)
        {
            return new KTcpWebsocket(this);
        }
    };
};

#endif
