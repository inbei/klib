#pragma once
#include "KTcpProcessor.hpp"
#include "util/KEndian.h"
namespace klib
{
#define MaxRegisterCount 32765
	struct KModbusMessage :public KTcpMessage
	{
	public:
		enum
		{
			ModbusNull, ModbusRequst, ModbusResponse
		};

		KModbusMessage(int mt = ModbusResponse)
			:messageType(mt), seq(0), ver(0), len(0), dev(0),
			func(0), saddr(0), count(0), ler(0) {}

		virtual size_t GetPayloadSize() const { return len; }
		virtual size_t GetHeaderSize() const { return sizeof(seq) + sizeof(ver) + sizeof(len); }
		virtual bool IsValid() { return (ver == 0x0 && dev == 0xff && func == 0x04); }
		virtual void Clear() { len = 0; dev = 0; func = 0; }

		bool ToRequest()
		{
			if (sizeof(saddr) + sizeof(count) == payload.GetSize())
			{
				uint8_t* src = (uint8_t*)payload.GetData();
				size_t offset = 0;
				KEndian::FromNetwork(src + offset, saddr);
				offset += sizeof(saddr);
				KEndian::FromNetwork(src + offset, count);
				return count <= MaxRegisterCount;
			}
			return false;
		}

		void ToResponse()
		{
			char* src = payload.GetData();
			size_t offset = 0;
			ler = src[offset++];
			size_t lsz = payload.GetSize() - offset;
			if (lsz > 0)
			{
				dat.Release();
				dat = KBuffer(lsz);
				dat.ApendBuffer(src + offset, lsz);
			}
		}
	public:
		uint16_t seq;
		uint16_t ver;
		uint16_t len;

		uint8_t dev;
		uint8_t func;

		KBuffer payload; // response payload, need to release manually

		uint16_t saddr; // request
		uint16_t count; // request

		uint8_t ler;// response len or error code
		KBuffer dat;// response, need to release manually

		int messageType;
	};

	template<>
	int ParseBlock(const KBuffer& dat, KModbusMessage& msg, KBuffer& left)
	{
		size_t hsz = msg.GetHeaderSize() + sizeof(msg.dev) + sizeof(msg.func);
		if (dat.GetSize() < hsz)
			return ShortHeader;

		uint8_t* src = (uint8_t*)dat.GetData();
		size_t offset = 0;
		KEndian::FromNetwork(src + offset, msg.seq);
		offset += sizeof(msg.seq);
		KEndian::FromNetwork(src + offset, msg.ver);
		offset += sizeof(msg.ver);
		KEndian::FromNetwork(src + offset, msg.len);
		offset += sizeof(msg.len);

		msg.dev = src[offset++];
		msg.func = src[offset++];

		if (!msg.IsValid())
		{
			std::cout << "invalid modbus packet\n";
			return ProtocolError;
		}

		size_t ssz = dat.GetSize();
		if (ssz < msg.GetHeaderSize() + msg.GetPayloadSize())
			return ShortPayload;

		size_t psz = msg.len - sizeof(msg.dev) - sizeof(msg.func);
		msg.payload = KBuffer(psz);
		msg.payload.ApendBuffer((const char*)src + offset, psz);
		offset += psz;

		// left data
		if (offset < ssz)
		{
			KBuffer tmp(ssz - offset);
			tmp.ApendBuffer((const char*)src + offset, tmp.Capacity());
			left = tmp;
		}
		return ParseSuccess;
	};

	class KModbusProcessor :public KTcpProcessor<KModbusMessage>
	{
	protected:
		virtual void OnMessages(const std::vector<KModbusMessage>& msgs)
		{
			std::vector<KModbusMessage>& ms = const_cast<std::vector<KModbusMessage>&>(msgs);
			std::vector<KModbusMessage>::iterator it = ms.begin();
			while (it != ms.end())
			{
				if(it->ToRequest())
					std::cout << "request header size:" << it->GetHeaderSize() << ", payload size:" << it->GetPayloadSize() << std::endl;
				else
				{
					it->ToResponse();
					std::cout << "response header size:" << it->GetHeaderSize() << ", payload size:" << it->GetPayloadSize() << std::endl;
				}
				++it;
			}
		}
	};
};