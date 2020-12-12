#pragma once
#include "KTcpProcessorWebsocket.hpp"

namespace klib {

    class KMqttMessage :public KTcpMessage
    {
    public:
        virtual size_t GetPayloadSize() const
        {
            return 0;
        }

        virtual size_t GetHeaderSize() const
        {
            return 0;
        }

        virtual bool IsValid()
        {
            return false;
        }

        virtual void Clear()
        {
        }
    };

    template<>
    int ParseBlock(const KBuffer& dat, KMqttMessage& msg, KBuffer& left)
    {
        return ProtocolError;
    }


    class KTcpProcessorMqtt :public KTcpProcessorWebsocket
    {
    public:
        ~KTcpProcessorMqtt()
        {
            m_mqttRemain.Release();
        }

    protected:
        virtual void OnWebsocket(const std::vector<KBuffer>& msgs)
        {
            std::vector<KMqttMessage> mqmsgs;
            Parse(msgs, mqmsgs, m_mqttRemain, false);
            OnMqttMessage(mqmsgs);
        }

        virtual void OnMqttMessage(const std::vector<KMqttMessage> mqmsgs)
        {

        }

    private:
        KBuffer m_mqttRemain;
    };


}
