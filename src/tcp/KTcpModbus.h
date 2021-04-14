#pragma once
#if defined(WIN32)
#include <WS2tcpip.h>
#endif
#include "tcp/KTcpConnection.hpp"
#include "tcp/KTcpNetwork.h"
#include "util/KEndian.h"
/**
modbus���ݴ�����
**/
namespace klib
{
#define MaxRegisterCount 32765

#define MaxModbusAddress 65535
    
    struct KModbusMessage :public KTcpMessage
    {
    public:
        enum
        {
            ModbusNull, ModbusRequest, ModbusResponse
        };
        friend int ParsePacket<KModbusMessage>(const KBuffer& dat, KModbusMessage& msg, KBuffer& left);

        /************************************
        * Method:    ��ȡ��Ϣ���С
        * Returns:   
        *************************************/
        virtual size_t GetPayloadSize() const { return len; }
        /************************************
        * Method:    ��ȡ��Ϣͷ��С
        * Returns:   
        *************************************/
        virtual size_t GetHeaderSize() const { return sizeof(seq) + sizeof(ver) + sizeof(len); }
        /************************************
        * Method:    �ж���Ϣ�Ƿ���Ч
        * Returns:   
        * Parameter: dev �豸ID
        * Parameter: func ������
        *************************************/
        virtual bool IsValid(uint16_t dev, uint16_t func) {

            return ver == 0
                && this->dev == dev
                && this->func == func
                && (MaxModbusAddress - saddr + 1 >= count)
                && saddr < MaxModbusAddress;
        }
        /************************************
        * Method:    �ж���Ϣ�Ƿ���Ч
        * Returns:   
        *************************************/
        virtual bool IsValid() {

            return ver == 0
                && ((dev == 0xff && func == 0x04) || (dev == 0x01 && func == 0x03))
                && (MaxModbusAddress - saddr + 1 >= count)
                && saddr < MaxModbusAddress;
        }

        /************************************
        * Method:    ������
        * Returns:   
        *************************************/
        virtual void Clear() { len = 0; dev = 0; func = 0; }

        KModbusMessage()
            :dev(0), func(0), messageType(ModbusNull),
            seq(0), ver(0), len(0), saddr(0), count(0), ler(0) {}

        /************************************
        * Method:    ����Ϊ����
        * Returns:   �ɹ�����trueʧ��false
        *************************************/
        bool ParseRequest()
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

        /************************************
        * Method:    ����Ϊ��Ӧ
        * Returns:   
        *************************************/
        void ParseResponse()
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

        KModbusMessage(uint16_t dev, uint16_t func)
            :dev(dev & 0xff), func(0xff & func), messageType(ModbusNull),
            seq(0), ver(0), len(0), saddr(0), count(0), ler(0) {}

        /************************************
        * Method:    ��ʼ��Ϊ����
        * Returns:   
        * Parameter: seq ���к�
        * Parameter: saddr ��ʼ��ַ
        * Parameter: count �Ĵ�������
        *************************************/
        void InitializeRequest(uint16_t seq, uint16_t saddr, uint16_t count)
        {
            messageType = ModbusRequest;
            this->seq = seq;
            ver = 0;
            len = 0x06;
            this->saddr = saddr;
            this->count = count;
        }

        /************************************
        * Method:    ��ʼ��Ϊ��Ӧ
        * Returns:   
        * Parameter: seq ���к�
        * Parameter: ler �ֽ���
        * Parameter: buf ��Ϣ����
        *************************************/
        void InitializeResponse(uint16_t seq, uint16_t ler, const KBuffer& buf)
        {
            messageType = ModbusResponse;
            this->seq = seq;
            ver = 0;
            len = 3 + buf.GetSize();
            this->ler = ler & 0xff;
            dat = buf;
        };

        /************************************
        * Method:    ��ȡ��Ϣ��
        * Returns:   ������Ϣ��
        *************************************/
        const KBuffer& GetPayload() const { return payload; }
        /************************************
        * Method:    ��ȡ��Ӧ����
        * Returns:   ��������
        *************************************/
        const KBuffer& GetData() const { return dat; }

        /************************************
        * Method:    �ͷ���Ϣ��
        * Returns:   
        *************************************/
        void ReleasePayload() { payload.Release(); }
        /************************************
        * Method:    �ͷ�����
        * Returns:   
        *************************************/
        void ReleaseData() { dat.Release(); }
        /************************************
        * Method:    ��ȡ���к�
        * Returns:   �������к�
        *************************************/
        inline uint16_t GetSeq() const { return seq; }
        /************************************
        * Method:    ��ȡ��ʼ��ַ
        * Returns:   ���ص�ַ
        *************************************/
        inline uint16_t GetStartAddress() const { return saddr; }
        /************************************
        * Method:    ��ȡ�Ĵ�������
        * Returns:   ���ظ���
        *************************************/
        inline uint16_t GetCount() const { return count; }

        /************************************
        * Method:    ���л���Ϣ
        * Returns:   
        * Parameter: result
        *************************************/
        virtual void Serialize(KBuffer& result)
        {
            switch (messageType)
            {
            case KModbusMessage::ModbusRequest:
            {
                // 00 01 00 00 00 06 ff 04 00 01 00 01
                result = KBuffer(12);
                size_t offset = 0;
                uint8_t* dst = (uint8_t*)result.GetData();
                KEndian::ToBigEndian(seq, dst + offset);
                offset += sizeof(seq);
                KEndian::ToBigEndian(ver, dst + offset);
                offset += sizeof(ver);
                KEndian::ToBigEndian(len, dst + offset);
                offset += sizeof(len);

                dst[offset++] = dev;
                dst[offset++] = func;

                KEndian::ToBigEndian(saddr, dst + offset);
                offset += sizeof(saddr);
                KEndian::ToBigEndian(count, dst + offset);
                offset += sizeof(count);
                result.SetSize(offset);
                break;
            }
            case KModbusMessage::ModbusResponse:
            {
                size_t sz = dat.GetSize();
                result = KBuffer(9 + sz);
                size_t offset = 0;
                uint8_t* dst = (uint8_t*)result.GetData();
                KEndian::ToBigEndian(seq, dst + offset);
                offset += sizeof(seq);
                KEndian::ToBigEndian(ver, dst + offset);
                offset += sizeof(ver);
                KEndian::ToBigEndian(len, dst + offset);
                offset += sizeof(len);

                dst[offset++] = dev;
                dst[offset++] = func;
                dst[offset++] = ler;

                memcpy(dst + offset, dat.GetData(), sz);
                offset += sz;
                result.SetSize(offset);
                break;
            }
            default:
                break;
            }
        }
    private:
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
    int ParsePacket(const KBuffer& dat, KModbusMessage& msg, KBuffer& left);

    class KTcpModbus :public KTcpConnection<KModbusMessage>
    {
    public:
        KTcpModbus(KTcpNetwork<KModbusMessage>* poller)
            :KTcpConnection<KModbusMessage>(poller)
        {

        }

    protected:
        /************************************
        * Method:    �µ�modbus��Ϣ
        * Returns:   
        * Parameter: msgs ����Ϣ
        *************************************/
        virtual void OnMessage(const std::vector<KModbusMessage>& msgs)
        {
            std::vector<KModbusMessage>& ms = const_cast<std::vector<KModbusMessage>&>(msgs);
            std::vector<KModbusMessage>::iterator it = ms.begin();
            while (it != ms.end())
            {
                if (it->ParseRequest())
                    printf("request header size:[%d], payload size:[%d]\n", it->GetHeaderSize(), it->GetPayloadSize());
                else
                {
                    it->ParseResponse();
                    printf("response header size:[%d], payload size:[%d]\n", it->GetHeaderSize(), it->GetPayloadSize());
                }
                it->ReleaseData();
                it->ReleasePayload();
                ++it;
            }
        }  

        /************************************
        * Method:    ԭʼ����
        * Returns:   
        * Parameter: ev ����
        *************************************/
        virtual void OnMessage(const std::vector<KBuffer>& ev)
        {
            printf("%s recv raw message, count:[%d]\n", ev.size());
            KTcpUtil::Release(const_cast<std::vector<KBuffer>&>(ev));
        }
    };
};
