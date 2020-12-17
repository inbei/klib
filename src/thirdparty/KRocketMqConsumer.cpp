#include "thirdparty/KRocketMqConsumer.h"

namespace thirdparty {
    KRocketMqConsumer* KRocketMqConsumer::m_self = NULL;

    KRocketMqConsumer::KRocketMqConsumer()
        : m_consumer(NULL), KEventObject<RocketMqMessage>("KRocketMqConsumer Thread")
    {
        m_self = this;
    }

    bool KRocketMqConsumer::Start(const std::string& brokers, const std::vector<std::string>& topics, const std::string& groupid)
    {
        if (!KEventObject<RocketMqMessage>::Start())
        {
            return false;
        }

        m_consumer = CreatePushConsumer(groupid.c_str());
        if (!m_consumer)
        {
            KEventObject<RocketMqMessage>::Stop();
            KEventObject<RocketMqMessage>::WaitForStop();
            return false;
        }

        if (SubscribeTopics(topics)
            && RegisterMessageCallback(m_consumer, ProcessMessage) == 0
            && SetPushConsumerNameServerAddress(m_consumer, brokers.c_str()) == 0
            && StartPushConsumer(m_consumer) == 0)
        {
            fprintf(stdout, "KRocketMqConsumer brokers:[%s], topic:[%d], groupid:[%s] start success\n",
                brokers.c_str(), topics.size(), groupid.c_str());
            return true;
        }

        Stop();
        return false;
    }

    void KRocketMqConsumer::Stop()
    {
        if (NULL != m_consumer)
        {
            ShutdownPushConsumer(m_consumer);
            UnregisterMessageCallback(m_consumer);
            DestroyPushConsumer(m_consumer);
            m_consumer = NULL;

            KEventObject<RocketMqMessage>::Stop();
        }
    }

    int KRocketMqConsumer::ProcessMessage(struct CPushConsumer* consumer, CMessageExt* msg)
    {
        RocketMqMessage rmsg;
        rmsg.topic = GetMessageTopic(msg);
        rmsg.keys = GetMessageKeys(msg);
        rmsg.tags = GetMessageTags(msg);
        rmsg.body = GetMessageBody(msg);
        return !Post(m_self->GetID(), rmsg);
    }

    bool KRocketMqConsumer::SubscribeTopics(const std::vector<std::string>& topics)
    {
        bool rc = false;
        std::vector<std::string>::const_iterator it = topics.begin();
        while (it != topics.end())
        {
            if (Subscribe(m_consumer, it->c_str(), "*") == 0)
            {
                rc = true;
            }
            ++it;
        }
        return rc;
    }
};
