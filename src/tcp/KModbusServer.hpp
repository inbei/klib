#ifndef ___KMODBUSSERVER_HPP__
#define ___KMODBUSSERVER_HPP__

#if defined(WIN32)
#include <WS2tcpip.h>
#endif
#include "tcp/KTcpServer.hpp"
#include "tcp/KTcpModbus.h"

namespace klib
{
    class KModbusServer :public KTcpServer<KModbusMessage>
    {
    public:
        bool Send(SocketType fd, const KBuffer& msg)
        {
            KModbusMessage wmsg;
            wmsg.InitializeResponse(0, 0xff, 0x04, msg.GetSize(), msg);
            klib::KBuffer buf;
            klib::KTcpModbus::Serialize(wmsg, buf);
            std::vector<KBuffer> bufs;
            bufs.push_back(buf);
            SendDataToConnection(fd, SocketEvent::SeSent, bufs);
            return true;
        }

    protected:
        virtual KTcpConnection<KModbusMessage>* NewConnection(SocketType fd, const std::string& ipport)
        {
            return new KTcpModbus(this);
        }
    };
};

#endif // ___KMODBUSSERVER_HPP__
