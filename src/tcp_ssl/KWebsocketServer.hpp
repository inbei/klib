#ifndef __KWEBSOCKETSERVER_HPP__
#define __KWEBSOCKETSERVER_HPP__
#if defined(WIN32)
#include <WS2tcpip.h>
#endif
#include "tcp/KTcpServer.hpp"
#include "tcp/KTcpWebsocket.h"
/**
websocket服务端
**/
namespace klib
{
    class KWebsocketServer :public klib::KTcpServer<KWebsocketMessage>
    {
    public:
        /************************************
        * Method:    发送数据给客户端
        * Returns:   
        * Parameter: fd
        * Parameter: msg
        *************************************/
        bool Send(SocketType fd, const std::string& msg)
        {
            KWebsocketMessage wmsg;
            wmsg.Initialize(msg);
            klib::KBuffer buf;
            wmsg.Serialize(buf);
            std::vector<KBuffer> bufs;
            bufs.push_back(buf);
            if (!SendDataToConnection(fd, SocketEvent::SeSent, bufs))
            {
                buf.Release();
                return false;
            }
            return true;
        }

    protected:
        virtual KTcpConnection<KWebsocketMessage>* NewConnection(SocketType fd, const std::string& ipport)
        {
            return new KTcpWebsocket(this);
        }
    };
};
#endif
