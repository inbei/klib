#include "thirdparty/KRedisClient.h"
#include "util/KStringUtility.h"
namespace thirdparty {
    KRedisClient::KRedisClient()
        :m_redisContext(NULL)
    {

    }

    KRedisClient::~KRedisClient()
    {
        Close();
    }

    bool KRedisClient::Initialize(const std::vector<RedisConfig>& confs)
    {
        m_confs = confs;
        return true;
    }

    bool KRedisClient::CheckConnection(uint16_t times /*= 0*/)
    {
        uint16_t count = times;
        KLockGuard<KMutex> lock(m_redisMutex);
        while (!m_redisContext && !(m_redisContext = Connect(m_currentConf))
            && (times == 0 || (times > 0 && count-- > 0)))
        {
            KTime::MSleep(1000);
        }
        return m_redisContext != NULL;
    }

    bool KRedisClient::Auth()
    {
        std::string cmd("auth ");
        cmd.append(m_currentConf.pwd);
        KLockGuard<KMutex> lock(m_redisMutex);
        return Exec(m_redisContext, cmd);
    }

    bool KRedisClient::Select(uint16_t index)
    {
        std::ostringstream os;
        os << "select " << index;
        KLockGuard<KMutex> lock(m_redisMutex);
        return Exec(m_redisContext, os.str());
    }

    bool KRedisClient::Flushdb()
    {
        KLockGuard<KMutex> lock(m_redisMutex);
        return Exec(m_redisContext, "flushdb");
    }

    bool KRedisClient::Remove(const std::string& pattern)
    {
        std::vector<std::string> kys;
        int rc = Keys(pattern, kys);
        if (rc == valnil || rc == valarray)
        {
            std::vector<std::string>::iterator it = kys.begin();
            while (it != kys.end())
            {
                std::vector<std::string>::iterator last = it;
                size_t step = std::distance(last, kys.end());
                step = (step > RedisBatchSize ? RedisBatchSize : step);
                std::advance(last, step);
                if (Remove(std::vector<std::string>(it, last)) != valint64)
                {
                    return false;
                }
                it = last;
            }
            return true;
        }
        return false;
    }

    bool KRedisClient::Sadd(const std::string& key, const std::set<std::string>& elements)
    {
        std::string cmd("sadd ");
        cmd.append(key);
        std::set<std::string>::const_iterator it = elements.begin();
        while (it != elements.end())
        {
            cmd.push_back(' ');
            cmd.append(*it);
            ++it;
        }

        KLockGuard<KMutex> lock(m_redisMutex);
        if (m_redisContext)
        {
            redisReply* reply = reinterpret_cast<redisReply*>(redisCommand(m_redisContext, cmd.c_str()));
            std::string val;
            int rc = GetResponse(reply, val);
            freeReplyObject(reply);
            return rc == valint64;
        }
        return false;
    }

    bool KRedisClient::Smembers(const std::string& key, std::vector<std::string>& vals)
    {
        std::string cmd("smembers ");
        cmd.append(key);

        KLockGuard<KMutex> lock(m_redisMutex);
        if (m_redisContext)
        {
            redisReply* reply = reinterpret_cast<redisReply*>(redisCommand(m_redisContext, cmd.c_str()));
            int rc = GetResponse(reply, vals);
            freeReplyObject(reply);
            return rc == valarray || valnil == rc;
        }
        return false;
    }

    bool KRedisClient::PipelineCmd(const std::vector<std::string>& cmds, std::vector<redisReply*>& replies)
    {
        KLockGuard<KMutex> lock(m_redisMutex);
        if (m_redisContext)
        {
            std::vector<std::string>::const_iterator it = cmds.begin();
            while (it != cmds.end())
            {
                if (redisAppendCommand(m_redisContext, it->c_str()) != REDIS_OK)
                {
                    Close();
                    return false;
                }
                ++it;
            }

            for (size_t i = 0; i < cmds.size(); ++i)
            {
                redisReply* reply = NULL;
                if (redisGetReply(m_redisContext, (void**)&reply) != REDIS_OK)
                {
                    freeReplyObject(reply);
                    std::vector<redisReply*>::iterator rit = replies.begin();
                    while (rit != replies.end())
                    {
                        freeReplyObject(*rit);
                        ++it;
                    }
                    replies.clear();
                    Close();
                    return false;
                }
                else
                {
                    replies.push_back(reply);
                }
            }
            return !replies.empty();
        }
        return false;
    }

    bool KRedisClient::PipelineCmd(const std::vector<RedisArgvCmd>& cmds, std::vector<redisReply*>& replies)
    {
        KLockGuard<KMutex> lock(m_redisMutex);
        if (m_redisContext)
        {
            std::vector<RedisArgvCmd>::const_iterator it = cmds.begin();
            while (it != cmds.end())
            {
                if (redisAppendCommandArgv(m_redisContext, it->argc, (const char**)it->argv, it->argvlen) != REDIS_OK)
                {
                    Close();
                    return false;
                }
                ++it;
            }

            for (size_t i = 0; i < cmds.size(); ++i)
            {
                redisReply* reply = NULL;
                if (redisGetReply(m_redisContext, (void**)&reply) != REDIS_OK)
                {
                    freeReplyObject(reply);
                    std::vector<redisReply*>::iterator rit = replies.begin();
                    while (rit != replies.end())
                    {
                        freeReplyObject(*rit);
                        ++it;
                    }
                    replies.clear();
                    Close();
                    return false;
                }
                else
                {
                    replies.push_back(reply);
                }
            }
            return !replies.empty();
        }
        return false;
    }

    bool KRedisClient::Mset(const std::map<std::string, std::string>& keyvals)
    {
        std::map<std::string, std::string>::const_iterator it = keyvals.begin();
        while (it != keyvals.end())
        {
            std::map<std::string, std::string>::const_iterator last = it;
            size_t step = std::distance(last, keyvals.end());
            step = (step > RedisBatchSize ? RedisBatchSize : step);
            std::advance(last, step);
            if (MsetInternal(it, last) != valstatus)
            {
                return false;
            }
            it = last;
        }
        return true;
    }

    bool KRedisClient::Mget(const std::vector<std::string>& kys, std::vector<std::string>& vals)
    {
        std::vector<std::string>::const_iterator it = kys.begin();
        while (it != kys.end())
        {
            std::vector<std::string>::const_iterator last = it;
            size_t step = std::distance(last, kys.end());
            step = (step > RedisBatchSize ? RedisBatchSize : step);
            std::advance(last, step);
            if (MgetInternal(std::vector<std::string>(it, last), vals) != valarray)
            {
                return false;
            }
            it = last;
        }
        return kys.size() == vals.size();
    }

    bool KRedisClient::Hmset(const std::string& key, const std::map<std::string, std::string>& fieldValues)
    {
        std::map<std::string, std::string>::const_iterator it = fieldValues.begin();
        while (it != fieldValues.end())
        {
            std::map<std::string, std::string>::const_iterator last = it;
            size_t step = std::distance(last, fieldValues.end());
            step = (step > RedisBatchSize ? RedisBatchSize : step);
            std::advance(last, step);
            if (HmsetInternal(key, it, last) != valstatus)
            {
                return false;
            }
            it = last;
        }
        return true;
    }

    bool KRedisClient::Hmget(const std::string& key, std::vector<std::string>& fields, std::vector<std::string>& values)
    {
        if (fields.empty())
        {
            int rc = HgetAll(key, fields, values);
            return rc == valarray || rc == valnil;
        }
        else
        {
            std::vector<std::string>::iterator it = fields.begin();
            while (it != fields.end())
            {
                std::vector<std::string>::iterator last = it;
                size_t step = std::distance(last, fields.end());
                step = (step > RedisBatchSize ? RedisBatchSize : step);
                std::advance(last, step);
                if (HmgetInternal(key, std::vector<std::string>(it, last), values) != valarray)
                {
                    return false;
                }
                it = last;
            }
            return fields.size() == values.size();
        }
    }

    bool KRedisClient::IsMaster(redisContext* ctx)
    {
        if (ctx)
        {
            redisReply* reply = reinterpret_cast<redisReply*>(redisCommand(ctx, "info replication"));
            std::string resp;
            int rc = GetResponse(reply, resp);
            freeReplyObject(reply);
            if (rc == valstring)
                return resp.find("role:master") != std::string::npos;
        }
        return false;
    }

    int KRedisClient::Keys(const std::string& pattern, std::vector<std::string>& kys)
    {
        std::string cmd("keys ");
        cmd.append(pattern);
        KLockGuard<KMutex> lock(m_redisMutex);
        if (m_redisContext)
        {
            redisReply* reply = reinterpret_cast<redisReply*>(redisCommand(m_redisContext, cmd.c_str()));
            int rc = GetResponse(reply, kys);
            freeReplyObject(reply);
            return rc;
        }
        return valunconnected;
    }

    int KRedisClient::HgetAll(const std::string& key, std::vector<std::string>& fields, std::vector<std::string>& values)
    {
        std::string cmd("hgetall ");
        cmd.append(key);
        KLockGuard<KMutex> lock(m_redisMutex);
        if (m_redisContext)
        {
            redisReply* reply = reinterpret_cast<redisReply*>(redisCommand(m_redisContext, cmd.c_str()));
            std::vector<std::string> vals;
            int rc = GetResponse(reply, vals);
            freeReplyObject(reply);
            if (rc == valarray && (!vals.empty() && vals.size() % 2 == 0))
            {
                std::vector<std::string>::iterator it = vals.begin();
                while (it != vals.end())
                {
                    fields.push_back(*it++);
                    values.push_back(*it++);
                }
            }
            return rc;
        }
        return valunconnected;
    }

    redisContext* KRedisClient::Connect(RedisConfig& conf)
    {
        std::vector<RedisConfig>::iterator it = m_confs.begin();
        while (it != m_confs.end())
        {
            conf = *it;
            timeval tv = { 3, 0 };
            redisContext* ctx = redisConnectWithTimeout(conf.ip.c_str(), conf.port, tv);
            // 连上之后判断是否是master，不是的话再连接其它的
            if (ctx && !ctx->err && IsMaster(ctx))
            {
                return ctx;
            }
            redisFree(ctx);
            ++it;
        }
        return NULL;
    }

    void KRedisClient::Close()
    {
        if (m_redisContext)
        {
            redisFree(m_redisContext);
            m_redisContext = NULL;
        }
    }

    bool KRedisClient::Exec(redisContext* ctx, const std::string& cmd)
    {
        if (ctx)
        {
            redisReply* reply = reinterpret_cast<redisReply*>(redisCommand(ctx, cmd.c_str()));
            std::string val;
            int rc = GetResponse(reply, val);
            freeReplyObject(reply);
            return (val.compare("OK") == 0);
        }
        return false;
    }

    int KRedisClient::Remove(const std::vector<std::string>& kys)
    {
        std::string tkys = KStringUtility::JoinString(kys, " ");
        std::string cmd("del ");
        cmd.append(tkys);
        KLockGuard<KMutex> lock(m_redisMutex);
        if (m_redisContext)
        {
            redisReply* reply = reinterpret_cast<redisReply*>(redisCommand(m_redisContext, cmd.c_str()));
            std::string val;
            int rc = GetResponse(reply, val);
            freeReplyObject(reply);
            return rc;
        }
        return valunconnected;
    }

    int KRedisClient::MgetInternal(const std::vector<std::string>& kys, std::vector<std::string>& vals)
    {
        std::ostringstream os;
        os << "mget";
        std::vector<std::string>::const_iterator it = kys.begin();
        while (it != kys.end())
        {
            os << " " << (it->empty() ? "###" : *it);
            ++it;
        }
        KLockGuard<KMutex> lock(m_redisMutex);
        if (m_redisContext)
        {
            redisReply* reply = reinterpret_cast<redisReply*>(redisCommand(m_redisContext, os.str().c_str()));
            int rc = GetResponse(reply, vals);
            freeReplyObject(reply);
            return rc;
        }
        return valunconnected;
    }

    int KRedisClient::HmgetInternal(const std::string& key, const std::vector<std::string>& kys, std::vector<std::string>& vals)
    {
        std::ostringstream os;
        os << "hmget " << key;
        std::vector<std::string>::const_iterator it = kys.begin();
        while (it != kys.end())
        {
            os << " " << (it->empty() ? "###" : *it);
            ++it;
        }
        std::string cmd = os.str();
        KLockGuard<KMutex> lock(m_redisMutex);
        if (m_redisContext)
        {
            redisReply* reply = reinterpret_cast<redisReply*>(redisCommand(m_redisContext, cmd.c_str()));
            int rc = GetResponse(reply, vals);
            freeReplyObject(reply);
            return rc;
        }
        return valunconnected;
    }

    int KRedisClient::MsetInternal(std::map<std::string, std::string>::const_iterator first, std::map<std::string, std::string>::const_iterator last)
    {
        int argc = std::distance(first, last) * 2 + 1;
        RedisArgvCmd rmd;
        rmd.Allocate(argc);
        rmd.AddArgv("mset", 4);
        std::string cmd("mset");
        std::map<std::string, std::string>::const_iterator it = first;
        while (it != last)
        {
            rmd.AddArgv(it->first);
            rmd.AddArgv(it->second);

            cmd.push_back(' ');
            cmd.append(it->first);
            cmd.push_back(' ');
            cmd.append(it->second);
            ++it;
        }
        KLockGuard<KMutex> lock(m_redisMutex);
        if (m_redisContext)
        {
            redisReply* reply = reinterpret_cast<redisReply*>(redisCommandArgv(m_redisContext, rmd.argc, (const char**)rmd.argv, rmd.argvlen));
            std::string val;
            int rc = GetResponse(reply, val);
            freeReplyObject(reply);
            rmd.Release();
            return rc;
        }
        rmd.Release();
        return valunconnected;
    }

    int KRedisClient::HmsetInternal(const std::string& key, std::map<std::string, std::string>::const_iterator first, std::map<std::string, std::string>::const_iterator last)
    {
        int argc = std::distance(first, last) * 2 + 2;
        RedisArgvCmd rmd;
        rmd.Allocate(argc);
        rmd.AddArgv("hmset", 5);
        rmd.AddArgv(key);

        std::string cmd("hmset");
        cmd.push_back(' ');
        cmd.append(key);

        std::map<std::string, std::string>::const_iterator it = first;
        while (it != last)
        {
            rmd.AddArgv(it->first);
            rmd.AddArgv(it->second);

            cmd.push_back(' ');
            cmd.append(it->first);
            cmd.push_back(' ');
            cmd.append(it->second);
            ++it;
        }
        KLockGuard<KMutex> lock(m_redisMutex);
        if (m_redisContext)
        {
            redisReply* reply = reinterpret_cast<redisReply*>(redisCommandArgv(m_redisContext, rmd.argc, (const char**)rmd.argv, rmd.argvlen));
            std::string val;
            int rc = GetResponse(reply, val);
            freeReplyObject(reply);
            rmd.Release();
            return rc;
        }
        rmd.Release();
        return valunconnected;
    }

    int KRedisClient::GetResponse(redisReply* reply, std::string& val)
    {
        if (!reply)
        {
            Close();
            return valnullreply;
        }

        switch (reply->type)
        {
        case REDIS_REPLY_STRING:
        {
            val.assign(std::string(reply->str, reply->len));
            return valstring;
        }
        case REDIS_REPLY_INTEGER:
        {
            val.assign(KStringUtility::Int64ToString(reply->integer));
            return valint64;
        }
        case REDIS_REPLY_STATUS:
        {
            val.assign(std::string(reply->str, reply->len));
            return valstatus;
        }
        /*case REDIS_REPLY_DOUBLE:
        {
            break;
        }*/
        case REDIS_REPLY_NIL:
        {
            return valnil;
        }
        case REDIS_REPLY_ERROR:
        {
            val.assign(std::string(reply->str, reply->len));
            return valerror;
        }
        default:
            return valunsupport;
        }
    }

    int KRedisClient::GetResponse(redisReply* reply, std::vector<std::string>& vals)
    {
        if (!reply)
        {
            Close();
            return valnullreply;
        }

        if (reply->type == REDIS_REPLY_ARRAY)
        {
            for (size_t i = 0; i < reply->elements; ++i)
            {
                std::string val;
                GetResponse(reply->element[i], val);
                vals.push_back(val);
            }
            return valarray;
        }
        else
        {
            std::string val;
            int rc = GetResponse(reply, val);
            vals.push_back(val);
            return rc;
        }
    }
};
