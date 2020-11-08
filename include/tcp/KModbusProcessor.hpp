#pragma once
#include "KTcpProcessor.hpp"
#include "KEndian.h"
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

		virtual bool Valid() { return (ver == 0x0 && dev == 0xff && func == 0x04); }

		virtual void Clear() { len = 0; dev = 0; func = 0; }

		virtual size_t GetPayloadLength() const { return len; }

		virtual size_t HeaderSize() const { return sizeof(seq) + sizeof(ver) + sizeof(len); }

		inline size_t Size()const { return HeaderSize() + len; }

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

	class KModbusProcessor :public KTcpProcessor<KModbusMessage>
	{
	protected:
		/*
		-1 protocol error
		0 whole message
		1 short header
		2 short payload
		rewrite this method
		*/
		virtual int ParseBlock(const KBuffer& dat, KModbusMessage& msg, KBuffer& left)
		{
			size_t hsz = msg.HeaderSize() + sizeof(msg.dev) + sizeof(msg.func);
			if (dat.GetSize() < hsz)
				return shortheader;

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

			if (!msg.Valid())
			{
				std::cout << "invalid modbus packet\n";
				return protoerr;
			}

			size_t ssz = dat.GetSize();
			if (ssz < msg.HeaderSize() + msg.GetPayloadLength())
				return shortpayload;

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
			return success;
		};

		virtual void OnMessages(const std::vector<KModbusMessage>& msgs)
		{
			std::vector<KModbusMessage>& ms = const_cast<std::vector<KModbusMessage>&>(msgs);
			std::vector<KModbusMessage>::iterator it = ms.begin();
			while (it != ms.end())
			{
				if(it->ToRequest())
					std::cout << "request header size:" << it->HeaderSize() << ", payload size:" << it->GetPayloadLength() << std::endl;
				else
				{
					it->ToResponse();
					std::cout << "response header size:" << it->HeaderSize() << ", payload size:" << it->GetPayloadLength() << std::endl;
				}
				++it;
			}
		}
	};
};