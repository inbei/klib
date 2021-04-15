#pragma once
#if defined(WIN32)
#include <WS2tcpip.h>
#endif
#include "util/KStringUtility.h"
#include "tcp/KTcpNetwork.h"
/**
tcp 客户端类
**/

namespace klib {

    template<typename MessageType>
    class KTcpClient :public KTcpNetwork<MessageType>
    {
    public:
        /************************************
        * Method:    启动客户端
        * Returns:   成功返回true否则返回false
        * Parameter: hosts 格式：1.1.1.1:12345,2.2.2.2:23456
        * Parameter: needAuth 是否需要授权
        *************************************/
        bool Start(const std::string& hosts, bool needAuth = false)
        {
            std::vector<std::string> brokers;
            klib::KStringUtility::SplitString(hosts, ",", brokers);
            std::vector<std::string>::const_iterator it = brokers.begin();
            while (it != brokers.end())
            {
                const std::string& broker = *it;
                std::vector<std::string> ipandport;
                klib::KStringUtility::SplitString(broker, ":", ipandport);
                if (ipandport.size() != 2)
                {
                    printf("Invalid broker:[%s]\n", broker.c_str());
                }
                else
                {
                    uint16_t port = atoi(ipandport[1].c_str());
                    if (port == 0)
                        printf("Invalid broker:[%s]\n", broker.c_str());
                    else
                        m_hostip[ipandport[0]] = port;
                }
                ++it;
            }

            if (m_hostip.empty())
            {
                printf("Invalid brokers:[%s]\n", hosts.c_str());
                return false;
            }

            m_it = m_hostip.begin();
            return KTcpNetwork<MessageType>::Start(m_it->first, m_it->second, false, needAuth);
        }

        /************************************
        * Method:    临时断开连接，由于自带重连功能，所以又会连接上
        * Returns:   
        *************************************/
        void Disconnect()
        {
            Disconnect(KTcpNetwork<MessageType>::GetSocket());
        }

    protected:
        /************************************
        * Method:    GetConfig获取配置
        * Returns:   返回IP和端口
        *************************************/
        virtual std::pair<std::string, uint16_t> GetConfig() const
        {
            if (KTcpNetwork<MessageType>::IsConnected())
                return *m_it;
            else
            {
                if (++m_it != m_hostip.end())
                {
                    return *m_it;
                }
                else
                {
                    m_it = m_hostip.begin();
                    return *m_it;
                }
            }
        }

    private:
        std::map<std::string, uint16_t> m_hostip;
        mutable std::map<std::string, uint16_t>::const_iterator m_it;
    };
};
