#ifndef __KMODBUSCLIENT_HPP_
#define __KMODBUSCLIENT_HPP_
#if defined(WIN32)
#include <WS2tcpip.h>
#endif
#include "tcp/KTcpClient.hpp"
#include "tcp/KTcpModbus.h"

namespace klib
{
    class KWebsocketClient :public KTcpClient<KModbusMessage>
    {
    public:


    protected:
        virtual KTcpConnection<KModbusMessage>* NewConnection(SocketType fd, const std::string& ipport)
        {
            return new KTcpModbus(this);
        }
    };
};

#endif
