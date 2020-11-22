#pragma once
#include <vector>
#include "KEventObject.h"
#include "KBuffer.h"
#include "KBase64.h"
#include "KTcpBase.h"

namespace klib
{
	class KTcpMessage// rewrite this class
	{
	public:
		virtual size_t GetPayloadSize() const { return 0; }
		virtual size_t GetHeaderSize() const{ return 0; }
		virtual bool IsValid() { return false; }
		virtual void Clear() {  }
	};

	enum { ProtocolError = 0, ParseSuccess = 1, ShortHeader = 2, ShortPayload = 3 };

	template<typename MessageType>
	int ParseBlock(const KBuffer& dat, MessageType& msg, KBuffer& left)
	{
		return ProtocolError;
	}

	template<typename MessageType>
	void Parse(const std::vector<KBuffer>& dats, std::vector<MessageType>& msgs, KBuffer& remain, bool autoRelease = true)
	{
		std::vector<KBuffer>& bufs = const_cast<std::vector<KBuffer>&>(dats);
		if (remain.GetSize() > 0)
		{
			bufs.front().PrependBuffer(remain.GetData(), remain.GetSize());
			remain.Release();
		}
		std::vector<KBuffer>::iterator it = bufs.begin();
		while (it != bufs.end())
		{
			KBuffer dat = *it;
			KBuffer tail;
			MessageType msg;
			int rc = ParseBlock(dat, msg, tail);
			switch (rc)
			{
			case ProtocolError:
			{
				dat.Release();
				++it;
				break;
			}
			case ParseSuccess:
			{
				msgs.push_back(msg);
				if (autoRelease)
					dat.Release();
				if (tail.GetSize() > 0)
					*it = tail;
				else
					++it;
				break;
			}
			case ShortHeader:
			{
				if (++it != bufs.end())
				{
					it->PrependBuffer(dat.GetData(), dat.GetSize());
					dat.Release();
				}
				else
					remain = dat;
				break;
			}
			default:
			{
				size_t pl = msg.GetPayloadSize();
				std::vector<KBuffer>::iterator bit = it;
				size_t sz = 0;
				while (pl + msg.GetHeaderSize() > sz && it != bufs.end())
				{
					sz += it->GetSize();
					++it;
				}

				if (it != bufs.end())
					sz += it->GetSize();

				KBuffer tmp(sz);
				while (bit != it)
				{
					tmp.ApendBuffer(bit->GetData(), bit->GetSize());
					bit->Release();
					++bit;
				}

				if (it != bufs.end())
				{
					tmp.ApendBuffer(it->GetData(), it->GetSize());
					it->Release();
					*it = tmp;
				}
				else
				{
					if (pl + msg.GetHeaderSize() <= sz)
					{
						it = bufs.end() - 1;
						*it = tmp;
					}
					else
						remain = tmp;
				}
			}
			}
		}
	}

	template<typename MessageType>
	class KTcpProcessor :public KEventObject<std::vector<KBuffer> >
	{
	public:
		typedef MessageType ProcessorMessageType;
		KTcpProcessor()
			:KEventObject<std::vector<KBuffer> >("KTcpProcessor Thread"), m_ready(false),m_base(NULL)
		{

		}

		virtual ~KTcpProcessor()
		{
			m_remain.Release();
		}

		bool Start(KTcpBase* base)
		{
			m_base = base;
			assert(m_base != NULL);
			return KEventObject<std::vector<KBuffer> >::Start();
		}

		virtual bool Handshake()
		{
			return false;
		}

		virtual void Serialize(const MessageType& msg, KBuffer& result) const
		{

		}

	protected:
		virtual void OnMessages(const std::vector<MessageType>& msgs)
		{
		}

		virtual void OnRaw(const std::vector<KBuffer>& ev)
		{
			std::vector<KBuffer>& bufs = const_cast<std::vector<KBuffer>&>(ev);
			std::vector<KBuffer>::iterator it = bufs.begin();
			while (it != bufs.end())
			{
				std::cout << std::string(it->GetData(), it->GetSize()) << std::endl;
				it->Release();
				++it;
			}
		}

		virtual bool IsServer() const { return false; }

		virtual bool NeedPrepare() const { return false; }

		virtual void Prepare(const std::vector<KBuffer>& ev){ m_ready = true; }

		inline bool IsPrepared() const { return m_ready; }

	private:
		virtual void ProcessEvent(const std::vector<KBuffer>& ev)
		{
			if (!ev.empty())
			{
				if (NeedPrepare() && !m_ready)
				{
					Prepare(ev);
				}
				else
				{
					MessageType t;
					if (t.GetHeaderSize() > 0)
					{
						std::vector<MessageType> msgs;
						Parse(ev, msgs, m_remain);
						if (!msgs.empty())
							OnMessages(msgs);
					}
					else
					{
						OnRaw(ev);
					}
				}
			}
		}		

	protected:
		KTcpBase* m_base;

	private:
		KBuffer m_remain;
		volatile bool m_ready;
	};

	typedef KTcpProcessor<KTcpMessage> KTcpDefaultProcessor;
};