#pragma once
#include "tcp/KTcpConnection.hpp"
#include "util/KStringUtility.h"
#include "util/KBase64.h"
#include "util/KSHA1.h"
#include "util/KEndian.h"
#include "tcp/KTcpNetwork.h"
/**
websocket数据处理类
**/
namespace klib
{
    class KWebsocketMessage :public KTcpMessage
    {
    public:
        friend class KTcpWebsocket;
        friend int ParsePacket<KWebsocketMessage>(const KBuffer& dat, KWebsocketMessage& msg, KBuffer& left);
        enum {
            opmore = 0x0, optext = 0x1,
            opbinary = 0x2, opclose = 0x8, opping = 0x9, oppong = 0xa
        };

        enum { mskno = 0, mskyes = 1 };

        enum { finmore = 0, finlast = 1 };

        KWebsocketMessage()
            :fin(0), reserved(0), opcode(0x0f), mask(0), plen(0), extplen()
        {

        }

        /************************************
        * Method:    获取消息体大小
        * Returns:   
        *************************************/
        virtual size_t GetPayloadSize() const
        {
            if (plen == 126)
                return extplen.extplen2;
            else if (plen == 127)
                return size_t(extplen.extplen8);
            else
                return plen;
        }

        /************************************
        * Method:    获取消息头大小
        * Returns:   
        *************************************/
        virtual size_t GetHeaderSize() const
        {
            if (plen == 126)
                return sizeof(uint16_t) * 2 + (mask ? sizeof(maskkey) : 0);
            else if (plen == 127)
                return sizeof(uint16_t) + sizeof(uint64_t) + (mask ? sizeof(maskkey) : 0);
            else
                return sizeof(uint16_t) + (mask ? sizeof(maskkey) : 0);

        }

        /************************************
        * Method:    判断消息是否有效
        * Returns:   
        *************************************/
        virtual bool IsValid()
        {
            return (reserved == 0 && (opcode == opmore
                || opcode == optext
                || opcode == opbinary
                || opcode == opclose
                || opcode == opping
                || opcode == oppong));
        }

        virtual void Clear()
        {
            opcode = 0x0f;
            plen = 0;
        }

        /************************************
        * Method:    设置payload大小
        * Returns:   
        * Parameter: sz
        *************************************/
        void SetPayloadSize(size_t sz)
        {
            if (sz < 126)
                plen = sz;
            else if (sz < 65535)
            {
                plen = 126;
                extplen.extplen2 = sz;
            }
            else
            {
                plen = 127;
                extplen.extplen8 = sz;
            }
        }

        /************************************
        * Method:    初始化消息
        * Returns:   
        * Parameter: msg
        *************************************/
        void Initialize(const std::string& msg)
        {
            fin = finlast;
            reserved = 0;
            opcode = optext;
            mask = 0;
            SetPayloadSize(msg.size());
            memset(maskkey, 0, sizeof(maskkey));
            payload.ApendBuffer(msg.c_str(), msg.size());
        }

        /************************************
        * Method:    序列化消息
        * Returns:   
        * Parameter: result
        *************************************/
        virtual void Serialize(KBuffer& result)
        {
            size_t psz = payload.GetSize();
            result = KBuffer(2 + 8 + 4 + psz);
            uint8_t* dst = reinterpret_cast<uint8_t*>(result.GetData());

            // first
            size_t offset = 0;
            dst[offset++] = uint8_t((fin << 7) + opcode);

            // second
            dst[offset++] = uint8_t((mask << 7) + plen);

            //fprintf(stdout, "header:%02x %02x \n", uint8_t(dst[0]), uint8_t(dst[1]));
            if (psz >= 126 && psz <= 65535)
            {
                // length
                KEndian::ToBigEndian(uint16_t(psz), dst + offset);
                offset += sizeof(uint16_t);

                //fprintf(stdout, "plen:%02x %02x \n", uint8_t(dst[2]), uint8_t(dst[3]));
            }
            else if (psz > 65535)
            {
                // length
                KEndian::ToBigEndian(uint64_t(psz), dst + offset);
                offset += sizeof(uint64_t);

                /*fprintf(stdout, "plen:%02x %02x %02x %02x %02x %02x %02x %02x \n",
                    uint8_t(dst[2]), uint8_t(dst[3]), uint8_t(dst[4]), uint8_t(dst[5]),
                    uint8_t(dst[6]), uint8_t(dst[7]), uint8_t(dst[8]), uint8_t(dst[9]));*/
            }

            // mask key
            if (mask & 0x1)
            {
                size_t msz = sizeof(maskkey);
                memcpy(dst + offset, maskkey, msz);
                offset += msz;
            }

            result.SetSize(offset);
            // payload
            if (psz > 0)
            {
                // payload
                uint8_t* src = reinterpret_cast<uint8_t*>(payload.GetData());
                // result
                if ((mask & 0x1) != 1)
                {
                    memmove(dst + offset, src, psz);
                }
                else
                {
                    size_t mod4 = 0;//dsz % 4;
                    for (uint32_t i = 0; i < psz; ++i)
                    {
                        dst[offset + i] = src[i] ^ maskkey[(i + mod4) % 4];
                    }
                }
                result.SetSize(offset + psz);
            }
        }

    private:
        uint8_t fin : 1; //1 last frame, 0 more frame
        uint8_t reserved : 3;
        /*
        (4 bits) 0x0 more frame, 0x1 text frame, 0x2 binary frame, 0x3-7 reserved, 0x8 closed,
        0x9 ping, 0xa pong
        */
        uint8_t opcode : 4;

        uint8_t mask : 1; //表示是否要对数据载荷进行掩码异或操作, 1 yes, 0 no 

        /*
        表示数据载荷的长度
        0~125：数据的长度等于该值；
        126：后续 2 个字节代表一个 16 位的无符号整数，该无符号整数的值为数据的长度；
        127：后续 8 个字节代表一个 64 位的无符号整数（最高位为 0），该无符号整数的值为数据的长度
        */
        uint8_t plen : 7;

        union
        {
            uint16_t extplen2;
            uint64_t extplen8;
        } extplen;

        /*
        当 mask 为 1，则携带了 4 字节的 Masking-key；
        当 mask 为 0，则没有 Masking-key。
        掩码算法：按位做循环异或运算，先对该位的索引取模来获得 Masking-key 中对应的值 x，然后对该位与 x 做异或，从而得到真实的 byte 数据。
        注意：掩码的作用并不是为了防止数据泄密，而是为了防止早期版本的协议中存在的代理缓存污染攻击（proxy cache poisoning attacks）等问题
        */
        char maskkey[4]; // 0 or 4 bytes
        KBuffer payload;
    };

    template<>
    int ParsePacket(const KBuffer& dat, KWebsocketMessage& msg, KBuffer& left);

    class KTcpWebsocket :public KTcpConnection<KWebsocketMessage>
    {
    public:
        KTcpWebsocket(KTcpNetwork<KWebsocketMessage>* poller)
            :KTcpConnection<KWebsocketMessage>(poller)
        {

        }

    protected:
        /************************************
        * Method:    二进制消息触发
        * Returns:   
        * Parameter: dat
        *************************************/
        virtual void OnBinary(const KBuffer& dat)// binary message
        {

        }

        /************************************
        * Method:    文本消息触发
        * Returns:   
        * Parameter: dat
        *************************************/
        virtual void OnText(const std::string& dat) // text message
        {

        }

        /************************************
        * Method:    连接触发
        * Returns:   
        * Parameter: mode
        * Parameter: ipport
        *************************************/
        virtual void OnConnected(NetworkMode mode, const std::string& ipport)
        {
            KTcpConnection<KWebsocketMessage>::OnConnected(mode, ipport);
        }

        /************************************
        * Method:    断开触发
        * Returns:   
        * Parameter: mode
        * Parameter: ipport
        * Parameter: fd
        *************************************/
        virtual void OnDisconnected(NetworkMode mode, const std::string& ipport, SocketType fd)
        {
            KTcpConnection<KWebsocketMessage>::OnDisconnected(mode, ipport, fd);
            m_partial.Clear();
        }

        /************************************
        * Method:    请求授权触发
        * Returns:   
        *************************************/
        virtual bool OnAuthRequest() const// client
        {
            /*
            GET / HTTP/1.1
            Upgrade: websocket
            Connection: Upgrade
            Sec-WebSocket-Key: sN9cRrP/n9NdMgdcy2VJFQ==
            Sec-WebSocket-Version: 13
            */
            m_secKey = "helloshit";
            std::string req;
            req.append("GET / HTTP/1.1\r\n");
            req.append("Upgrade: websocket\r\n");
            req.append("Connection: Upgrade\r\n");
            req.append("Sec-WebSocket-Key: ");
            req.append(m_secKey + "\r\n");
            req.append("Sec-WebSocket-Version: 13\r\n\r\n");

            return WriteSocket(GetSocket(), req.c_str(), req.size()) == req.size();
        }

        /************************************
        * Method:    响应授权触发
        * Returns:   
        * Parameter: ev
        *************************************/
        virtual bool OnAuthResponse(const std::vector<KBuffer>& ev) const
        {
            bool rc = false;
            std::string req(ev[0].GetData(), ev[0].GetSize());
            SocketType fd = GetSocket();
            if (GetMode() == NmServer)// server
            {
                // get key
                std::string wskey;
                if (!GetHandshakeKey(req, "Sec-WebSocket-Key", wskey))
                    goto end;

                std::string protocol;
                GetHandshakeKey(req, "Sec-WebSocket-Protocol", protocol);
                if (!protocol.empty())
                    protocol += "\r\n";

                // generate key
                GetHandshakeResponseKey(wskey);
                wskey += "\r\n";
                /*
                HTTP/1.1 101 Switching Protocols
                Upgrade: websocket
                Connection: Upgrade
                Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=
                Sec-WebSocket-Protocol: mqttv3.1
                */
                std::string resp;
                resp.append("HTTP/1.1 101 Switching Protocols\r\n");
                resp.append("Connection: upgrade\r\n");
                resp.append("Sec-WebSocket-Accept: ");
                resp.append(wskey);
                if (!protocol.empty())
                {
                    resp.append("Sec-WebSocket-Protocol: ");
                    resp.append(protocol);
                }
                resp.append("Upgrade: websocket\r\n\r\n");

                if (WriteSocket(fd, resp.c_str(), resp.size()) == resp.size())
                {
                    printf("handshake with client successfully\n");
                    rc = true;
                }
            }
            else // client
            {
                std::string wskey;
                GetHandshakeKey(req, "Sec-WebSocket-Accept", wskey);
                std::string respKey(m_secKey);
                GetHandshakeResponseKey(respKey);
                if (respKey == wskey)
                {
                    printf("handshake with server successfully\n");
                    rc = true;
                }
            }

        end:
            KTcpNetwork<KWebsocketMessage>::Release(const_cast<std::vector<KBuffer>&>(ev));
            if (rc)
                SetState(NsReadyToWork);
            return rc;
        }

        /************************************
        * Method:    新消息触发
        * Returns:   
        * Parameter: msgs
        *************************************/
        virtual void OnMessage(const std::vector<KWebsocketMessage>& msgs)
        {
            std::vector<KWebsocketMessage>& ms = const_cast<std::vector<KWebsocketMessage>&>(msgs);
            std::vector<KWebsocketMessage>::iterator it = ms.begin();
            while (it != ms.end())
            {
                MergeMessage(*it, m_partial);
                ++it;
            }
        }

        /************************************
        * Method:    二进制数据触发
        * Returns:   
        * Parameter: ev
        *************************************/
        virtual void OnMessage(const std::vector<KBuffer>& ev)
        {
            printf("%s recv raw message, count:[%d]\n", ev.size());
            KTcpNetwork<KWebsocketMessage>::Release(const_cast<std::vector<KBuffer>&>(ev));
        }

    private:
        /************************************
        * Method:    获取key
        * Returns:   
        * Parameter: reqstr
        * Parameter: keyword
        * Parameter: wskey
        *************************************/
        bool GetHandshakeKey(const std::string& reqstr, const std::string& keyword, std::string& wskey) const
        {
            std::istringstream s(reqstr);
            std::string line;
            while (std::getline(s, line, '\n'))
            {
                std::vector<std::string> strs;
                KStringUtility::SplitString2(line, ": ", strs);
                if (strs.size() == 2 && strs[0].find(keyword) != std::string::npos)
                {
                    wskey = strs[1];
                    wskey.erase(wskey.end() - 1);
                }
            }
            return !wskey.empty();
        }

        /************************************
        * Method:    获取响应key
        * Returns:   
        * Parameter: wskey
        *************************************/
        void GetHandshakeResponseKey(std::string& wskey) const
        {
            wskey += "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
            KSHA1 sha;
            unsigned int msgdigest[5];
            sha.Reset();
            sha << wskey.c_str();
            sha.Result(msgdigest);
            for (int i = 0; i < 5; i++)
                msgdigest[i] = htonl(msgdigest[i]);
            wskey = KBase64::Encode(reinterpret_cast<const char*>(msgdigest), 20);
        }

        /************************************
        * Method:    合并分片消息
        * Returns:   
        * Parameter: msg 消息
        * Parameter: partial 消息片
        *************************************/
        void MergeMessage(KWebsocketMessage& msg, KWebsocketMessage& partial)
        {
            switch (msg.opcode)
            {
            case KWebsocketMessage::optext:
            case KWebsocketMessage::opbinary:
            case KWebsocketMessage::opmore:
            {
                if (KWebsocketMessage::finlast == msg.fin)// last frame
                {
                    if (partial.IsValid())
                    {
                        AppendBuffer(msg, partial);
                        if (partial.opcode == KWebsocketMessage::opbinary)
                        {
                            OnBinary(partial.payload);
                        }
                        else
                            OnText(std::string(partial.payload.GetData(), partial.payload.GetSize()));
                        partial.payload.Release();
                        partial.Clear();
                    }
                    else
                    {
                        if (msg.opcode == KWebsocketMessage::opbinary)
                        {
                            OnBinary(msg.payload);
                        }
                        else
                            OnText(std::string(msg.payload.GetData(), msg.payload.GetSize()));
                        msg.payload.Release();
                    }
                }
                else// not last frame
                {
                    if (partial.IsValid())// middle frame
                        AppendBuffer(msg, partial);
                    else// first frame
                        partial = msg;
                }
                break;
            }
            case KWebsocketMessage::opclose:
            {
                printf("web socket recv close request start\n");
                Disconnect(GetSocket());
                printf("web socket recv close request end\n");
                msg.payload.Release();
                break;
            }
            default:
                break;
            }
        }

        /************************************
        * Method:    追加消息
        * Returns:   
        * Parameter: msg 消息
        * Parameter: partial 消息片
        *************************************/
        void AppendBuffer(KWebsocketMessage& msg, KWebsocketMessage& partial) const
        {
            partial.payload.ApendBuffer(msg.payload.GetData(), msg.payload.GetSize());
            partial.SetPayloadSize(partial.payload.GetSize() + msg.payload.GetSize());
            msg.payload.Release();
        }

    private:
        KWebsocketMessage m_partial;
        mutable std::string m_secKey;// client
    };
};
