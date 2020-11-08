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
		virtual size_t GetPayloadLength() const { return 0; }
		virtual size_t HeaderSize() const{ return 0; }
	};

	template<typename MessageType>
	class KTcpProcessor :public KEventObject<std::vector<KBuffer> >
	{
	public:
		typedef MessageType ProcessorMessageType;

		enum{protoerr = 0, success = 1, shortheader = 2, shortpayload = 3};

		KTcpProcessor():KEventObject<std::vector<KBuffer> >("KTcpProcessor Thread"), m_ready(false){}

		virtual ~KTcpProcessor(){ m_remain.Release(); }

		bool Start(KTcpBase* base)
		{
			m_base = base;
			assert(m_base != NULL);
			return KEventObject<std::vector<KBuffer> >::Start();
		}

		virtual bool Handshake() { return false; }

		virtual void Serialize(const MessageType& msg, KBuffer& result) const{}

	protected:
		/*
		-1 protocol error
		0 whole message
		1 short header
		2 short payload
		rewrite this method
		*/
		virtual int ParseBlock(const KBuffer& dat, MessageType& msg, KBuffer& left){ return protoerr; }
		// rewrite this method
		virtual void OnMessages(const std::vector<MessageType>& msgs){}
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

		virtual bool NeedPrepare() const { return false; }

		virtual bool IsServer() const { return false; }

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
					if (t.HeaderSize() > 0)
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

		void Parse(const std::vector<KBuffer>& dats, std::vector<MessageType>& msgs, KBuffer& remain)
		{
			std::vector<KBuffer>& bufs = const_cast<std::vector<KBuffer>&>(dats);
			if (m_remain.GetSize() > 0)
			{
				bufs.front().PrependBuffer(m_remain.GetData(), m_remain.GetSize());
				m_remain.Release();
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
				case protoerr:
				{
					dat.Release();
					++it;
					break;
				}
				case success:
				{
					msgs.push_back(msg);
					dat.Release();
					if (tail.GetSize() > 0)
						*it = tail;
					else
						++it;
					break;
				}
				case shortheader:
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
					size_t pl = msg.GetPayloadLength();
					std::vector<KBuffer>::iterator bit = it;
					size_t sz = 0;
					while (pl + msg.HeaderSize() > sz && it != bufs.end())
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
						if (pl + msg.HeaderSize() <= sz)
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

	protected:
		KTcpBase* m_base;

	private:
		KBuffer m_remain;
		volatile bool m_ready;
	};

	typedef KTcpProcessor<KTcpMessage> KTcpDefaultProcessor;
};