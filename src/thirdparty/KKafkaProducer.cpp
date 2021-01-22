#include "thirdparty/KKafkaProducer.h"

namespace thirdparty {
    KKafkaProducer::KKafkaProducer() :m_running(false)
    {
        m_eventcb = new KEventCb(m_running);
    }

    KKafkaProducer::~KKafkaProducer()
    {
        Release(m_eventcb);
        m_producer.Release();
    }

    void KKafkaProducer::Initialize(const KafkaConf& conf)
    {
        m_conf = conf;
    }

    bool KKafkaProducer::CreateProducer()
    {
        m_producer.Release();
        std::string errStr;
        RdKafka::Conf* kc = RdKafka::Conf::create(RdKafka::Conf::CONF_GLOBAL);
        //Kafka server
        if (kc->set("bootstrap.servers", m_conf.brokers, errStr)
            != RdKafka::Conf::CONF_OK)
        {
            printf("Kafka CreateProducer set bootstrap.servers property error:[%s]\n",
                errStr.c_str());
        }

        //事件回调       
        if (kc->set("event_cb", m_eventcb, errStr)
            != RdKafka::Conf::CONF_OK)
        {
            printf("Kafka CreateProducer set event_cb property error:[%s]\n",
                errStr.c_str());
        }

        //
        if (kc->set("linger.ms", "100", errStr)
            != RdKafka::Conf::CONF_OK)
        {
            printf("Kafka CreateProducer set linger.ms property error:[%s]\n",
                errStr.c_str());
        }

        //
        if (kc->set("batch.num.messages", "16000", errStr)
            != RdKafka::Conf::CONF_OK)
        {
            printf("Kafka CreateProducer set batch.num.messages property error:[%s]\n",
                errStr.c_str());
        }

        // 
        RdKafka::Producer* producer = RdKafka::Producer::create(kc, errStr);
        Release(kc);
        if (!producer)
        {
            printf("Kafka CreateProducer create Producer error:[%s]\n",
                errStr.c_str());
            return false;
        }
        printf("Kafka create Producer success:[%s]\n", m_conf.brokers.c_str());
        // topic conf
        kc = RdKafka::Conf::create(RdKafka::Conf::CONF_TOPIC);
        RdKafka::Topic* topic = RdKafka::Topic::create(producer, m_conf.topicName, kc, errStr);
        Release(kc);
        if (!topic)
        {
            Release(producer);
            printf("Kafka CreateProducer create Topic error:[%s]\n", errStr.c_str());
            return false;
        }

        printf("Kafka create topic success:[%s]\n", m_conf.topicName.c_str());
        RdKafka::Metadata* metadata;
        RdKafka::ErrorCode err = producer->metadata(false, topic, &metadata, 3000);
        if (err == RdKafka::ERR_NO_ERROR)
        {
            RdKafka::Metadata::TopicMetadataIterator it = metadata->topics()->begin();
            while (it != metadata->topics()->end())
            {
                size_t psz = (*it)->partitions()->size();
                if (psz > 0)
                    m_conf.partition = m_conf.partitionKey % psz;
                else
                    m_conf.partition = 0;
                ++it;
            }
        }
        m_producer.producer = producer;
        m_producer.topic = topic;
        m_running = true;
        return true;
    }

    bool KKafkaProducer::Produce(const std::string& msg, std::string& errStr)
    {
        if (!m_running && !CreateProducer())
            return false;

        RdKafka::ErrorCode resp = m_producer.producer->produce(m_producer.topic,
            m_conf.partition, RdKafka::Producer::RK_MSG_COPY,
            const_cast<char*>(msg.c_str()), msg.size(), NULL, NULL);
        if (RdKafka::ERR_NO_ERROR != resp)
        {
            if (RdKafka::ERR__UNKNOWN_PARTITION == resp)
                m_conf.partition = 0;
            std::ostringstream os;
            os << resp;
            errStr = os.str();
            return false;
        }
        return true;
    }

    void KKafkaProducer::Poll(int ms)
    {
        if (m_running && m_producer.producer)
            m_producer.producer->poll(ms);
    }

};
