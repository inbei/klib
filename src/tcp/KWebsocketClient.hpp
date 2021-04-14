#ifndef __KWEBSOCKETCLIENT_HPP__
#define __KWEBSOCKETCLIENT_HPP__
#if defined(WIN32)
#include <WS2tcpip.h>
#endif
#include "tcp/KTcpClient.hpp"
#include "tcp/KTcpWebsocket.h"
/**
websocket�ͻ���
**/
namespace klib
{
    class KWebsocketClient :public KTcpClient<KWebsocketMessage>
    {
    public:


    protected:
        /************************************
        * Method:    ��������
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
