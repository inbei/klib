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
    /*
    struct ContinuousQueryParams
    {
        std::string cqname;
        std::string srcdb;
        std::map<std::string, std::string> aggfuncalias;
        std::string dstmeasurement;
        std::string srcmeasurement;
        std::string groupbyfields;
        std::string interval;
    };*/

    struct QuaryResult
    {
        rapidjson::Value rows;
        rapidjson::Value fields;
        rapidjson::Document doc;
    };

    class KInfluxDbCli
    {
    public:
        KInfluxDbCli()
            :m_retentionpolicy("autogen")
        {

        }

        ~KInfluxDbCli()
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
        //************************************
        // Method:    创建保留策略
        // FullName:  KInfluxdbCli::CreateRetentionPolicy
        // Access:    public 
        // Returns:   bool
        // Qualifier:
        // Parameter: const std::string & retentionpolicy 保存策略名称
        // Parameter: const std::string & duration1 保留时长
        // Parameter: const std::string & sharedduration 文件分片时长
        // Parameter: bool bdefault 是否设置为默认策略
        //************************************
        bool CreateRetentionPolicy(const std::string& retentionpolicy, const std::string& duration1, const std::string& sharedduration, bool bdefault = false);

        void SetRetentionPolicy(const std::string& retentionpolicy);

        /*bool DropRetentionPolicy(const std::string &rpname)
        {
            // DROP RETENTION POLICY <retention_policy_name> ON <database_name>
            std::string resp;
            return Post(_queryurl, std::string("q=DDROP RETENTION POLICY \"" + rpname + "\" ON \"" + _database + "\""), resp);
        }*/

        //************************************
        // Method:    创建连续查询
        // FullName:  KInfluxdbCli::CreateContinuousQuery
        // Access:    public 
        // Returns:   bool
        // Qualifier:
        // Parameter: const std::string & cqname 连续查询名称
        // Parameter: const std::string & interval 查询间隔
        // Parameter: const std::map<std::string, std::string> & aggfuncalias 新字段
        // Parameter: const std::vector<std::string> groupbyfields 分组字段
        //************************************
        bool CreateContinuousQuery(const std::string& cqname, const std::string& interval,
            const std::map<std::string, std::string>& aggfuncalias, const std::vector<std::string> groupbyfields);

        /*bool DropContinuousQuery(const std::string &cqname)
        {
            // DROP CONTINUOUS QUERY <cq_name> ON <database_name>
            std::string resp;
            return Post(_queryurl, std::string("q=DROP CONTINUOUS QUERY \"" + cqname + "\" ON \"" + _database + "\""), resp);
        }	*/

        //************************************
        // Method:    插入POINT
        // FullName:  KInfluxdbCli::InsertPoint
        // Access:    public 
        // Returns:   bool
        // Qualifier:
        // Parameter: const std::string & sql
        //************************************
        bool InsertPoint(const std::string& sql)
        {
            std::string resp;
            return Post(m_writeurl, sql, resp);
        }

        //************************************
        // Method:    批量插入POINT
        // FullName:  KInfluxdbCli::InsertPoint
        // Access:    public 
        // Returns:   bool
        // Qualifier:
        // Parameter: const std::vector<std::string> & sqls
        //************************************
        bool InsertPoint(const std::vector<std::string>& sqls);

        //************************************
        // Method:    查询
        // FullName:  KInfluxdbCli::QueryMesurement
        // Access:    public 
        // Returns:   bool
        // Qualifier:
        // Parameter: const std::string & sql， sql语句
        // Parameter: std::string & tablename，表名
        // Parameter: Json::Value & fields 字段名
        // Parameter: Json::Value & rows 行数据
        //************************************
        bool QueryMesurement(const std::string& sql, std::string& tablename, QuaryResult& qr);

    private:
        void SetEasyOpt(const std::string& url, CURL* curl, std::string* resp) const;

        //************************************
        // Method:    解析influxdb响应内容
        // FullName:  KInfluxdbCli::ParseInfluxdbResponse
        // Access:    private 
        // Returns:   bool
        // Qualifier:
        // Parameter: const std::string & resp
        // Parameter: std::string & tablename
        // Parameter: Json::Value & fields
        // Parameter: Json::Value & rows
        //************************************
        bool ParseInfluxdbResponse(const std::string& resp, std::string& tablename, QuaryResult& qr);

        static size_t WriteCallback(void* data, size_t size, size_t nmemb, void* buffer);

        bool Post(const std::string& url, const std::string& dat, std::string& resp);

    private:
        std::string m_database;
        std::string m_host;
        std::string m_username;
        std::string m_password;

        std::string m_writeurl;
        std::string m_queryurl;
        std::string m_retentionpolicy;
    };
};