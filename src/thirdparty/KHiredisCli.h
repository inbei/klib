#pragma once
#ifndef _HIREDIS_HPP_
#define _HIREDIS_HPP_

#include "hiredis/hiredis.h"
#include "KMutex.h"
#include "KLockGuard.h"
#include "KStringUtility.h"
#include "KTime.h"
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

#include "KPthread.h"

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

    class KHiredisCli
    {
    public:
        enum ValState
        {
            valnil, valint64, valstatus, valdouble, valstring, valbool, valerror, valnullreply, valunsupport, valarray, valunconnected
        };

        KHiredisCli();

        virtual ~KHiredisCli();

        bool Initialize(const std::vector<RedisConfig>& confs);
        const RedisConfig& GetCurrentConf() const { return m_currentConf; }

        bool CheckConnection(uint16_t times = 0);

        bool Auth();

        bool Select(uint16_t index);

        bool Flushdb();

        bool Remove(const std::string& pattern);

        /************************************
        * Description: 批量删除key
        * Method:    Remove
        * FullName:  KHiredisCli::Remove
        * Access:    private
        * Returns:   int
        * Qualifier:
        * Parameter: const std::vector<std::string> & kys 要删除的key
        ************************************/
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
        /************************************
        * Description: 根据模式获取key
        * Method:    Keys
        * FullName:  KHiredisCli::Keys
        * Access:    public
        * Returns:   bool
        * Qualifier:
        * Parameter: const std::string & pattern key模式
        * Parameter: std::vector<std::string> & kys 根据pattern得到的key
        ************************************/
        int Keys(const std::string& pattern, std::vector<std::string>& kys);

        /************************************
        * Description: 连接redis
        * Method:    Connect
        * FullName:  KHiredisCli::Connect
        * Access:    private
        * Returns:   redisContext *
        * Qualifier:
        * Parameter: RedisConfig & conf redis的配置
        ************************************/
        redisContext* Connect(RedisConfig& conf);

        void Close();

        /************************************
        * Description: 执行命令
        * Method:    Exec
        * FullName:  KHiredisCli::Exec
        * Access:    private
        * Returns:   bool
        * Qualifier:
        * Parameter: redisContext * ctx redis的context
        * Parameter: const std::string & cmd 要执行的命令
        ************************************/
        bool Exec(redisContext* ctx, const std::string& cmd);

        /************************************
        * Description: 批量获取key
        * Method:    MgetInternal
        * FullName:  KHiredisCli::MgetInternal
        * Access:    private
        * Returns:   int
        * Qualifier:
        * Parameter: const std::vector<std::string> & kys key的集合
        * Parameter: std::vector<std::string> & vals 根据key获取的值
        ************************************/
        int MgetInternal(const std::vector<std::string>& kys, std::vector<std::string>& vals);

        int HmgetInternal(const std::string& key, const std::vector<std::string>& kys, std::vector<std::string>& vals);

        int HgetAll(const std::string& key, std::vector<std::string>& fields, std::vector<std::string>& values);

        /************************************
        * Description: 批量写入key
        * Method:    MsetInternal
        * FullName:  KHiredisCli::MsetInternal
        * Access:    private
        * Returns:   int
        * Qualifier:
        * Parameter: std::map<std::string, std::string>::const_iterator first 要写入的第一个元素
        * Parameter: std::map<std::string, std::string>::const_iterator last 要写入的最后一个元素
        ************************************/
        int MsetInternal(std::map<std::string, std::string>::const_iterator first, std::map<std::string, std::string>::const_iterator last);

        int HmsetInternal(const std::string& key, std::map<std::string, std::string>::const_iterator first, std::map<std::string, std::string>::const_iterator last);

        /************************************
        * Description: 获取非数组的返回值
        * Method:    GetResponse
        * FullName:  KHiredisCli::GetResponse
        * Access:    private
        * Returns:   int
        * Qualifier:
        * Parameter: redisReply * reply redis返回的句柄
        * Parameter: std::string & val 获取的值
        ************************************/
        int GetResponse(redisReply* reply, std::string& val);

        /************************************
        * Description: 获取字符串数组的返回值
        * Method:    GetResponse
        * FullName:  KHiredisCli::GetResponse
        * Access:    private
        * Returns:   int
        * Qualifier:
        * Parameter: redisReply * reply redis返回的句柄
        * Parameter: std::vector<std::string> & vals 获取的值
        ************************************/
        int GetResponse(redisReply* reply, std::vector<std::string>& vals);

    private:
        redisContext* m_redisContext;
        std::vector<RedisConfig> m_confs;
        RedisConfig m_currentConf;
        KMutex m_redisMutex;
    };
};
#endif
