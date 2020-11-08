#ifndef _DB2CLI_H_
#define _DB2CLI_H_

#include <sql.h>
#include <sqltypes.h>
#include <sqlext.h>

#include <stdint.h>
#include <string>
#include <vector>
#include <iostream>
#include "KBuffer.h"
#include "KMutex.h"
#include "KLockGuard.h"


#if defined(X64)
#define DBCHAR SQLWCHAR
#else
#define DBCHAR SQLCHAR
#endif

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
    uint16_t size;
    bool nul;

    QueryValue()
        :size(0), val(), nul(false)
    {

    }
};

typedef std::vector<QueryValue> QueryRow;

struct QueryField
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

    QueryField()
        :bufsize(0), nullable(true), type(tnull), precision(0), valbuf(NULL), valsize(0)
    {

    }
};

typedef std::vector<QueryField> QueryHeader;

struct QueryResult
{
    QueryHeader header;
    std::vector<QueryRow> rows;

    void release()
    {
        std::vector<QueryRow>::iterator it = rows.begin();
        while (it != rows.end())
        {
            for (size_t i = 0; i < header.size(); ++i)
            {
                uint16_t type = header[i].type;
                char* buf = (*it)[i].val.cval;
                switch (type)
                {
                case QueryField::tbool:
                case QueryField::tfloat:
                case QueryField::tdouble:
                case QueryField::tuint8:
                case QueryField::tint8:
                case QueryField::tuint16:
                case QueryField::tint16:
                case QueryField::tuint32:
                case QueryField::tint32:
                case QueryField::tuint64:
                case QueryField::tint64:
                    break;
                default:
                    delete[] buf;
                }
            }
            ++it;
        }

        QueryHeader::iterator hit = header.begin();
        while (hit != header.end())
        {
            char* buf = (*hit).valbuf;
            if (buf)
                delete[] buf;
            ++hit;
        }
    }
};

typedef std::vector<KBuffer> QueryParam;

typedef std::vector<QueryParam> QueryParamSeq;

enum DriverType
{
    null, mysql, db2, oracle, sqlserver, sybase, dbase
};

    // 配置信息
struct DbConfig
{
    std::string username;
    std::string passwd;
    std::string dbname;
    std::string host;
    std::string drvname;
    DriverType drvtype;
};

    // 字段信息
struct FieldDescr
{
    SQLUSMALLINT colnum;
    DBCHAR colname[256];
    SQLSMALLINT namelen;

    SQLSMALLINT sqltype;
    SQLULEN colsize;
    SQLSMALLINT decimaldigits;
    SQLSMALLINT nullable;
};

    //错误信息
struct SqlState
{
    DBCHAR         state[16];
    SQLINTEGER      nativerr;
    DBCHAR         msgtext[SQL_MAX_MESSAGE_LENGTH];
    SQLSMALLINT     msglen;
};

class KOdbcCli
{
public:
    KOdbcCli(const DbConfig &prop)
        :m_conf(prop), m_henv(SQL_NULL_HENV), m_hdbc(SQL_NULL_HDBC), m_autoCommit(true)
    {
        // Create the DB2 environment handle
        SQLRETURN r = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &m_henv);
        if(CheckSqlState(SQL_HANDLE_ENV, m_henv, r))
        {
            // Set attribute to enable application to run as ODBC 3.0 application 
            r = SQLSetEnvAttr(m_henv, SQL_ATTR_ODBC_VERSION, (void*)SQL_OV_ODBC3, 0);
            CheckSqlState(SQL_HANDLE_ENV, m_henv, r);
        }
    }

    virtual ~KOdbcCli()
    {
        //释放资源
        SQLRETURN r = SQLFreeHandle(SQL_HANDLE_ENV, m_henv);
        CheckSqlState(SQL_HANDLE_ENV, m_henv, r);
    }

    bool Connect(bool autocommit = true)
    {
        KLockGuard<KMutex> lock(m_dmtx);
        m_autoCommit = autocommit;
        SQLRETURN r = SQLAllocHandle(SQL_HANDLE_DBC, m_henv, &m_hdbc);
        if (!CheckSqlState(SQL_HANDLE_DBC, m_hdbc, r))
            return false;
        r = SQLSetConnectAttr(m_hdbc, SQL_ATTR_AUTOCOMMIT,
            (SQLPOINTER)(m_autoCommit? SQL_AUTOCOMMIT_ON:SQL_AUTOCOMMIT_OFF), SQL_IS_INTEGER);
        if (!CheckSqlState(SQL_HANDLE_DBC, m_hdbc, r))
            return false;
        r = SQLSetConnectAttr(m_hdbc, SQL_LOGIN_TIMEOUT, (SQLPOINTER)10, 0);
        if (!CheckSqlState(SQL_HANDLE_DBC, m_hdbc, r))
            return false;

        std::string connstr = GetConnStr(m_conf);
        if (connstr.empty())
        {
            fprintf(stderr, "OdbcCli connect failed, null driver\n");
            return false;
        }

        SQLSMALLINT sz = 1024;
        char outstr[1024] = { 0 };
        r = SQLDriverConnect(m_hdbc, NULL, (DBCHAR *)connstr.c_str(), SQL_NTS,
            (DBCHAR *)outstr, sizeof(outstr), &sz, SQL_DRIVER_NOPROMPT);

        /*r = SQLConnect(_hdbc,
            (DBCHAR *)_prop.host.c_str(), SQL_NTS,
            (DBCHAR *)_prop.username.c_str(), SQL_NTS,
            (DBCHAR *)_prop.passwd.c_str(), SQL_NTS);*/

        return CheckSqlState(SQL_HANDLE_DBC, m_hdbc, r);
    }

    void Close()
    {
        KLockGuard<KMutex> lock(m_dmtx);
        SQLRETURN r = SQLDisconnect(m_hdbc);
        CheckSqlState(SQL_HANDLE_DBC, m_hdbc, r);
        r = SQLFreeHandle(SQL_HANDLE_DBC, m_hdbc);
        CheckSqlState(SQL_HANDLE_DBC, m_hdbc, r);
    }

    bool Prepare(SQLHANDLE &stmt, const std::string &sql)
    {
        KLockGuard<KMutex> lock(m_dmtx);
        stmt = SQLHANDLE(SQL_NULL_HSTMT);
        //获取操作句柄
        SQLRETURN r = SQLAllocHandle(SQL_HANDLE_STMT, m_hdbc, &stmt);
        if (!CheckSqlState(SQL_HANDLE_STMT, stmt, r))
            return false;
        r = SQLSetStmtAttr(stmt, SQL_ATTR_USE_BOOKMARKS, (SQLPOINTER)SQL_UB_OFF, SQL_IS_INTEGER);
        if (!CheckSqlState(SQL_HANDLE_STMT, stmt, r))
            return false;
        r = SQLPrepare(stmt, (DBCHAR*)sql.c_str(), SQL_NTS);
        return CheckSqlState(SQL_HANDLE_STMT, stmt, r);
    }

    bool BindParam(SQLHANDLE stmt, std::vector<KBuffer> &paras)
    {
        KLockGuard<KMutex> lock(m_dmtx);
        // Check to see if there are any parameters. If so, process them. 
        SQLSMALLINT numpara = 0;
        SQLRETURN r = SQLNumParams(stmt, &numpara);
        if (!CheckSqlState(SQL_HANDLE_STMT, stmt, r))
            return false;
        if (numpara == paras.size())
        {
            SQLSMALLINT   sqltype, decimaldigits, nullable;
            SQLUINTEGER   colsize;
            int i = 1;
            std::vector<KBuffer>::const_iterator it = paras.begin();
            while (it != paras.end())
            {
                SQLRETURN r = SQLDescribeParam(stmt, i, &sqltype, &colsize, &decimaldigits, &nullable);
                if (!CheckSqlState(SQL_HANDLE_STMT, stmt, r))
                    return false;
                // Bind the memory to the parameter. Assume that we only have input parameters. 
                r = SQLBindParameter(stmt, i, SQL_PARAM_INPUT, EnumToCType(SqlTypeToEnum(sqltype)),
                    sqltype, colsize, decimaldigits, it->GetData(), it->Size(), 0);
                if (!CheckSqlState(SQL_HANDLE_STMT, stmt, r))
                    return false;
                ++i;
                ++it;
            }
            return true;
        }
        return numpara == 0;
    }

    bool Execute(SQLHANDLE stmt)
    {
        KLockGuard<KMutex> lock(m_dmtx);
        SQLRETURN r = SQLExecute(stmt);
        return CheckSqlState(SQL_HANDLE_STMT, stmt, r);
    }

    bool GetResult(SQLHANDLE stmt, QueryResult &qr)
    {
        KLockGuard<KMutex> lock(m_dmtx);
        if (!GetHeader(stmt, qr.header))
        {
            qr.release();
            return false;
        }

        if (qr.header.empty())
        {
            return true;
        }

        while (SQLFetch(stmt) != SQL_NO_DATA)
        {
            ParseRow(qr);
        }
            
        return true;
    }

    void Release(SQLHANDLE stmt)
    {
        KLockGuard<KMutex> lock(m_dmtx);
        SQLRETURN r = SQLCloseCursor(stmt);
        r = SQLCancel(stmt);
        r = SQLFreeStmt(stmt, SQL_CLOSE);
        CheckSqlState(SQL_HANDLE_STMT, stmt, r);
        r = SQLFreeStmt(stmt, SQL_UNBIND);
        CheckSqlState(SQL_HANDLE_STMT, stmt, r);
        r = SQLFreeStmt(stmt, SQL_RESET_PARAMS);
        CheckSqlState(SQL_HANDLE_STMT, stmt, r);
        r = SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        CheckSqlState(SQL_HANDLE_STMT, stmt, r);
    }

    /*
    Transactions in ODBC do not have to be explicitly initiated. Instead,
    a transaction begins implicitly whenever the application starts operating on the database
    */
    bool Commit()
    {
        KLockGuard<KMutex> lock(m_dmtx);
        if (m_autoCommit)
        {
            return true;
        }

        SQLRETURN r = SQLEndTran(SQL_HANDLE_DBC, m_hdbc, SQL_COMMIT);
        return CheckSqlState(SQL_HANDLE_DBC, m_hdbc, r);
    }

    bool Rollback()
    {
        KLockGuard<KMutex> lock(m_dmtx);
        SQLRETURN r = SQLEndTran(SQL_HANDLE_DBC, m_hdbc, SQL_ROLLBACK);
        return CheckSqlState(SQL_HANDLE_DBC, m_hdbc, r);
    }

private:
    std::string GetConnStr(const DbConfig &prop)
    {
        // 使用创建数据源时显示的驱动程序名称（必须完全一致）
        char constr[256] = { 0 };
        switch (prop.drvtype)
        {
        case mysql:
            //MySQL ODBC 8.0 Unicode Driver
            //libmyodbc8w.so
            sprintf(constr, "DRIVER={%s};SERVER=%s;PORT=3306;"
                "DATABASE=%s;USER=%s;PASSWORD=%s;CHARSET=GB18030;OPTION=3;",
                m_conf.drvname.c_str(), m_conf.host.c_str(), m_conf.dbname.c_str(),
                m_conf.username.c_str(), m_conf.passwd.c_str());
            break;
        case db2:
            //IBM DB2 ODBC DRIVER
            //libdb2.so
            sprintf(constr, "driver={%s};hostname=%s;database=%s;"
                "port=50000;protocol=TCPIP;uid=%s;pwd=%s",
                m_conf.drvname.c_str(), m_conf.host.c_str(), m_conf.dbname.c_str(),
                m_conf.username.c_str(), m_conf.passwd.c_str());
            break;
        case oracle:
            /*
            "Driver={%s};"
            "CONNECTSTRING=(DESCRIPTION=(ADDRESS=(PROTOCOL=TCP)(HOST=%s)(PORT=1521))(CONNECT_DATA=(SERVICE_NAME=%s)));"
            "Uid=%s;Pwd=%s;"
            "Driver={%s};"
            "Server=(DESCRIPTION=(ADDRESS=(PROTOCOL=TCP)(HOST=%s)(PORT=1521))(CONNECT_DATA=(SID=%s)));"
            "Uid=%s;Pwd=%s;"
            */
            //Oracle in instantclient_19_5
            //libsqora.so.19.1
            //have to set tnsnames.ora
            sprintf(constr, "DRIVER={%s};"
                "DBQ=%s;DBA=W;"
                "UID=%s;PWD=%s;",
                m_conf.drvname.c_str(), m_conf.dbname.c_str(),
                m_conf.username.c_str(), m_conf.passwd.c_str());
            break;
        case sqlserver:
            sprintf(constr, "Driver={%s};Server=%s;Database=%s;"
                "Uid=%s;Pwd=%s;",
                m_conf.drvname.c_str(), m_conf.host.c_str(), m_conf.dbname.c_str(),
                m_conf.username.c_str(), m_conf.passwd.c_str());
            break;
        case dbase:
            break;
        case sybase:
            sprintf(constr, "Driver={%s};Srvr=%s;Uid=%s;Pwd=%s",
                m_conf.drvname.c_str(), m_conf.host.c_str(),
                m_conf.username.c_str(), m_conf.passwd.c_str());
            break;
        default:
            break;
        }
        return std::string(constr);
    }

    bool CheckSqlState(SQLSMALLINT htype, SQLHANDLE handle, SQLRETURN r)
    {
        if (r == SQL_ERROR || r == SQL_SUCCESS_WITH_INFO)
        {
            SQLLEN numrecs = 0;
            SQLGetDiagField(htype, handle, 0, SQL_DIAG_NUMBER, &numrecs, 0, 0);
            // Get the status records.
            int i = 1;
            SqlState ss;
            SQLRETURN tmp;
            while (i <= numrecs && (tmp = SQLGetDiagRec(htype, handle, i, ss.state, &ss.nativerr,
                ss.msgtext, sizeof(ss.msgtext), &ss.msglen)) != SQL_NO_DATA)
            {
                fprintf(stderr, "checksqlstate error:%s\n", std::string((char *)&ss.msgtext[0], ss.msglen).c_str());
                i++;
            }
        }

        return (r == SQL_SUCCESS || r == SQL_SUCCESS_WITH_INFO);
    }

    bool GetHeader(SQLHANDLE stmt, QueryHeader &head)
    {
        //获取列数
        SQLSMALLINT numcols = 0;
        SQLRETURN r = SQLNumResultCols(stmt, &numcols);
        if (!CheckSqlState(SQL_HANDLE_STMT, stmt, r))
            return false;

        if (numcols == 0)
        {
            return true;
        }

        // 获取字段信息
        head.resize(numcols);
        for (SQLSMALLINT i = 0; i < numcols; i++)
        {
            FieldDescr cd;
            r = SQLDescribeCol(stmt, (SQLSMALLINT)i + 1, cd.colname, sizeof(cd.colname), &cd.namelen,
                &cd.sqltype, &cd.colsize, &cd.decimaldigits, &cd.nullable);
            if (!CheckSqlState(SQL_HANDLE_STMT, stmt, r))
                return false;
            DescribeField(cd, head[i]);
            r = SQLBindCol(stmt, i + 1, EnumToCType(head[i].type), head[i].valbuf, head[i].bufsize, (SQLLEN *)&(head[i].valsize));
            if (!CheckSqlState(SQL_HANDLE_STMT, stmt, r))
                return false;
        }
        return true;
    }

    void DescribeField(const FieldDescr &cd, QueryField &r)
    {
        r.name = std::string((char *)&cd.colname[0], cd.namelen);
        r.precision = cd.decimaldigits;
        r.nullable = (cd.nullable == SQL_NO_NULLS ? false : true);
        r.bufsize = cd.colsize;
        r.valbuf = new char[r.bufsize]();
        r.type = SqlTypeToEnum(cd.sqltype);
    }

    SQLSMALLINT SqlTypeToEnum(SQLSMALLINT ct)
    {
        switch (ct)
        {
        case SQL_BIT:
            return QueryField::tbool;
        case SQL_TINYINT:
            return QueryField::tuint8;
        case SQL_SMALLINT:
            return QueryField::tint16;
        case SQL_INTEGER:
            return QueryField::tint32;
        case SQL_BIGINT:
            return QueryField::tint64;
        case SQL_REAL:
            return QueryField::tfloat;
        case SQL_FLOAT:
        case SQL_DOUBLE:
            return QueryField::tdouble;           
        case SQL_CHAR:
        case SQL_VARCHAR:
        case -9:
            return QueryField::tstring;
        case SQL_LONGVARCHAR:
        case SQL_VARBINARY:
        case SQL_BINARY:
            return QueryField::tbinary;
        case SQL_DECIMAL:
        case SQL_NUMERIC:
            //SQL_NUMERIC_STRUCT
            return QueryField::tnumeric;
        case SQL_GUID:
            //SQLGUID
            return QueryField::tguid;
        case SQL_DATE:
            //DATE_STRUCT
            return QueryField::tdate;
        case SQL_TIME:
            //TIME_STRUCT
            return QueryField::ttime;
        case SQL_TIMESTAMP:
        case 93:
            //TIMESTAMP_STRUCT
            return QueryField::ttimestamp;
        default:
            fprintf(stderr, "sqltype2enum unknown type:%d\n", ct);
            return QueryField::tnull;
        }
    }

    SQLSMALLINT EnumToCType(SQLSMALLINT et)
    {
        switch (et)
        {
        case QueryField::tbool:
            {
                return SQL_C_BIT;
            }
        case QueryField::tuint8:
            {
                return SQL_C_TINYINT;
            }
        case QueryField::tint16:
            {
                return SQL_C_SHORT;
            }
        case QueryField::tint32:
            {
                return SQL_C_LONG;
            }
        case QueryField::tint64:
            {
                return SQL_C_SBIGINT;
            }
        case QueryField::tfloat:
            {
                return SQL_C_FLOAT;
            }
        case QueryField::tdouble:
            {
                return SQL_C_DOUBLE;
            }
        case QueryField::tguid:
            {
                return SQL_C_GUID;
            }
        case QueryField::tnumeric:
            {
                return SQL_C_NUMERIC;
            }
        case QueryField::tstring:
            {
                return SQL_C_CHAR;
            }
        case QueryField::tbinary:
            {
                return SQL_C_BINARY;
            }
        case QueryField::tdate:
            {
                return SQL_C_DATE;
            }
        case QueryField::ttime:
            {
                return SQL_C_TIME;
            }
        case QueryField::ttimestamp:
            {
                return SQL_C_TIMESTAMP;
            }
        default:
            return SQL_C_DEFAULT;
        }
    }

    void ParseRow(QueryResult &qr)
    {
        QueryRow rw;
        QueryHeader::iterator it = qr.header.begin();
        while (it != qr.header.end())
        {
            QueryValue qv;
            if ((qv.size = it->valsize) > 0)
            {
                if (!ParseNumber(qv, it))
                {
                    ParseStruct(qv, it);
                }
            }
            else
            {
                qv.nul = true;
                fprintf(stdout, "null,");
            }
            rw.push_back(qv);
            ++it;
        }
        fprintf(stdout, "\n");
        qr.rows.push_back(rw);
    }

    void ParseStruct(QueryValue &qv, QueryHeader::iterator it)
    {
        qv.val.cval = new char[qv.size]();
        memcpy(qv.val.cval, it->valbuf, qv.size);
        switch (it->type)
        {
        case QueryField::tbinary:
            {
                fprintf(stdout, "binary,");
                break;
            }
        case QueryField::tguid:
            {
                SQLGUID * guid = reinterpret_cast<SQLGUID *>(qv.val.cval);
                fprintf(stdout, "%d-%d-%d-%s,", guid->Data1, guid->Data2, guid->Data3, guid->Data4);
                break;
            }
        case QueryField::tstring:
            {
            fprintf(stdout, "%s,", std::string(qv.val.cval, qv.size).c_str());
                break;
            }
        case QueryField::tnumeric:
            {
                SQL_NUMERIC_STRUCT * numberic = reinterpret_cast<SQL_NUMERIC_STRUCT *>(qv.val.cval);
                if (numberic->scale > 0)
                {
                    fprintf(stdout, "%lf,", *reinterpret_cast<double *>(numberic->val));
                }
                else
                {
                    fprintf(stdout, "%lld,", *reinterpret_cast<int64_t *>(numberic->val));
                }
                break;
            }

        case QueryField::tdate:
            {
                DATE_STRUCT * date = reinterpret_cast<DATE_STRUCT *>(qv.val.cval);
                fprintf(stdout, "%d/%d/%d,", date->year, date->month, date->day);
                break;
            }
        case QueryField::ttime:
            {
                TIME_STRUCT * time = reinterpret_cast<TIME_STRUCT *>(qv.val.cval);
                fprintf(stdout, "%d:%d:%d,", time->hour, time->minute, time->second);
                break;
            }
        case QueryField::ttimestamp:
            {
                TIMESTAMP_STRUCT * timestamp = reinterpret_cast<TIMESTAMP_STRUCT *>(qv.val.cval);
                std::cout << timestamp->year << "/" << timestamp->month << "/" << timestamp->day << " "
                    << timestamp->hour << ":" << timestamp->minute << ":" << timestamp->second << ",";
                break;
            }
        default:
            fprintf(stderr, "type:[%d],", it->type);
            break;
        }
    }

    bool ParseNumber(QueryValue &qv, QueryHeader::iterator it)
    {
        bool ret = true;
        switch (it->type)
        {
        case QueryField::tuint8:
            {
                qv.val.ui8val = *(uint8_t *)(it->valbuf);
                std::cout << qv.val.ui8val << ',';
                break;
            }
        case QueryField::tint8:
            {
                qv.val.ui8val = *(int8_t *)(it->valbuf);
                std::cout << qv.val.i8val << ',';
                break;
            }
        case QueryField::tuint16:
            {
                qv.val.ui16val = *(uint16_t *)(it->valbuf);
                std::cout << qv.val.ui16val << ',';
                break;
            }
        case QueryField::tint16:
            {
                qv.val.i16val = *(int16_t *)(it->valbuf);
                std::cout << qv.val.i16val << ',';
                break;
            }
        case QueryField::tuint32:
            {
                qv.val.ui32val = *(uint32_t *)(it->valbuf);
                std::cout << qv.val.ui32val << ',';
                break;
            }
        case QueryField::tint32:
            {
                qv.val.i32val = *(int32_t *)(it->valbuf);
                std::cout << qv.val.i32val << ',';
                break;
            }
        case QueryField::tuint64:
            {
                qv.val.ui64val = *(uint64_t *)(it->valbuf);
                std::cout << qv.val.ui64val << ',';
                break;
            }
        case QueryField::tint64:
            {
                qv.val.i64val = *(int64_t *)(it->valbuf);
                std::cout << qv.val.i64val << ',';
                break;
            }
        case QueryField::tbool:
            {
                qv.val.bval = 0x01 & it->valbuf[0];
                std::cout << qv.val.bval << ',';
                break;
            }
        case QueryField::tfloat:
            {
                qv.val.fval = *(float *)it->valbuf;
                std::cout << qv.val.dval << ',';
                break;
            }
        case QueryField::tdouble:
            {
                qv.val.dval = *(double *)it->valbuf;
                std::cout << qv.val.dval << ',';
                break;
            }
        default:
            ret = false;
        }
        return ret;
    }

private:
    DbConfig m_conf;
    // db2 handle
    SQLHDBC   m_hdbc;
    SQLHENV   m_henv;          // Environment handle
    KMutex m_dmtx;
    volatile bool m_autoCommit;
};
#endif // !_DB2CLI_H_
