#pragma once
#ifndef _HIREDIS_HPP_
#define _HIREDIS_HPP_

#include "hiredis/hiredis.h"
#include "thread/KMutex.h"
#include "thread/KLockGuard.h"
#include "util/KStringUtility.h"
#include "util/KTime.h"
#include <string>
#if defined(WIN32)
#include <WS2tcpip.h>
#else
#include <sys/time.h>
#endif
#include <cstdio>
#include <vector>
#include <map>
#include <sstream>
#include <set>
#include <algorithm>

#include "thread/KPthread.h"

namespace thirdparty {
    using namespace klib;
#define  RedisBatchSize 1000
    struct RedisConfig
    {
        std::string ip;
        int port;
        std::string pwd;
    };

    struct RedisArgvCmd
    {
        int argc;
        char** argv;
        size_t* argvlen;

        RedisArgvCmd()
            :argc(0), argv(NULL), argvlen(NULL), m_index(0)
        {

        }

        void Allocate(int c)
        {
            argc = c;
            argv = new char* [argc]();
            argvlen = new size_t[argc]();
        }

        void AddArgv(const char* argvp, size_t argvsz)
        {
            argv[m_index] = const_cast<char*>(argvp);
            argvlen[m_index++] = argvsz;
        }

        void AddArgv(const std::string& argvs)
        {
            argv[m_index] = const_cast<char*>(argvs.c_str());
            argvlen[m_index++] = argvs.size();
        }

        void Release()
        {
            if (argv)
            {
                delete[] argv;
                argv = NULL;
            }

            if (argvlen)
            {
                delete[] argvlen;
                argvlen = NULL;
            }
        }

    private:
        int m_index;
    };

    class KRedisClient
    {
    public:
        enum ValState
        {
            valnil, valint64, valstatus, valdouble, valstring, valbool, valerror, valnullreply, valunsupport, valarray, valunconnected
        };

        KRedisClient();

        virtual ~KRedisClient();

        bool Initialize(const std::vector<RedisConfig>& confs);

        const RedisConfig& GetCurrentConf() const { return m_currentConf; }

        bool CheckConnection(uint16_t times = 0);

        bool Auth();

        bool Select(uint16_t index);

        bool Flushdb();

        bool Remove(const std::string& pattern);

        int Remove(const std::vector<std::string>& kys);

        bool Sadd(const std::string& key, const std::set<std::string>& elements);

        bool Smembers(const std::string& key, std::vector<std::string>& vals);

        bool PipelineCmd(const std::vector<std::string>& cmds, std::vector<redisReply*>& replies);

        bool PipelineCmd(const std::vector<RedisArgvCmd>& cmds, std::vector<redisReply*>& replies);

        bool Mset(const std::map<std::string, std::string>& keyvals);

        bool Mget(const std::vector<std::string>& kys, std::vector<std::string>& vals);

        bool Hmset(const std::string& key, const std::map<std::string, std::string>& fieldValues);

        bool Hmget(const std::string& key, std::vector<std::string>& fields, std::vector<std::string>& values);
        /*
        判断当前连接的redis是否是master
        */
        bool IsMaster(redisContext* ctx);


    private:
        int Keys(const std::string& pattern, std::vector<std::string>& kys);
        redisContext* Connect(RedisConfig& conf);

        void Close();

        bool Exec(redisContext* ctx, const std::string& cmd);

        int MgetInternal(const std::vector<std::string>& kys, std::vector<std::string>& vals);

        int HmgetInternal(const std::string& key, const std::vector<std::string>& kys, std::vector<std::string>& vals);

        int HgetAll(const std::string& key, std::vector<std::string>& fields, std::vector<std::string>& values);

        int MsetInternal(std::map<std::string, std::string>::const_iterator first, std::map<std::string, std::string>::const_iterator last);

        int HmsetInternal(const std::string& key, std::map<std::string, std::string>::const_iterator first, std::map<std::string, std::string>::const_iterator last);

        int GetResponse(redisReply* reply, std::string& val);

        int GetResponse(redisReply* reply, std::vector<std::string>& vals);

    private:
        redisContext* m_redisContext;
        std::vector<RedisConfig> m_confs;
        RedisConfig m_currentConf;
        KMutex m_redisMutex;
    };
};
#endif
