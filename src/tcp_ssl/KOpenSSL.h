#ifndef __KOPENSSL__
#define __KOPENSSL__
#include "openssl/ssl.h"
#include "openssl/err.h"
#include "openssl/ssl3.h"
#include <string>
#include "util/KTime.h"
#include "thread/KBuffer.h"
#include <vector>
#define SSLBlockSize 40960
namespace klib
{
    struct KOpenSSLConfig
    {
        std::string caFile;
        std::string certFile;
        std::string privateKeyFile;
    };

    class KOpenSSL
    {
    public:
        static bool CreateCtx(bool isServer, const KOpenSSLConfig &conf, SSL_CTX** ctx);

        static void DestroyCtx(SSL_CTX** ctx);

        static SSL* Accept(int fd, SSL_CTX* ctx);

        static SSL* Connect(int fd, SSL_CTX* ctx);

        static void Disconnect(SSL** ssl);

        static int ReadSocket(SSL* ssl, std::vector<KBuffer>& dat);

        static int WriteSocket(SSL* ssl, const char* dat, size_t sz);
    };

};

#endif