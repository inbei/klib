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
		if (m_base->IsConnected())
		{
			SocketType fd = m_base->GetFd();
			if (m_base->WriteSocket(fd, ev.GetData(), ev.GetSize()) < 0)
				m_base->DelFd(fd);
		}
		const_cast<KBuffer&>(ev).Release();
	}
}