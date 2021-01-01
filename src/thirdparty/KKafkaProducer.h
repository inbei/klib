#pragma once

#include "librdkafka/rdkafkacpp.h"
#include "thread/KMutex.h"
#include "thread/KLockGuard.h"
#include "thread/KAtomic.h"

namespace thirdparty {
    using namespace klib;
    class KEventCb : public RdKafka::EventCb {
    public:
        KEventCb(AtomicBool& running)
            :m_running(running)
        {

        }

        void event_cb(RdKafka::Event& event)
        {
            switch (event.type())
            {
            case RdKafka::Event::EVENT_ERROR:
            {
                RdKafka::ErrorCode ec = event.err();
                if (ec == RdKafka::ERR__ALL_BROKERS_DOWN || RdKafka::ERR__TRANSPORT == ec)
                    m_running = false;
                std::ostringstream os;
                os << ec;
                printf("kafka error:[%s]\n", os.str().c_str());
                break;
            }
            default:
                ;
            }
        }

    private:
        AtomicBool& m_running;
    };

    struct KafkaConf
    {
        int partition;
        std::string topicName;
        std::string brokers;
        int partitionKey;

        KafkaConf()
            :partitionKey(1), partition(0)
        {

        }
    };

    class KKafkaProducer
    {
        struct KafkaProducer
        {
            RdKafka::Producer* producer;
            RdKafka::Topic* topic;

            KafkaProducer()
                :producer(NULL), topic(NULL)
            {

            }

            void Release()
            {
                if (topic)
                {
                    delete topic;
                    topic = NULL;
                }

                if (producer)
                {
                    delete producer;
                    producer = NULL;
                }
            }
        };
    public:
        KKafkaProducer();
        ~KKafkaProducer();

        void Initialize(const KafkaConf& conf);
        bool Produce(const std::string& msg, std::string& errStr);
        void Poll(int ms = 0);

    private:
        bool CreateProducer();
        template<typename PointerType>
        void Release(PointerType*& p)
        {
            if (p)
            {
                delete p;
                p = NULL;
            }
        }

    private:
        KEventCb* m_eventcb;
        KafkaProducer m_producer;
        KafkaConf m_conf;
        AtomicBool m_running;
    };
};
