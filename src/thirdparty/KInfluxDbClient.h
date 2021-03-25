#pragma once

#include <curl/curl.h>
#include <sstream>
#include <iostream>
#include <cstdio>
#include <vector>
#include <map>
#include <stdint.h>
#include "util/KTime.h"

#define RAPIDJSON_HAS_STDSTRING 1
#include "rapidjson/rapidjson.h"
#include "rapidjson/document.h"
#include "rapidjson/writer.h"

namespace thirdparty {
    using namespace klib;

    struct Measurement
    {
        std::string database;
        std::string rp;
        std::string measurement;
    };

    struct ContinuousQueryParams
    {
        std::string cqname;
        Measurement src;
        std::map<std::string, std::string> alias;
        Measurement dst;
        std::vector<std::string> groupFields; // 分组字段 //
        std::string interval;
        std::string condition;
    };

    struct RetentionPolicyParams
    {
        std::string rp; // 保存策略名称 //
        std::string duration; // 保留时长 //
        std::string sharedDuration; // 文件分片时长 //
        std::string database;
        bool defaultRp; // 是否设置为默认策略 //
        bool enable;
        RetentionPolicyParams()
            :defaultRp(true), enable(true)
        {

        }
    };

    struct QueryResult
    {
        rapidjson::Value rows;
        rapidjson::Value fields;
        rapidjson::Document doc;
    };

    class KInfluxDbClient
    {
    public:
        KInfluxDbClient()
            :m_rp("autogen")
        {

        }

        void Initialize(const std::string& host, const std::string& database, const std::string& username, const std::string& password);

        bool Ping();

        std::string DumpToInfluxdbPoint(const std::string& tablename,
            const std::map<std::string, std::string>& tags,
            const std::map<std::string, std::string>& numfields,
            const std::map<std::string, std::string>& strfields);

        bool CreateDB(const std::string& database);

        // m:minute, h:hour, d:day, w:week, for example: 30w means 30 weeks
        bool CreateRetentionPolicy(const RetentionPolicyParams &p);

        void UpdateUrl(const std::string& rp, const std::string& database);

        bool DropRetentionPolicy(const std::string &rp, const std::string &database);

        bool CreateContinuousQuery(const ContinuousQueryParams &p);

        bool DropContinuousQuery(const std::string &cq, const std::string &database);   

        bool InsertPoint(const std::string& sql);

        bool InsertPoint(const std::vector<std::string>& sqls);

        bool QueryMesurement(const std::string& sql, std::string& tablename, QueryResult& qr);

    private:
        void SetEasyOpt(const std::string& url, CURL* curl, std::string* resp) const;

        bool ParseResponse(const std::string& resp, std::string& tablename, QueryResult& qr);

        static size_t WriteCallback(void* data, size_t size, size_t nmemb, void* buffer);

        bool Post(const std::string& url, const std::string& dat, std::string& resp);

    private:
        std::string m_database;
        std::string m_host;
        std::string m_username;
        std::string m_password;

        std::string m_writeurl;
        std::string m_queryurl;
        std::string m_rp;
    };
};
