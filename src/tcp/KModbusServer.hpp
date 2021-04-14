#ifndef ___KMODBUSSERVER_HPP__
#define ___KMODBUSSERVER_HPP__

#if defined(WIN32)
#include <WS2tcpip.h>
#endif
#include "tcp/KTcpServer.hpp"
#include "tcp/KTcpModbus.h"

/**
modbus 服务端类
**/

namespace klib
{
    class KModbusServer :public KTcpServer<KModbusMessage>
    {
    public:
        /************************************
        * Method:    发送数据给客户端
        * Returns:   成功返回true失败false
        * Parameter: fd socket ID
        * Parameter: msg 数据
        *************************************/
        bool Send(SocketType fd, const KBuffer& msg)
        {
            KModbusMessage wmsg(0xff, 0x04);
            wmsg.InitializeResponse(0, msg.GetSize(), msg);
            klib::KBuffer buf;
            wmsg.Serialize(buf);
            std::vector<KBuffer> bufs;
            bufs.push_back(buf);
            return SendClient(fd, SocketEvent::SeSent, bufs);
        }

    protected:
        /************************************
        * Method:    创建新连接
        * Returns:   返回新连接
        * Parameter: fd socket ID
        * Parameter: ipport IP端口
        *************************************/
        virtual KTcpConnection<KModbusMessage>* NewConnection(SocketType fd, const std::string& ipport)
        {
            return new KTcpModbus(this);
        }
    };
};

#endif // ___KMODBUSSERVER_HPP__
