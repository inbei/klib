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

    struct QueryField
    {
        enum
        {
            tnull = 0, tbool, tuint8, tint8, tuint16, tint16, tuint32, tint32,
            tuint64, tint64, tfloat, tdouble, tnumeric, tguid, tstring, tbinary,
            tdate, ttime, ttimestamp, tstringnum
        };

        std::string name;
        uint16_t type;
        char* valbuf;
        int bufsize;
        int valsize;
        int precision;
        bool nullable;

        QueryField()
            :bufsize(0), nullable(true), type(tnull), precision(0), valbuf(NULL), valsize(0)
        {

        }
    };

    typedef std::vector<QueryField> QueryHeader;

    struct QueryValue
    {
        union
        {
            uint8_t ui8val;
            int8_t i8val;
            uint16_t ui16val;
            int16_t i16val;
            uint32_t ui32val;
            int32_t i32val;
            uint64_t ui64val;
            int64_t i64val;
            float fval;
            double dval;
            char* cval;
            bool bval;
        } val;
        int16_t size;
        bool nul;

        QueryValue()
            :size(0), val(), nul(false)
        {

        }
    };

    typedef std::vector<QueryValue> QueryRow;

    struct QueryResult
    {
        QueryHeader header;
        std::vector<QueryRow> rows;

        void Release();
    };

    typedef std::vector<KBuffer> QueryParam;

    typedef std::vector<QueryParam> QueryParamSeq;

    //struct QueryResult;
    struct FieldDescr;
    //struct QueryField;
    //struct QueryValue;

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

        bool BindParam(const QueryParam& paras);

        bool Execute();

        bool GetResult(QueryResult& qr);

        static void Clear(QueryParam& param);

        static void Clear(QueryParamSeq& params);

    private:
        void Release();

        bool GetHeader(SQLHANDLE stmt, QueryHeader& head);

        void DescribeField(const FieldDescr& cd, QueryField& r);

        SQLSMALLINT SqlTypeToEnum(SQLSMALLINT ct);

        SQLSMALLINT EnumToCType(SQLSMALLINT et);

        void ParseRow(QueryResult& qr);

        void ParseStruct(QueryValue& qv, const QueryHeader::iterator& it);

        double GetDoubleFromHexStruct(SQL_NUMERIC_STRUCT& numeric);

        bool ParseNumber(QueryValue& qv, const QueryHeader::iterator& it);

    private:
        SQLHANDLE m_stmt;
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
