#ifndef _DB2CLI_H_
#define _DB2CLI_H_
#ifdef WIN32
#include <windows.h>
#endif // WIN32

#include <sql.h>
#include <sqltypes.h>
#include <sqlext.h>

#include <stdint.h>
#include <string>
#include <vector>
#include <iostream>
#include "thread/KBuffer.h"
#include "thread/KMutex.h"
#include "thread/KLockGuard.h"

namespace klib
{
#if defined(X64)
#define DBCHAR SQLWCHAR
#else
#define DBCHAR SQLCHAR
#endif

    enum DriverType
    {
        null, mysql, db2, oracle, sqlserver, sybase, dbase
    };
    
    // 配置信息
    struct DataBaseConfig
    {
        std::string username;
        std::string passwd;
        std::string dbname;
        std::string host;
        std::string drvname;
        DriverType drvtype;
    };

    struct QueryValue
    {
        enum
        {
            tnull = 0, tbool, tuint8, tint8, tuint16, tint16, tuint32, tint32,
            tuint64, tint64, tfloat, tdouble, tnumeric, tguid, tstring, tbinary,
            tdate, ttime, ttimestamp
        };

        std::string name;
        uint16_t type;
        char* valbuf;
        int bufsize;
        int valsize;
        int precision;
        bool nullable;

        QueryValue()
            :bufsize(-1), nullable(true), type(tnull), precision(0), valbuf(NULL), valsize(-1)
        {

        }

        inline bool IsNull() const { return valsize < 0; }

        void Clone(const QueryValue& r)
        {
            name = r.name;
            type = r.type;
            valbuf = new char[r.bufsize]();
            memmove(valbuf, r.valbuf, r.bufsize);
            bufsize = r.bufsize;
            valsize = r.valsize;
            precision = r.precision;
            nullable = r.nullable;
        }

        void Release()
        {
            if (valbuf)
            {
                delete[] valbuf;
                valbuf = NULL;
            }
        }

        bool StringVal(std::string& val) const
        {
            switch (type)
            {
            case QueryValue::tdate:
                return GetDate(val);
            case QueryValue::ttime:
                return GetTime(val);
            case QueryValue::ttimestamp:
                return GetTimestamp(val);
            case QueryValue::tguid:
                return GetTimestamp(val);
            case QueryValue::tstring:
                return GetStr(val);
            default:
                return false;
            }
        }

        bool DoubleVal(double& val) const
        {
            switch (type)
            {
            case QueryValue::tfloat:
            {
                float fval = 0;
                if (GetFloat(fval))
                {
                    val = fval;
                    return true;
                }
            }
            case QueryValue::tdouble:
                return GetDouble(val);
            case QueryValue::tnumeric:
                return GetNumeric(val);
            default:
                return false;
            }
        }

        bool IntegerVal(int64_t& val) const
        {
            switch (type)
            {
            case QueryValue::tint8:
            case QueryValue::tint16:
            case QueryValue::tint32:
            {
                int32_t tval;
                if (GetInt32(tval))
                {
                    val = tval;
                    return true;
                }
            }
            case QueryValue::tint64:
                return GetInt64(val);
            case QueryValue::tuint8:
            case QueryValue::tuint16:
            case QueryValue::tuint32:
            {
                uint32_t tval;
                if (GetUint32(tval))
                {
                    val = tval;
                    return true;
                }
            }
            case QueryValue::tuint64:
            {
                uint64_t tval;
                if (GetUint64(tval))
                {
                    val = tval;
                    return true;
                }
            }
            default:
                return false;
            }
        }

        bool BinaryVal(char* val) const
        {
            if (QueryValue::tbinary == type)
                return GetBinary(val);
            return false;
        }

        bool GetBool(bool& val) const
        {
            if (IsNull())
                return false;
            if (type != QueryValue::tbool)
                return false;
            val = 0x01 & valbuf[0];
            return true;
        }

        bool GetBinary(char* val) const
        {
            if (IsNull())
                return false;
            if (type != QueryValue::tbinary)
                return false;
            memmove(val, valbuf, valsize);
            return true;
        }

        bool GetStr(std::string &val) const
        {
            if (IsNull())
                return false;
            if (type != QueryValue::tstring)
                return false;
            std::string(valbuf, valsize).swap(val);
            return true;
        }

        bool GetFloat(float &val) const
        {
            if (IsNull())
                return false;
            if (type != QueryValue::tfloat)
                return false;
            val = *(float*)(valbuf);
            return true;
        }

        bool GetDouble(double &val) const
        {
            if (IsNull())
                return false;
            if (type != QueryValue::tdouble)
                return false;
            val = *(double*)(valbuf);
            return true;
        }

        bool GetInt32(int32_t &val) const
        {
            if (IsNull())
                return false;
            switch (type)
            {
            case QueryValue::tint8:
            {
                val = *(int8_t*)(valbuf);
                return true;
            }
            case QueryValue::tint16:
            {
                val = *(int16_t*)(valbuf);
                return true;
            }
            case QueryValue::tint32:
            {
                val = *(int32_t*)(valbuf);
                return true;
            }
            default:
                return false;
            }
        }

        bool GetUint32(uint32_t &val) const
        {
            if (IsNull())
                return false;
            switch (type)
            {
            case QueryValue::tuint8:
            {
                val = *(uint8_t*)(valbuf);
                return true;
            }
            case QueryValue::tuint16:
            {
                val = *(uint16_t*)(valbuf);
                return true;
            }
            case QueryValue::tuint32:
            {
                val = *(uint32_t*)(valbuf);
                return true;
            }
            default:
                return false;
            }
        }

        bool GetInt64(int64_t &val) const
        {
            if (IsNull())
                return false;
            if (type != QueryValue::tint64)
                return false;
            val = *(int64_t*)(valbuf);
            return true;
        }

        bool GetUint64(uint64_t &val) const
        {
            if (IsNull())
                return false;
            if (type != QueryValue::tuint64)
                return false;
            val = *(uint64_t*)(valbuf);
            return true;
        }

        bool GetGuid(std::string& val) const
        {
            if (IsNull())
                return false;
            if (type != QueryValue::tguid)
                return false;
            char buf[128] = { 0 };
            SQLGUID* guid = reinterpret_cast<SQLGUID*>(valbuf);
            sprintf(buf, "%d-%d-%d-%s,", guid->Data1, guid->Data2, guid->Data3, guid->Data4);
            std::string(buf).swap(val);
            return true;
        }

        bool GetDate(std::string& val) const
        {
            if (IsNull())
                return false;
            if (type != QueryValue::tdate)
                return false;
            char buf[128] = { 0 };
            DATE_STRUCT* date = reinterpret_cast<DATE_STRUCT*>(valbuf);
            sprintf(buf, "%04d/%02d/%02d,", date->year, date->month, date->day);
            std::string(buf).swap(val);
            return true;
        }

        bool GetTime(std::string& val) const
        {
            if (IsNull())
                return false;
            if (type != QueryValue::ttime)
                return false;
            char buf[128] = { 0 };
            TIME_STRUCT* time = reinterpret_cast<TIME_STRUCT*>(valbuf);
            sprintf(buf, "%02d:%02d:%02d,", time->hour, time->minute, time->second);
            std::string(buf).swap(val);
            return true;
        }

        bool GetTimestamp(std::string& val) const
        {
            if (IsNull())
                return false;
            if (type != QueryValue::ttimestamp)
                return false;
            char buf[128] = { 0 };
            TIMESTAMP_STRUCT* timestamp = reinterpret_cast<TIMESTAMP_STRUCT*>(valbuf);
            sprintf(buf, "%04d/%02d/%02d %02d:%02d:%02d,", timestamp->year, timestamp->month, timestamp->day,
                timestamp->hour, timestamp->minute, timestamp->second);
            std::string(buf).swap(val);
            return true;
        }

        bool GetNumeric(double& val) const
        {
            if (IsNull())
                return false;
            if (type != QueryValue::tnumeric)
                return false;
            SQL_NUMERIC_STRUCT* numeric = reinterpret_cast<SQL_NUMERIC_STRUCT*>(valbuf);
            long value = 0;
            int lastVal = 1;
            for (int i = 0; i < 16; i++)
            {
                int currentVal = (int)numeric->val[i];
                int lsd = currentVal % 16;
                int msd = currentVal / 16;
                value += lastVal * lsd;
                lastVal = lastVal * 16;
                value += lastVal * msd;
                lastVal = lastVal * 16;
            }
            long divisor = 1;
            for (int i = 0; i < numeric->scale; i++)
                divisor = divisor * 10;

            val = (double)(value / (double)divisor);
        }
    };

    struct QueryRow
    {
        std::vector<QueryValue> dat;

        void Clone(const QueryRow& other)
        {
            std::vector<QueryValue>::const_iterator it = other.dat.begin();
            while (it != other.dat.end())
            {
                QueryValue val;
                val.Clone(*it);
                dat.push_back(val);
                ++it;
            }
        }

        void Release()
        {
            std::vector<QueryValue>::iterator it = dat.begin();
            while (it != dat.end())
            {
                it->Release();
                ++it;
            }
            dat.clear();
        }
    };

    struct FieldDescription;

    class KOdbcSql
    {
    public:
        KOdbcSql(SQLHANDLE stmt)
            :m_stmt(stmt)
        {

        }

        ~KOdbcSql()
        {
            Release();
        }

        bool BindParam(int pCount, ...);

        bool Execute();

        bool Next(QueryRow& row);

    private:
        void Release();

        bool InitializeBuffer(SQLHANDLE stmt, QueryRow& header);

        void DescribeField(const FieldDescription& cd, QueryValue& r);

        SQLSMALLINT SqlTypeToEnum(SQLSMALLINT ct);

        SQLSMALLINT EnumToCType(SQLSMALLINT et);

    private:
        SQLHANDLE m_stmt;
        QueryRow m_buffer;
    };

    class KOdbcClient
    {
    public:
        KOdbcClient(const DataBaseConfig& prop);

        virtual ~KOdbcClient();

        bool Connect(bool autocommit = true);

        void Disconnect();

        KOdbcSql* Prepare(const std::string& sql);

        /*
        Transactions in ODBC do not have to be explicitly initiated. Instead,
        a transaction begins implicitly whenever the application starts operating on the database
        */
        bool Commit();

        bool Rollback();

    private:
        std::string GetConnStr(const DataBaseConfig& prop);

        static bool CheckSqlState(SQLSMALLINT htype, SQLHANDLE handle, SQLRETURN r);

    private:
        DataBaseConfig m_conf;
        // db2 handle
        SQLHDBC   m_hdbc;
        SQLHENV   m_henv;          // Environment handle
        KMutex m_dmtx;
        volatile bool m_autoCommit;
        friend class KOdbcSql;
    };
};
#endif // !_DB2CLI_H_
