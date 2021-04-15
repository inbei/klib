#ifndef __KMODBUSCLIENT_HPP_
#define __KMODBUSCLIENT_HPP_
#if defined(WIN32)
#include <WS2tcpip.h>
#endif
#include "tcp/KTcpClient.hpp"
#include "tcp/KTcpModbus.h"

/**
modbus 客户端类
**/

namespace klib
{
    class KModbusClient :public KTcpClient<KModbusMessage>
    {
    public:
        KModbusClient()
            :m_seq(0)
        {

        }

    protected:
        /************************************
        * Method:    创建连接
        * Returns:   返回连接
        * Parameter: fd socket ID
        * Parameter: ipport IP和端口
        *************************************/
        virtual KTcpConnection<KModbusMessage>* NewConnection(SocketType fd, const std::string& ipport)
        {
            return new KTcpModbus(this);
        }

        /************************************
        * Method:    获取序列号
        * Returns:   返回序列号
        *************************************/
        uint16_t GetSeq()
        {
            uint16_t seq = m_seq++;
            if (seq == 0xffff)
                m_seq = 0;
            return seq;
        }

    private:
        uint16_t m_seq;
    };
};

#endif
