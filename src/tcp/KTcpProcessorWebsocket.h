#pragma once
#include "tcp/KTcpProcessor.hpp"
#include "util/KStringUtility.h"
#include "util/KBase64.h"
#include "util/KSHA1.h"
#include "util/KEndian.h"
namespace klib
{
    class KWebsocketMessage :public KTcpMessage
    {
    public:
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

        virtual size_t GetPayloadSize() const
        {
            if (plen == 126)
                return extplen.extplen2;
            else if (plen == 127)
                return size_t(extplen.extplen8);
            else
                return plen;
        }
        virtual size_t GetHeaderSize() const
        {
            if (plen == 126)
                return sizeof(uint16_t) * 2 + (mask ? sizeof(maskkey) : 0);
            else if (plen == 127)
                return sizeof(uint16_t) + sizeof(uint64_t) + (mask ? sizeof(maskkey) : 0);
            else
                return sizeof(uint16_t) + (mask ? sizeof(maskkey) : 0);

        }
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

    public:
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
    int ParseBlock(const KBuffer& dat, KWebsocketMessage& msg, KBuffer& left);

    class KTcpProcessorWebsocket :public KTcpProcessor<KWebsocketMessage>
    {
    public:
        KTcpProcessorWebsocket()
        {

        }

        virtual bool Handshake() // client
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

            return m_base->WriteSocket(m_base->GetSocket(), req.c_str(), req.size()) == req.size();
        }

        virtual void Serialize(const KWebsocketMessage& msg, KBuffer& result) const
        {
            const KBuffer& payload = msg.payload;
            size_t psz = payload.GetSize();
            result = KBuffer(2 + 8 + 4 + psz);
            uint8_t* dst = reinterpret_cast<uint8_t*>(result.GetData());

            // first
            size_t offset = 0;
            dst[offset++] = uint8_t((msg.fin << 7) + msg.opcode);

            // second
            dst[offset++] = uint8_t((msg.mask << 7) + msg.plen);

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
            if (msg.mask & 0x1)
            {
                size_t msz = sizeof(msg.maskkey);
                memcpy(dst + offset, msg.maskkey, msz);
                offset += msz;
            }

            result.SetSize(offset);
            // payload
            if (psz > 0)
            {
                // payload
                uint8_t* src = reinterpret_cast<uint8_t*>(payload.GetData());
                // result
                if ((msg.mask & 0x1) != 1)
                {
                    memmove(dst + offset, src, psz);
                }
                else
                {
                    size_t mod4 = 0;//dsz % 4;
                    for (uint32_t i = 0; i < psz; ++i)
                    {
                        dst[offset + i] = src[i] ^ msg.maskkey[(i + mod4) % 4];
                    }
                }
                result.SetSize(offset + psz);
            }
        }

    protected:
        virtual void OnMessages(const std::vector<KWebsocketMessage>& msgs)
        {
            std::vector<KWebsocketMessage>& ms = const_cast<std::vector<KWebsocketMessage>&>(msgs);
            std::vector<KWebsocketMessage>::iterator it = ms.begin();
            while (it != ms.end())
            {
                MergeMessage(*it, m_partial);
                ++it;
            }
        }

        virtual bool NeedFirstResponse() const { return true; }

        virtual void FirstResponse(const std::vector<KBuffer>& ev)
        {
            std::string req(ev[0].GetData(), ev[0].GetSize());
            SocketType fd = m_base->GetSocket();
            if (IsServer())// server
            {
                // get key
                std::string wskey;
                if (!GetHandshakeKey(req, "Sec-WebSocket-Key", wskey))
                    return;

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

                if (m_base->WriteSocket(fd, resp.c_str(), resp.size()) == resp.size())
                {
                    std::cout << "handshake with client successfully\n";
                    KTcpProcessor<KWebsocketMessage>::FirstResponse(ev);
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
                    std::cout << "handshake with server successfully\n";
                    KTcpProcessor<KWebsocketMessage>::FirstResponse(ev);
                }
            }

            if (!IsFirstResponseReady())
                m_base->DeleteSocket(fd);

            std::vector<KBuffer>& bufs = const_cast<std::vector<KBuffer>&>(ev);
            std::vector<KBuffer>::iterator it = bufs.begin();
            while (it != bufs.end())
            {
                it->Release();
                ++it;
            }
        }

        virtual void OnWebsocket(const std::string& msg)// text web socket
        {
            if (IsServer())
            {
                KWebsocketMessage m;
                m.Initialize("xxxx");
                KBuffer resp;
                Serialize(m, resp);
                static int i = 0;
                if (m_base->WriteSocket(m_base->GetSocket(), resp.GetData(), resp.GetSize()) != resp.GetSize())
                    std::cout << "response failed\n";
                else
                    ++i;
                std::cout << "server:" << i << std::endl;
                m.payload.Release();
                resp.Release();
            }
            else
            {
                std::cout << msg << std::endl;
            }
        }

        virtual void OnWebsocket(const std::vector<KBuffer>& msgs) // binary web socket
        {

        }

    private:
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
                            std::vector<KBuffer> msgs;
                            msgs.push_back(partial.payload);
                            OnWebsocket(msgs);
                        }
                        else
                            OnWebsocket(std::string(partial.payload.GetData(), partial.payload.GetSize()));
                        partial.payload.Release();
                        partial.Clear();
                    }
                    else
                    {
                        if (msg.opcode == KWebsocketMessage::opbinary)
                        {
                            std::vector<KBuffer> msgs;
                            msgs.push_back(msg.payload);
                            OnWebsocket(msgs);
                        }
                        else
                            OnWebsocket(std::string(msg.payload.GetData(), msg.payload.GetSize()));
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
                printf("web socket recv close request\n");
                m_base->DeleteSocket(m_base->GetSocket());
                msg.payload.Release();
                break;
            }
            default:
                break;
            }
        }

        void AppendBuffer(KWebsocketMessage& msg, KWebsocketMessage& partial) const
        {
            partial.payload.ApendBuffer(msg.payload.GetData(), msg.payload.GetSize());
            partial.SetPayloadSize(partial.payload.GetSize() + msg.payload.GetSize());
            msg.payload.Release();
        }

    private:
        KWebsocketMessage m_partial;
        std::string m_secKey;// client
    };
};
