#pragma once
#include <CPushConsumer.h>
#include <string>
#include <stdint.h>
#include <sstream>
#include <vector>
#include "thread/KEventObject.h"

namespace thirdparty {
    using namespace klib;
    struct RocketMqMessage
    {
        std::string topic;
        std::string tags;
        std::string keys;
        std::string body;
    };

    class KRocketMqConsumer :public KEventObject<RocketMqMessage>
    {
    public:
        KRocketMqConsumer();

        bool Start(const std::string& brokers, const std::vector<std::string>& topics, const std::string& groupid);

        void Stop();

    protected:
        virtual void ProcessEvent(const RocketMqMessage& ev)
        {

        }

    private:
        //************************************
        // Method:    处理接收到的rocketmq消息
        // FullName:  KRocketMqConsumer::ProcessMessage
        // Access:    private static 
        // Returns:   int
        // Qualifier:
        // Parameter: struct CPushConsumer * consumer
        // Parameter: CMessageExt * msg
        //************************************
        static int ProcessMessage(struct CPushConsumer* consumer, CMessageExt* msg);

        //************************************
        // Method:    订阅主题
        // FullName:  KRocketMqConsumer::SubscribeTopics
        // Access:    private 
        // Returns:   bool
        // Qualifier:
        // Parameter: const std::vector<std::string> & topics
        //************************************
        bool SubscribeTopics(const std::vector<std::string>& topics);

    private:
        CPushConsumer* m_consumer;
        static KRocketMqConsumer* m_self;
    };
};
