#include "tcp/KTcpWriter.h"
namespace klib {
    klib::KTcpWriter::KTcpWriter()
        :KEventObject<KBuffer>("KTcpWriter Thread"), m_base(NULL)
    {

    }

    bool klib::KTcpWriter::Start(KTcpBase* base)
    {
        m_base = base;
        assert(m_base != NULL);
        return KEventObject<KBuffer>::Start();
    }

    void klib::KTcpWriter::ProcessEvent(const KBuffer& ev)
    {
        assert(m_base != NULL);
        SocketType fd = m_base->GetSocket();
        int rc = m_base->WriteSocket(fd, ev.GetData(), ev.GetSize());
        if (rc < 0)
            m_base->DeleteSocket(fd);
        else if (rc != ev.GetSize())
            std::cout << "write failed\n";
        const_cast<KBuffer&>(ev).Release();
    }

    bool KTcpWriter::IsReady() const
    {
        assert(m_base != NULL);
        return m_base->IsConnected();
    }

}
