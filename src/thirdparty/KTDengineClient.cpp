#include "KTDengineClient.h"
#include <sstream>
namespace thirdparty {
    KTDEngineQuery::KTDEngineQuery(void* stmt, void* res) 
        :m_res(res), m_fields(NULL), m_numFields(0), m_stmt(stmt) 
    { 
        
    }

    KTDEngineQuery::KTDEngineQuery() 
        : m_res(NULL), m_fields(NULL), m_numFields(0), m_stmt(NULL) 
    { 
        
    }

    taosField* KTDEngineQuery::GetFields()
    {
        if (m_res != NULL && m_fields == NULL)
            m_fields = taos_fetch_fields(m_res);
        return m_fields;
    }
    int KTDEngineQuery::GetFieldCount()
    {
        if (m_res != NULL && m_numFields == 0)
            m_numFields = taos_num_fields(m_res);
        return m_numFields;
    }
    bool KTDEngineQuery::BindParams(TAOS_BIND* params)
    {
        if (m_stmt != NULL)
        {
            int rc = taos_stmt_bind_param(m_stmt, params);
            if (rc != 0)
            {
                printf("failed to bind param, error:[%d]\n", rc);
                return false;
            }
            return true;
        }
        return false;
    }
    bool KTDEngineQuery::Exec()
    {
        if (m_stmt == NULL && m_res == NULL)
            return false;
        else if (m_stmt != NULL && m_res != NULL)
            Reset();
        else if (m_stmt == NULL && m_res != NULL)
            return true;

        int rc = taos_stmt_execute(m_stmt);
        if (rc != 0)
        {
            printf("failed to execute statement, error:[%d]\n", rc);
            return false;
        }

        m_res = taos_stmt_use_result(m_stmt);
        rc = taos_errno(m_res);
        if (rc != 0)
        {
            printf("errstr:[%s]\n", taos_errstr(m_res));
            return false;
        }

        return true;
    }
    bool KTDEngineQuery::Next(std::vector<KTDengineValue>& row)
    {
        row.clear();

        if (GetFields() == NULL)
            return false;

        if (GetFieldCount() < 1)
            return false;

        void** rowDat = NULL;
        if (rowDat = taos_fetch_row(m_res))
        {
            for (size_t i = 0; i < m_numFields; i++)
            {
                KTDengineValue value;
                value.type = m_fields[i].type;
                switch (m_fields[i].type)
                {
                case TSDB_DATA_TYPE_TIMESTAMP:
                {
                    value.value.uval = *reinterpret_cast<uint64_t**>(rowDat)[i];
                    break;
                }
                case TSDB_DATA_TYPE_BINARY:
                {
                    value.value.strVal = reinterpret_cast<char**>(rowDat)[i];
                    break;
                }
                case TSDB_DATA_TYPE_NCHAR:
                {
                    value.value.strVal = reinterpret_cast<char**>(rowDat)[i];
                    break;
                }
                case TSDB_DATA_TYPE_FLOAT:
                {
                    value.value.dval = *reinterpret_cast<float**>(rowDat)[i];
                    break;
                }
                case TSDB_DATA_TYPE_DOUBLE:
                {
                    value.value.dval = *reinterpret_cast<double**>(rowDat)[i];
                    break;
                }
                case TSDB_DATA_TYPE_TINYINT:
                {
                    value.value.ival = *reinterpret_cast<int8_t**>(rowDat)[i];
                    break;
                }
                case TSDB_DATA_TYPE_SMALLINT:
                {
                    value.value.ival = *reinterpret_cast<int16_t**>(rowDat)[i];
                    break;
                }
                case TSDB_DATA_TYPE_INT:
                {
                    value.value.ival = *reinterpret_cast<int32_t**>(rowDat)[i];
                    break;
                }
                case TSDB_DATA_TYPE_BIGINT:
                {
                    value.value.ival = *reinterpret_cast<int64_t**>(rowDat)[i];
                    break;
                }
                case TSDB_DATA_TYPE_UTINYINT:
                {
                    value.value.uval = *reinterpret_cast<uint8_t**>(rowDat)[i];
                    break;
                }
                case TSDB_DATA_TYPE_USMALLINT:
                {
                    value.value.uval = *reinterpret_cast<uint16_t**>(rowDat)[i];
                    break;
                }
                case TSDB_DATA_TYPE_UINT:
                {
                    value.value.uval = *reinterpret_cast<uint32_t**>(rowDat)[i];
                    break;
                }
                case TSDB_DATA_TYPE_UBIGINT:
                {
                    value.value.uval = *reinterpret_cast<uint64_t**>(rowDat)[i];
                    break;
                }
                case TSDB_DATA_TYPE_BOOL:
                {
                    value.value.uval = *reinterpret_cast<uint8_t**>(rowDat)[i];
                    break;
                }
                default:
                    break;
                };
                row.push_back(value);
            }
            return true;
        }
        return false;
    }
    void KTDEngineQuery::Release()
    {
        if (m_stmt != NULL)
        {
            taos_stmt_close(m_stmt);
            m_stmt = NULL;
        }
        Reset();
    }
    void KTDEngineQuery::Reset()
    {
        if (m_res != NULL)
        {
            taos_free_result(m_res);
            m_res = NULL;
        }
        m_fields = NULL;
        m_numFields = 0;
    }

    bool KTDEngineClient::Connect(const KTDendgineConfig& conf)
    {
        m_conf = conf;
        taos_options(TSDB_OPTION_TIMEZONE, "GMT-8");

        m_taos = taos_connect(conf.host.c_str(), conf.user.c_str(), conf.passwd.c_str(), NULL, 0);
        if (m_taos == NULL)
        {
            printf("failed to connect to db, reason:[%s]\n", taos_errstr(m_taos));
            return false;
        }
        return true;
    };

    void KTDEngineClient::Close()
    {
        if (m_taos)
        {
            taos_close(m_taos);
            taos_cleanup();
        }
    };

    KTDEngineQuery KTDEngineClient::Prepare(const std::string& sql)
    {
        TAOS_STMT* stmt = taos_stmt_init(m_taos);
        int rc = taos_stmt_prepare(stmt, sql.c_str(), 0);
        if (rc != 0)
        {
            printf("failed to execute taos_stmt_prepare, error:[%d]\n", rc);
            taos_stmt_close(stmt);
            return KTDEngineQuery();
        }

        return KTDEngineQuery(stmt, NULL);
    };

    KTDEngineQuery KTDEngineClient::Select(const std::string& sql)
    {
        TAOS_RES* res = taos_query(m_taos, sql.c_str());

        if (res == NULL)
            return KTDEngineQuery();

        int rc = taos_errno(res);
        if (rc != 0)
        {
            printf("errstr:[%s]\n", taos_errstr(res));
            return KTDEngineQuery();
        }

        return KTDEngineQuery(NULL, res);
    }

    bool KTDEngineClient::Exec(const std::string& sql)
    {
        KTDEngineQuery q = Select(sql);
        if (!q.IsValid())
            return false;
        q.Release();
        return true;
    }

    int KTDEngineClient::BeginContinuousQuery(const std::string& sql, ContinuousQueryCb cb, int64_t stime, void* param)
    {
        TAOS_STREAM * ts = taos_open_stream(m_taos, sql.c_str(), cb, 0, param, ContinuousQueryStopCb);
        if (ts == NULL)
            printf("open stream failed\n");
        return reinterpret_cast<int>(ts);
    }

    void KTDEngineClient::EndContinuousQuery(int id)
    {
        taos_close_stream(reinterpret_cast<TAOS_STREAM*>(id));
    }
    void KTDEngineClient::ContinuousQueryStopCb(void* param)
    {
        printf("continuous query stopped\n");
    }
};