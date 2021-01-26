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
#define CharType SQLWCHAR
#else
#define CharType SQLCHAR
#endif

    enum KOdbcType
    {
        null, mysql, db2, oracle, sqlserver, sybase, dbase
    };
    
    // 配置信息
    struct KOdbcConfig
    {
        std::string username;
        std::string passwd;
        std::string dbname;
        std::string host;
        std::string drvname;
        KOdbcType drvtype;
    };

    class KOdbcValue
    {
    public:
        KOdbcValue();

        inline bool IsNull() const { return valsize < 0; }

        void Clone(const KOdbcValue& r);

        void Release();

        bool StringVal(std::string& val) const;

        bool DoubleVal(double& val) const;

        bool IntegerVal(int64_t& val) const;

        bool BinaryVal(char* val) const;


        bool GetBool(bool& val) const;

        bool GetBinary(char* val) const;

        bool GetStr(std::string &val) const;

        bool GetFloat(float &val) const;

        bool GetDouble(double &val) const;

        bool GetInt32(int32_t &val) const;

        bool GetUint32(uint32_t &val) const;

        bool GetInt64(int64_t &val) const;

        bool GetUint64(uint64_t &val) const;

        bool GetGuid(std::string& val) const;

        bool GetDate(std::string& val) const;

        bool GetTime(std::string& val) const;

        bool GetTimestamp(std::string& val) const;     

    private:
        std::string name;
        int16_t ctype;
        int16_t sqltype;
        char* valbuf;
        int bufsize;
        int valsize;
        int precision;
        bool nullable;
        friend class KOdbcSql;
    };

    class KOdbcRow:public std::vector<KOdbcValue>
    {
    public:
        void Clone(const KOdbcRow& other);
        void Release();

    private:
        friend class KOdbcSql;
    };

    struct KOdbcField;

    class KOdbcSql
    {
    public:
        KOdbcSql(SQLHANDLE stmt);

        ~KOdbcSql()
        {
            Release();
        }

        inline bool IsValid() const { return m_stmt != SQL_NULL_HSTMT; }

        // %d int32_t， %lld int64_t， %s string %f float %llf  double  %c char * %u uint32_t %llu uint64_t//
        bool BindParam(const char *fmt, ...);

        bool Execute();

        bool Next();

        inline const KOdbcRow& GetRow() { return m_buffer; }

    private:
        void Release();

        bool DescribeHeader(SQLHANDLE stmt, KOdbcRow& buffer);

        void DescribeField(const KOdbcField& cd, KOdbcValue& r);

        SQLSMALLINT GetCType(SQLSMALLINT sqlType, bool bsigned = false);

        SQLSMALLINT GetSqlType(SQLSMALLINT ctype, bool& bsigned);

        void CleanParams();


    private:
        SQLHANDLE m_stmt;
        KOdbcRow m_buffer;
        std::map<SQLSMALLINT, SQLSMALLINT> m_sqlType2cType;
        std::map<SQLSMALLINT, SQLSMALLINT> m_cType2sqlType;
        std::vector<KBuffer> m_paraBufs;
    };

    class KOdbcClient
    {
    public:
        KOdbcClient(const KOdbcConfig& prop);

        virtual ~KOdbcClient();

        bool Connect(bool autocommit = true);

        void Disconnect();

        KOdbcSql Prepare(const std::string &sql);

        /*
        Transactions in ODBC do not have to be explicitly initiated. Instead,
        a transaction begins implicitly whenever the application starts operating on the database
        */
        bool Commit();

        bool Rollback();

    private:
        std::string GetConnStr(const KOdbcConfig& prop);

        static bool CheckSqlState(SQLSMALLINT htype, SQLHANDLE handle, SQLRETURN r);

    private:
        KOdbcConfig m_conf;
        // db2 handle
        SQLHDBC   m_hdbc;
        SQLHENV   m_henv;          // Environment handle
        KMutex m_dmtx;
        volatile bool m_autoCommit;
        friend class KOdbcSql;
    };
};
#endif // !_DB2CLI_H_
