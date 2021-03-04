#pragma once

#include <cms/Connection.h>
#include <cms/Session.h>
#include <cms/Destination.h>
#include <cms/ExceptionListener.h>
#include <cms/MessageListener.h>
#include <cms/MessageConsumer.h>
#include <cms/CMSException.h>
#include <cms/Message.h>
#include <decaf/lang/Exception.h>
#include <activemq/transport/DefaultTransportListener.h>
#include <activemq/core/ActiveMQConnectionFactory.h>
#include <activemq/core/ActiveMQConnection.h>
#include <activemq/library/ActiveMQCPP.h>

#include "thread/KAtomic.h"
#include <stdint.h>
using namespace activemq;
using namespace activemq::core;
using namespace activemq::transport;
using namespace decaf::lang;
using namespace decaf::util;
using namespace decaf::util::concurrent;
using namespace cms;

namespace thirdparty {

    class KActivemqConsumer:public cms::ExceptionListener,
        public cms::MessageListener,
        public activemq::transport::DefaultTransportListener
    {
    public:
        enum ConsumerState { CSDisconnected = 0, CSConnected = 1, CSException = 2 };
        KActivemqConsumer(const std::vector<std::string>& brokers, const std::string& destURI);

        static void Initialize();
        static void Shutdown();

        template<typename PointerType>
        static void Release(PointerType*& p)
        {
            try {
                if (p != NULL) delete p;
            }
            catch (cms::CMSException& e) {}
            p = NULL;
        }

        virtual bool Start();
        virtual void Stop();

        inline bool IsConnected() const { return m_state == CSConnected; }

    protected:
        virtual bool OnText(const TextMessage* msg) { return true; }

        virtual void onMessage(const Message* message);

        virtual void onException(const CMSException& ex)
        {
            m_state = CSException;
            printf("CMS Exception occurred:[%s].  Shutting down client.\n", ex.what());
        }

        virtual void transportInterrupted()
        {
            printf("The Connection's Transport has been Interrupted.\n");
        }

        virtual void transportResumed()
        {
            printf("The Connection's Transport has been Restored.\n");
        }

    private:
        virtual void Cleanup();
        std::string GetBrokerUrl(const std::vector<std::string>& ips) const;

    protected:
        Connection* m_connection;
        Session* m_session;
        Destination* m_destination;
        std::string m_brokerURI;
        std::string m_destURI;
        MessageConsumer* m_consumer;
        klib::AtomicInteger<int32_t> m_state;
    };
};

