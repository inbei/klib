#include "thirdparty/KInfluxDbClient.h"

namespace thirdparty {
    void KInfluxDbClient::Initialize(const std::string& host, const std::string& database, const std::string& username, const std::string& password)
    {
        m_host = host;
        m_database = database;
        m_username = username;
        m_password = password;

        m_queryurl = "http://" + m_host + ":8086/query?u=" + m_username + "&p=" + m_password + "&db=" + m_database + "&rp=" + m_retentionpolicy;
        m_writeurl = "http://" + m_host + ":8086/write?u=" + m_username + "&p=" + m_password + "&db=" + m_database + "&rp=" + m_retentionpolicy;
    }

    bool KInfluxDbClient::Ping()
    {
        std::string pingurl = "http://" + m_host + ":8086/ping?verbose=true";
        std::string resp;
        return Post(pingurl, std::string(), resp);
    }

    std::string KInfluxDbClient::DumpToInfluxdbPoint(const std::string& tablename, const std::map<std::string, std::string>& tags, const std::map<std::string, std::string>& numfields, const std::map<std::string, std::string>& strfields)
    {
        // table,tag1="1",tag2="2" field1=1,field2=2
        // table name
        std::string s(tablename);
        if (!tags.empty())
        {
            s.push_back(',');
        }

        // tags
        {
            std::map<std::string, std::string>::const_iterator sit = tags.begin();
            while (sit != tags.end())
            {
                s.append(sit->first + "=\"" + sit->second + "\",");
                ++sit;
            }
            s.resize(s.length() - 1);//去掉多余的逗号
        }
        if (!numfields.empty() || !strfields.empty())
        {
            s.push_back(' ');
        }
        // num fields
        {
            std::map<std::string, std::string>::const_iterator nit = numfields.begin();
            while (nit != numfields.end())
            {
                s.append(nit->first + "=" + nit->second + ",");
                ++nit;
            }
            s.resize(s.length() - 1);
        }

        // str fields
        {
            std::map<std::string, std::string>::const_iterator nit = strfields.begin();
            while (nit != strfields.end())
            {
                s.append(nit->first + "=\"" + nit->second + "\",");
                ++nit;
            }
            s.resize(s.length() - 1);
        }
        return s;
    }

    bool KInfluxDbClient::CreateDB(const std::string& database)
    {
        std::string resp;
        return Post("http://" + m_host + ":8086/query?u=" + m_username + "&p=" + m_password, std::string("q=create database ") + database, resp);
    }

    bool KInfluxDbClient::CreateRetentionPolicy(const std::string& retentionpolicy, const std::string& duration1, const std::string& sharedduration, bool bdefault /*= false*/)
    {
        //SHOW RETENTION POLICIES ON mics
        std::string resp;
        if (Post(m_queryurl, "q=SHOW RETENTION POLICIES ON " + m_database, resp))
        {
            std::string tablename;
            QuaryResult qr;
            if (ParseInfluxdbResponse(resp, tablename, qr))
            {
                for (size_t rw = 0; qr.rows.IsArray() && rw < qr.rows.Size(); ++rw)
                {
                    const rapidjson::Value& row = qr.rows[rw];
                    if (row.IsArray() && retentionpolicy.compare(row[0].GetString()) == 0)
                        return true;
                }

                resp.clear();
                char buf[512] = { 0 };
#ifdef WIN32
                sprintf_s(buf, "q=CREATE RETENTION POLICY %s ON %s DURATION %s REPLICATION 1 SHARD DURATION %s %s",
#else
                sprintf(buf, "q=CREATE RETENTION POLICY %s ON %s DURATION %s REPLICATION 1 SHARD DURATION %s %s",
#endif
                    retentionpolicy.c_str(), m_database.c_str(), duration1.c_str(), sharedduration.c_str(), (bdefault ? "DEFAULT" : ""));
                std::cout << buf << std::endl;
                if (Post(m_queryurl, buf, resp))
                {
                    return true;
                }
            }
        }
        return false;
    }

    bool KInfluxDbClient::CreateContinuousQuery(const std::string& cqname, const std::string& interval, const std::map<std::string, std::string>& aggfuncalias, const std::vector<std::string> groupbyfields)
    {
        std::string resp;
        std::string qcq("q=SHOW CONTINUOUS QUERIES");
        if (Post(m_queryurl, qcq, resp))
        {
            std::string tablename;
            QuaryResult qr;
            if (ParseInfluxdbResponse(resp, tablename, qr))
            {
                for (size_t rw = 0; qr.rows.IsArray() && rw < qr.rows.Size(); ++rw)
                {
                    const rapidjson::Value& row = qr.rows[rw];
                    if (row.IsArray() && cqname.compare(row[0].GetString()) == 0)
                        return true;
                }

                resp.clear();
                std::string dstdb = m_database + "_" + interval;
                if (!CreateDB(dstdb))
                {
                    return false;
                }

                std::string fields;
                {
                    std::map<std::string, std::string>::const_iterator it = aggfuncalias.begin();
                    while (it != aggfuncalias.end())
                    {
                        fields.append(it->first);
                        fields.append(" as ");
                        fields.append(it->second);
                        if (++it != aggfuncalias.end())
                            fields.push_back(',');
                    }
                }

                std::string groupbystr;
                {
                    std::vector<std::string>::const_iterator it = groupbyfields.begin();
                    while (it != groupbyfields.end())
                    {
                        groupbystr.append(*it);
                        groupbystr.push_back(',');
                        ++it;
                    }
                }

                char buf[1024] = { 0 };
#ifdef WIN32
                sprintf_s(buf, "CREATE CONTINUOUS QUERY %s ON %s BEGIN SELECT %s INTO %s.autogen.:MEASUREMENT FROM /.*/ GROUP BY %stime(%s) END",
#else
                sprintf(buf, "CREATE CONTINUOUS QUERY %s ON %s BEGIN SELECT %s INTO %s.autogen.:MEASUREMENT FROM /.*/ GROUP BY %stime(%s) END",
#endif
                    cqname.c_str(), m_database.c_str(), fields.c_str(), dstdb.c_str(), groupbystr.c_str(), interval.c_str());
                if (Post(m_queryurl, std::string("q=") + buf, resp))
                {
                    return true;
                }
            }
        }
        return false;
    }

    bool KInfluxDbClient::InsertPoint(const std::vector<std::string>& sqls)
    {
        std::string composesql;
        std::vector<std::string>::const_iterator it = sqls.begin();
        while (it != sqls.end())
        {
            composesql.append(*it);
            composesql.push_back('\n');
            ++it;
        }
        return InsertPoint(composesql);
    }

    bool KInfluxDbClient::QueryMesurement(const std::string& sql, std::string& tablename, QuaryResult& qr)
    {
        std::string resp;
        if (Post(m_queryurl, std::string("q=") + sql, resp))
        {
            return ParseInfluxdbResponse(resp, tablename, qr);
        }
        return false;
    }

    void KInfluxDbClient::SetEasyOpt(const std::string& url, CURL* curl, std::string* resp) const
    {
        // set opts  
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str()); // url  
        //curl_easy_setopt(curl, CURLOPT_PRIVATE, url.c_str());
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, false); // if want to use https  
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, false); // set peer and host verify false  
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 0); // debug switch
        curl_easy_setopt(curl, CURLOPT_READFUNCTION, NULL);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, resp);
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 6);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 6);

        //curl_slist *headers = NULL;
        //headers = curl_slist_append(headers, "Content-Type:application/octet-stream; charset=UTF-8");
        //headers = curl_slist_append(headers, "Content-Type:text/plain; charset=UTF-8");
        //curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    }

    bool KInfluxDbClient::ParseInfluxdbResponse(const std::string& resp, std::string& tablename, QuaryResult& qr)
    {
        rapidjson::Document doc;
        doc.Parse(resp);
        if (!doc.HasParseError() && doc.HasMember("results"))
        {
            const rapidjson::Value& rval = doc["results"];
            if (rval.IsArray())
            {
                const rapidjson::Value& sval = rval[rval.Size() - 1];
                if (sval.HasMember("series"))
                {
                    const rapidjson::Value& series = sval["series"];
                    if (series.IsArray())
                    {
                        const rapidjson::Value& last = series[series.Size() - 1];
                        if (last.HasMember("name"))
                        {
                            tablename = last["name"].GetString();
                        }
                        qr.fields.CopyFrom(last["columns"], qr.doc.GetAllocator());
                        if (last.HasMember("values"))
                        {
                            qr.rows.CopyFrom(last["values"], qr.doc.GetAllocator());
                        }
                        return true;
                    }
                }
            }
        }
        return false;
    }

    size_t KInfluxDbClient::WriteCallback(void* data, size_t size, size_t nmemb, void* buffer)
    {
        std::string* resp = static_cast<std::string*>(buffer);
        size_t sz = size * nmemb;
        if (resp)
        {
            resp->append(std::string((char*)data, sz));
        }
        return sz;
    }

    bool KInfluxDbClient::Post(const std::string& url, const std::string& dat, std::string& resp)
    {
        long rc = 0;
        try
        {
            CURL* curl = curl_easy_init();
            if (curl)
            {
                curl_easy_setopt(curl, CURLOPT_POSTFIELDS, dat.c_str()); // data  
                curl_easy_setopt(curl, CURLOPT_POST, 1); // post req 
                SetEasyOpt(url, curl, &resp);
                CURLcode cc;
                if (CURLE_OK == (cc = curl_easy_perform(curl)))
                {
                    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &rc);
                }
                curl_easy_cleanup(curl);
            }
        }
        catch (const std::exception& e)
        {
            printf("KInfluxdbCli exception:[%s]\n", e.what());
        }
        return (rc / 100 == 2);
    }

    void KInfluxDbClient::SetRetentionPolicy(const std::string& retentionpolicy)
    {
        m_retentionpolicy = retentionpolicy;
        m_queryurl = "http://" + m_host + ":8086/query?u=" + m_username + "&p=" + m_password + "&db=" + m_database + "&rp=" + m_retentionpolicy;
        m_writeurl = "http://" + m_host + ":8086/write?u=" + m_username + "&p=" + m_password + "&db=" + m_database + "&rp=" + m_retentionpolicy;
    }
};
