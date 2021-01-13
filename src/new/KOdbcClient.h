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
#include <map>
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
        std::string name;
        uint16_t ctype;
        char* valbuf;
        int bufsize;
        int valsize;
        int precision;
        bool nullable;

        QueryValue()
            :bufsize(-1), nullable(true), ctype(SQL_C_DEFAULT), precision(0), valbuf(NULL), valsize(-1)
        {

        }

        inline bool IsNull() const { return valsize < 0; }

        void Clone(const QueryValue& r)
        {
            name = r.name;
            ctype = r.ctype;
            bufsize = r.bufsize;
            valsize = r.valsize;
            if (valsize > 0 && r.valbuf != NULL)
            {
                valbuf = new char[valsize]();
                memmove(valbuf, r.valbuf, valsize);
            }
            else
            {
                valbuf = NULL;
            }
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
            switch (ctype)
            {
            case SQL_C_TYPE_DATE:
                return GetDate(val);
            case SQL_C_TYPE_TIME:
                return GetTime(val);
            case SQL_C_TYPE_TIMESTAMP:
                return GetTimestamp(val);
            case SQL_C_GUID:
                return GetTimestamp(val);
            case SQL_C_CHAR:
                return GetStr(val);
            default:
                return false;
            }
        }

        bool DoubleVal(double& val) const
        {
            if (IsNull())
                return false;
            switch (ctype)
            {
            case SQL_C_FLOAT:
            {
                val = *(float*)(valbuf);
                return true;
            }
            case SQL_C_DOUBLE:
            {
                val = *(double*)(valbuf);
                return true;
            }
            default:
                return false;
            }
        }

        bool IntegerVal(int64_t& val) const
        {
            if (IsNull())
                return false;
            switch (ctype)
            {
            case SQL_C_STINYINT:
            {
                val = *(int8_t*)(valbuf);
                return true;
            }
            case SQL_C_SSHORT:
            {
                val = *(int16_t*)(valbuf);
                return true;
            }
            case SQL_C_SLONG:
            {
                val = *(int32_t*)(valbuf);
                return true;
            }
            case SQL_C_SBIGINT:
            {
                val = *(int64_t*)(valbuf);
                return true;
            }
            case SQL_C_UTINYINT:
            {
                val = *(uint8_t*)(valbuf);
                return true;
            }
            case SQL_C_USHORT:
            {
                val = *(uint16_t*)(valbuf);
                return true;
            }
            case SQL_C_ULONG:
            {
                val = *(uint32_t*)(valbuf);
                return true;
            }
            case SQL_C_UBIGINT:
            {
                val = *(uint64_t*)(valbuf);
                return true;
            }
            default:
                return false;
            }
        }

        bool BinaryVal(char* val) const
        {
            return GetBinary(val);
        }

        bool GetBool(bool& val) const
        {
            if (IsNull())
                return false;
            if (ctype != SQL_C_BIT)
                return false;
            val = 0x01 & valbuf[0];
            return true;
        }

        bool GetBinary(char* val) const
        {
            if (IsNull())
                return false;
            if (ctype != SQL_C_BINARY)
                return false;
            memmove(val, valbuf, valsize);
            return true;
        }

        bool GetStr(std::string &val) const
        {
            if (IsNull())
                return false;
            if (ctype != SQL_C_CHAR)
                return false;
            std::string(valbuf, valsize).swap(val);
            return true;
        }

        bool GetFloat(float &val) const
        {
            if (IsNull())
                return false;
            if (ctype != SQL_C_FLOAT)
                return false;
            val = *(float*)(valbuf);
            return true;
        }

        bool GetDouble(double &val) const
        {
            if (IsNull())
                return false;
            if (ctype != SQL_C_DOUBLE)
                return false;
            val = *(double*)(valbuf);
            return true;
        }

        bool GetInt32(int32_t &val) const
        {
            if (IsNull())
                return false;
            switch (ctype)
            {
            case SQL_C_STINYINT:
            {
                val = *(int8_t*)(valbuf);
                return true;
            }
            case SQL_C_SSHORT:
            {
                val = *(int16_t*)(valbuf);
                return true;
            }
            case SQL_C_SLONG:
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
            switch (ctype)
            {
            case SQL_C_UTINYINT:
            {
                val = *(uint8_t*)(valbuf);
                return true;
            }
            case SQL_C_USHORT:
            {
                val = *(uint16_t*)(valbuf);
                return true;
            }
            case SQL_C_ULONG:
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
            if (ctype != SQL_C_SBIGINT)
                return false;
            val = *(int64_t*)(valbuf);
            return true;
        }

        bool GetUint64(uint64_t &val) const
        {
            if (IsNull())
                return false;
            if (ctype != SQL_C_UBIGINT)
                return false;
            val = *(uint64_t*)(valbuf);
            return true;
        }

        bool GetGuid(std::string& val) const
        {
            if (IsNull())
                return false;
            if (ctype != SQL_C_GUID)
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
            if (ctype != SQL_C_TYPE_DATE)
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
            if (ctype != SQL_C_TYPE_TIME)
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
            if (ctype != SQL_C_TYPE_TIMESTAMP)
                return false;
            char buf[128] = { 0 };
            TIMESTAMP_STRUCT* timestamp = reinterpret_cast<TIMESTAMP_STRUCT*>(valbuf);
            sprintf(buf, "%04d/%02d/%02d %02d:%02d:%02d,", timestamp->year, timestamp->month, timestamp->day,
                timestamp->hour, timestamp->minute, timestamp->second);
            std::string(buf).swap(val);
            return true;
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
        KOdbcSql(SQLHANDLE stmt);

        

        ~KOdbcSql()
        {
            Release();
        }

        // %d int32_t， %lld int64_t， %s string %f float %llf  double  %c char *//
        bool BindParam(const char *fmt, ...);

        bool Execute();

        bool Next();

        inline const QueryRow& GetRow() { return m_buffer; }

    private:
        void Release();

        bool InitializeBuffer(SQLHANDLE stmt, QueryRow& buffer);

        void DescribeField(const FieldDescription& cd, QueryValue& r);

        SQLSMALLINT GetCType(SQLSMALLINT sqlType, bool bsigned = false);

        SQLSMALLINT GetSqlType(SQLSMALLINT ctype, bool& bsigned);


    private:
        SQLHANDLE m_stmt;
        QueryRow m_buffer;
        std::map<SQLSMALLINT, SQLSMALLINT> m_sqlType2cType;
        std::map<SQLSMALLINT, SQLSMALLINT> m_cType2sqlType;
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
