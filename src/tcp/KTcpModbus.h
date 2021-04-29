#pragma once
#if defined(WIN32)
#include <WS2tcpip.h>
#endif
#include "tcp/KTcpConnection.hpp"
#include "tcp/KTcpNetwork.h"
#include "util/KEndian.h"
/**
modbus数据处理类
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
        * Method:    获取消息体大小
        * Returns:   
        *************************************/
        virtual size_t GetPayloadSize() const { return len; }
        /************************************
        * Method:    获取消息头大小
        * Returns:   
        *************************************/
        virtual size_t GetHeaderSize() const { return sizeof(seq) + sizeof(ver) + sizeof(len); }
        /************************************
        * Method:    判断消息是否有效
        * Returns:   
        * Parameter: dev 设备ID
        * Parameter: func 功能码
        *************************************/
        virtual bool IsValid(uint16_t dev, uint16_t func) {

            return ver == 0
                && this->dev == dev
                && this->func == func
                && (MaxModbusAddress - saddr + 1 >= count)
                && saddr < MaxModbusAddress;
        }
        /************************************
        * Method:    判断消息是否有效
        * Returns:   
        *************************************/
        virtual bool IsValid() {

            return ver == 0
                && ((dev == 0xff && func == 0x04) || (dev == 0x01 && func == 0x03))
                && (MaxModbusAddress - saddr + 1 >= count)
                && saddr < MaxModbusAddress;
        }

        /************************************
        * Method:    清理缓存
        * Returns:   
        *************************************/
        virtual void Clear() { len = 0; dev = 0; func = 0; }

        KModbusMessage()
            :dev(0), func(0), messageType(ModbusNull),
            seq(0), ver(0), len(0), saddr(0), count(0), ler(0) {}

        /************************************
        * Method:    解析为请求
        * Returns:   成功返回true失败false
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
        * Method:    解析为响应
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
        * Method:    初始化为请求
        * Returns:   
        * Parameter: seq 序列号
        * Parameter: saddr 开始地址
        * Parameter: count 寄存器个数
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
        * Method:    初始化为响应
        * Returns:   
        * Parameter: seq 序列号
        * Parameter: ler 字节数
        * Parameter: buf 消息内容
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
        * Method:    获取消息体
        * Returns:   返回消息体
        *************************************/
        const KBuffer& GetPayload() const { return payload; }
        /************************************
        * Method:    获取响应数据
        * Returns:   番长数据
        *************************************/
        const KBuffer& GetData() const { return dat; }

        /************************************
        * Method:    释放消息体
        * Returns:   
        *************************************/
        void ReleasePayload() { payload.Release(); }
        /************************************
        * Method:    释放数据
        * Returns:   
        *************************************/
        void ReleaseData() { dat.Release(); }
        /************************************
        * Method:    获取序列号
        * Returns:   返回序列号
        *************************************/
        inline uint16_t GetSeq() const { return seq; }
        /************************************
        * Method:    获取开始地址
        * Returns:   返回地址
        *************************************/
        inline uint16_t GetStartAddress() const { return saddr; }
        /************************************
        * Method:    获取寄存器个数
        * Returns:   返回个数
        *************************************/
        inline uint16_t GetCount() const { return count; }

        /************************************
        * Method:    序列化消息
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
        * Method:    新的modbus消息
        * Returns:   
        * Parameter: msgs 新消息
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
        * Method:    原始数据
        * Returns:   
        * Parameter: ev 数据
        *************************************/
        virtual void OnMessage(const std::vector<KBuffer>& ev)
        {
            //printf("%s recv raw message, count:[%d]\n", ev.size());
            KTcpUtil::Release(const_cast<std::vector<KBuffer>&>(ev));
        }
    };
};
