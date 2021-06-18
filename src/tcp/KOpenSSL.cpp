#ifdef __OPEN_SSL__
#include "tcp/KOpenSSL.h"
#include "tcp/KTcpConnection.hpp"
namespace klib
{

    bool KOpenSSL::CreateCtx(bool isServer, const KOpenSSLConfig& conf, SSL_CTX** ctx)
    {
        /* SSL 库初始化 */
        SSL_library_init();

        /* 载入所有SSL 算法 */
        OpenSSL_add_all_algorithms();

        /* 载入所有SSL 错误消息 */
        SSL_load_error_strings();

        /* 以SSL V2 和V3 标准兼容方式产生一个SSL_CTX ，即SSL Content Text */
        if (isServer)
            *ctx = SSL_CTX_new(SSLv23_server_method());
        else
            *ctx = SSL_CTX_new(SSLv23_client_method());

        /* 也可以用SSLv2_server_method() 或SSLv3_server_method() 单独表示V2 或V3
         * 标准 */
        if (*ctx == NULL)
        {
            printf("<%s> %s\n", __FUNCTION__, ERR_error_string(ERR_get_error(), NULL));
            return false;
        }

        /* 验证与否 */
        SSL_CTX_set_verify(*ctx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, NULL);

        /* 若验证,则放置CA证书 */
        SSL_CTX_load_verify_locations(*ctx, conf.caFile.c_str(), NULL);

        /* 载入用户的数字证书， 此证书用来发送给客户端。证书里包含有公钥 */
        if (SSL_CTX_use_certificate_file(*ctx, conf.certFile.c_str(), SSL_FILETYPE_PEM) <= 0)
        {
            printf("<%s> %s\n", __FUNCTION__, ERR_error_string(ERR_get_error(), NULL));
            return false;
        }

        /* 载入用户私钥 */
        if (SSL_CTX_use_PrivateKey_file(*ctx, conf.privateKeyFile.c_str(), SSL_FILETYPE_PEM) <= 0)
        {
            printf("<%s> %s\n", __FUNCTION__, ERR_error_string(ERR_get_error(), NULL));
            return false;
        }

        /* 检查用户私钥是否正确 */
        if (!SSL_CTX_check_private_key(*ctx))
        {
            printf("<%s> %s\n", __FUNCTION__, ERR_error_string(ERR_get_error(), NULL));
            return false;
        }

        SSL_CTX_set_cipher_list(*ctx, "RC4-MD5");
        return true;
    }

    void KOpenSSL::DestroyCtx(SSL_CTX** ctx)
    {
        if (*ctx)
        {
            /* 释放CTX */
            SSL_CTX_free(*ctx);
            *ctx = NULL;
        }
    }

    SSL* KOpenSSL::Accept(int fd, SSL_CTX* ctx)
    {
        /* 基于ctx 产生一个新的SSL */
        SSL* ssl = SSL_new(ctx);

        /* 将连接用户的socket 加入到SSL */
        SSL_set_fd(ssl, fd);

        //SSL_set_accept_state(ssl);

        while (true)
        {
            int rc;
            if ((rc = SSL_accept(ssl)) != 1)
            {
                int err = SSL_get_error(ssl, rc);
                if ((err == SSL_ERROR_WANT_WRITE) || (err == SSL_ERROR_WANT_READ))
                {
                    KTime::MSleep(2);
                    continue;
                }
                else
                {
                    SSL_free(ssl);
                    return NULL;
                }
            }
            else
            {
                X509* cert = SSL_get_peer_certificate(ssl);
                if (SSL_get_verify_result(ssl) == X509_V_OK)
                {
                    printf("certificate is authorized\n");
                }

                if (cert != NULL)
                {
                    printf("certificate: %s\n", X509_NAME_oneline(X509_get_subject_name(cert), 0, 0));
                    printf("licensor: %s\n", X509_NAME_oneline(X509_get_issuer_name(cert), 0, 0));
                    X509_free(cert);
                }
                else
                    printf("no certificate\n");
                return ssl;
            };
        };
    }

    SSL* KOpenSSL::Connect(int fd, SSL_CTX* ctx)
    {
        /* 基于ctx 产生一个新的SSL */
        SSL* ssl = SSL_new(ctx);

        /* 将连接用户的socket 加入到SSL */
        SSL_set_fd(ssl, fd);

        //SSL_set_connect_state(ssl);

        while (true)
        {
            int rc;
            if ((rc = SSL_connect(ssl)) != 1)
            {
                int err = SSL_get_error(ssl, rc);
                if ((err == SSL_ERROR_WANT_WRITE) || (err == SSL_ERROR_WANT_READ))
                {
                    KTime::MSleep(2);
                    continue;
                }
                else
                {
                    SSL_free(ssl);
                    return NULL;
                }
            }
            else
            {
                X509* cert = SSL_get_peer_certificate(ssl);
                if (SSL_get_verify_result(ssl) == X509_V_OK)
                {
                    printf("certificate is authorized\n");
                }

                if (cert != NULL)
                {
                    printf("certificate: %s\n", X509_NAME_oneline(X509_get_subject_name(cert), 0, 0));
                    printf("licensor: %s\n", X509_NAME_oneline(X509_get_issuer_name(cert), 0, 0));
                    X509_free(cert);
                }
                else
                    printf("no certificate\n");
                return ssl;
            };
        };
    }

    void KOpenSSL::Disconnect(SSL** ssl)
    {
        if (*ssl)
        {
            SSL_shutdown(*ssl);
            SSL_free(*ssl);
            *ssl = NULL;
        }
    }

    int KOpenSSL::ReadSocket(SSL* ssl, std::vector<KBuffer>& dat)
    {
        if (ssl == NULL)
            return 0;

        int bytes = 0;
        char buf[SSLBlockSize] = { 0 };
        while (true)
        {
            int rc = SSL_read(ssl, buf, SSLBlockSize);
            if (rc > 0)
            {
                KBuffer b(rc);
                b.ApendBuffer(buf, rc);
                dat.push_back(b);
                bytes += rc;
            }
            else
            {
                int err = SSL_get_error(ssl, rc);
                if (SSL_ERROR_WANT_READ == err
                    || SSL_ERROR_NONE == err)
                {
                    break;
                }
                else
                {
                    printf("ReadSocket rc:[%d] err:[%d]\n", rc, err);
                    return -1;
                }
            }
        };
        return bytes;
    }

    int KOpenSSL::WriteSocket(SSL* ssl, const char* dat, size_t sz)
    {
        if (sz < 1 || dat == NULL || ssl == NULL)
            return 0;

        int sent = 0;
        int count = 0;
        while (sent != sz)
        {
            int rc = SSL_write(ssl, (void*)(dat + sent), sz - sent);
            if (rc > 0)
                sent += rc;
            else
            {
                int err = SSL_get_error(ssl, rc);
                
                if (SSL_ERROR_WANT_WRITE == err
                    || SSL_ERROR_NONE == err)
                {
                    KTime::MSleep(6);
                    continue;
                }
                else
                {
                    printf("WriteSocket rc:[%d] err:[%d]\n", rc, err);
                    return -1;
                }
            }
        }
        return sent;
    }

};

#endif