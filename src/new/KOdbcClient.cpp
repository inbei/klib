#include "new/KOdbcClient.h"
namespace klib
{
    // 字段信息
    struct FieldDescription
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

    bool KOdbcSql::BindParam(int pCount, ...)
    {
        SQLHANDLE& stmt = m_stmt;
        // Check to see if there are any parameters. If so, process them. 
        SQLSMALLINT numPara = 0;
        SQLRETURN r = SQLNumParams(stmt, &numPara);
        if (!KOdbcClient::CheckSqlState(SQL_HANDLE_STMT, stmt, r))
            return false;
        SQLSMALLINT   sqltype, decimaldigits, nullable;
        SQLUINTEGER   colsize;
        int i = 1;
        va_list args;
        va_start(args, pCount);
        for (; i <= numPara; ++i)
        {
            SQLRETURN r = SQLDescribeParam(stmt, i, &sqltype, &colsize, &decimaldigits, &nullable);
            if (!KOdbcClient::CheckSqlState(SQL_HANDLE_STMT, stmt, r))
                break;
            // Bind the memory to the parameter. Assume that we only have input parameters. 
            char* src = va_arg(args, char*);
            uint16_t sz = va_arg(args, uint16_t);
            r = SQLBindParameter(stmt, i, SQL_PARAM_INPUT, EnumToCType(SqlTypeToEnum(sqltype)),
                sqltype, colsize, decimaldigits, src, sz, 0);
            if (!KOdbcClient::CheckSqlState(SQL_HANDLE_STMT, stmt, r))
                break;
        }
        va_end(args);
        return numPara + 1 == i;
    }

    bool KOdbcSql::Execute()
    {
        SQLHANDLE& stmt = m_stmt;
        SQLRETURN r = SQLExecute(stmt);
        return KOdbcClient::CheckSqlState(SQL_HANDLE_STMT, stmt, r) && InitializeBuffer(stmt, m_buffer);
    }

    bool KOdbcSql::Next(QueryRow& rw)
    {
        if (m_buffer.dat.empty())
            return false;

        if (SQLFetch(m_stmt) == SQL_NO_DATA)
            return false;

        rw.Clone(m_buffer);
        return true;
    }

    void KOdbcSql::Release()
    {
        SQLHANDLE& stmt = m_stmt;
        SQLRETURN r = SQLCloseCursor(stmt);
        r = SQLCancel(stmt);
        r = SQLFreeStmt(stmt, SQL_CLOSE);
        KOdbcClient::CheckSqlState(SQL_HANDLE_STMT, stmt, r);
        r = SQLFreeStmt(stmt, SQL_UNBIND);
        KOdbcClient::CheckSqlState(SQL_HANDLE_STMT, stmt, r);
        r = SQLFreeStmt(stmt, SQL_RESET_PARAMS);
        KOdbcClient::CheckSqlState(SQL_HANDLE_STMT, stmt, r);
        r = SQLFreeHandle(SQL_HANDLE_STMT, stmt);
        KOdbcClient::CheckSqlState(SQL_HANDLE_STMT, stmt, r);
        m_buffer.Release();
    }

    bool KOdbcSql::InitializeBuffer(SQLHANDLE stmt, QueryRow& buf)
    {
        //获取列数
        SQLSMALLINT numCols = 0;
        SQLRETURN r = SQLNumResultCols(stmt, &numCols);
        if (!KOdbcClient::CheckSqlState(SQL_HANDLE_STMT, stmt, r))
            return false;

        if (numCols == 0)
            return true;

        // 获取字段信息
        buf.dat.clear();
        buf.dat.resize(numCols);
        for (SQLSMALLINT i = 0; i < numCols; i++)
        {
            FieldDescription cd;
            r = SQLDescribeCol(stmt, (SQLSMALLINT)i + 1, cd.colname, sizeof(cd.colname), &cd.namelen,
                &cd.sqltype, &cd.colsize, &cd.decimaldigits, &cd.nullable);
            if (!KOdbcClient::CheckSqlState(SQL_HANDLE_STMT, stmt, r))
                return false;
            DescribeField(cd, buf.dat[i]);
            r = SQLBindCol(stmt, i + 1, EnumToCType(buf.dat[i].type), buf.dat[i].valbuf, 
                buf.dat[i].bufsize, (SQLLEN*)&(buf.dat[i].valsize));
            if (!KOdbcClient::CheckSqlState(SQL_HANDLE_STMT, stmt, r))
                return false;
        }
        return true;
    }

    void KOdbcSql::DescribeField(const FieldDescription& cd, QueryValue& r)
    {
        r.name = std::string((char*)&cd.colname[0], cd.namelen);
        r.precision = cd.decimaldigits;
        r.nullable = (cd.nullable == SQL_NO_NULLS ? false : true);
        r.bufsize = cd.colsize;
        r.valbuf = new char[r.bufsize]();
        r.type = SqlTypeToEnum(cd.sqltype);
    }

    SQLSMALLINT KOdbcSql::SqlTypeToEnum(SQLSMALLINT ct)
    {
        switch (ct)
        {
        case SQL_BIT:
            return QueryValue::tbool;
        case SQL_TINYINT:
            return QueryValue::tuint8;
        case SQL_SMALLINT:
            return QueryValue::tint16;
        case SQL_INTEGER:
            return QueryValue::tint32;
        case SQL_BIGINT:
            return QueryValue::tint64;
        case SQL_REAL:
            return QueryValue::tfloat;
        case SQL_FLOAT:
        case SQL_DOUBLE:
            return QueryValue::tdouble;
        case SQL_CHAR:
        case SQL_VARCHAR:
        case -9:
            return QueryValue::tstring;
        case SQL_LONGVARCHAR:
        case SQL_VARBINARY:
        case SQL_BINARY:
            return QueryValue::tbinary;
        case SQL_DECIMAL:
        case SQL_NUMERIC:
            //SQL_NUMERIC_STRUCT
            return QueryValue::tnumeric;
        case SQL_GUID:
            //SQLGUID
            return QueryValue::tguid;
        case SQL_DATE:
            //DATE_STRUCT
            return QueryValue::tdate;
        case SQL_TIME:
            //TIME_STRUCT
            return QueryValue::ttime;
        case 91://mysql
        case SQL_TIMESTAMP:
        case 93:
            //TIMESTAMP_STRUCT
            return QueryValue::ttimestamp;
        default:
            fprintf(stderr, "sqltype2enum unknown type:%d\n", ct);
            return QueryValue::tnull;
        }
    }

    SQLSMALLINT KOdbcSql::EnumToCType(SQLSMALLINT et)
    {
        switch (et)
        {
        case QueryValue::tbool:
            return SQL_C_BIT;
        case QueryValue::tuint8:
            return SQL_C_TINYINT;
        case QueryValue::tint16:
            return SQL_C_SHORT;
        case QueryValue::tint32:
            return SQL_C_LONG;
        case QueryValue::tint64:
            return SQL_C_SBIGINT;
        case QueryValue::tfloat:
            return SQL_C_FLOAT;
        case QueryValue::tdouble:
            return SQL_C_DOUBLE;
        case QueryValue::tguid:
            return SQL_C_GUID;
        case QueryValue::tnumeric:
            return SQL_C_NUMERIC;
        case QueryValue::tstring:
            return SQL_C_CHAR;
        case QueryValue::tbinary:
            return SQL_C_BINARY;
        case QueryValue::tdate:
            return SQL_C_DATE;
        case QueryValue::ttime:
            return SQL_C_TIME;
        case QueryValue::ttimestamp:
            return SQL_C_TIMESTAMP;
        default:
            return SQL_C_DEFAULT;
        }
    }

    KOdbcClient::KOdbcClient(const DataBaseConfig& prop) :m_conf(prop), m_henv(SQL_NULL_HENV), m_hdbc(SQL_NULL_HDBC), m_autoCommit(true)
    {
        // Create the DB2 environment handle
        SQLRETURN r = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &m_henv);
        if (CheckSqlState(SQL_HANDLE_ENV, m_henv, r))
        {
            // Set attribute to enable application to run as ODBC 3.0 application 
            r = SQLSetEnvAttr(m_henv, SQL_ATTR_ODBC_VERSION, (void*)SQL_OV_ODBC3, 0);
            CheckSqlState(SQL_HANDLE_ENV, m_henv, r);
        }
    }

    KOdbcClient::~KOdbcClient()
    {
        //释放资源
        SQLRETURN r = SQLFreeHandle(SQL_HANDLE_ENV, m_henv);
        CheckSqlState(SQL_HANDLE_ENV, m_henv, r);
    }

    bool KOdbcClient::Connect(bool autocommit /*= true*/)
    {
        KLockGuard<KMutex> lock(m_dmtx);
        m_autoCommit = autocommit;
        SQLRETURN r = SQLAllocHandle(SQL_HANDLE_DBC, m_henv, &m_hdbc);
        if (!CheckSqlState(SQL_HANDLE_DBC, m_hdbc, r))
            return false;
        r = SQLSetConnectAttr(m_hdbc, SQL_ATTR_AUTOCOMMIT,
            (SQLPOINTER)(m_autoCommit ? SQL_AUTOCOMMIT_ON : SQL_AUTOCOMMIT_OFF), SQL_IS_INTEGER);
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
        r = SQLDriverConnect(m_hdbc, NULL, (DBCHAR*)connstr.c_str(), SQL_NTS,
            (DBCHAR*)outstr, sizeof(outstr), &sz, SQL_DRIVER_NOPROMPT);

        /*r = SQLConnect(_hdbc,
            (DBCHAR *)_prop.host.c_str(), SQL_NTS,
            (DBCHAR *)_prop.username.c_str(), SQL_NTS,
            (DBCHAR *)_prop.passwd.c_str(), SQL_NTS);*/

        return CheckSqlState(SQL_HANDLE_DBC, m_hdbc, r);
    }

    void KOdbcClient::Disconnect()
    {
        KLockGuard<KMutex> lock(m_dmtx);
        SQLRETURN r = SQLDisconnect(m_hdbc);
        CheckSqlState(SQL_HANDLE_DBC, m_hdbc, r);
        r = SQLFreeHandle(SQL_HANDLE_DBC, m_hdbc);
        CheckSqlState(SQL_HANDLE_DBC, m_hdbc, r);
    }

    KOdbcSql* KOdbcClient::Prepare(const std::string& sql)
    {
        KLockGuard<KMutex> lock(m_dmtx);
        SQLHANDLE stmt = SQLHANDLE(SQL_NULL_HSTMT);
        //获取操作句柄
        SQLRETURN r = SQLAllocHandle(SQL_HANDLE_STMT, m_hdbc, &stmt);
        if (!CheckSqlState(SQL_HANDLE_STMT, stmt, r))
            return NULL;
        r = SQLSetStmtAttr(stmt, SQL_ATTR_USE_BOOKMARKS, (SQLPOINTER)SQL_UB_OFF, SQL_IS_INTEGER);
        if (!CheckSqlState(SQL_HANDLE_STMT, stmt, r))
            return NULL;
        r = SQLPrepare(stmt, (DBCHAR*)sql.c_str(), SQL_NTS);
        if (CheckSqlState(SQL_HANDLE_STMT, stmt, r))
            return new KOdbcSql(stmt);
        return NULL;
    }

    bool KOdbcClient::Commit()
    {
        KLockGuard<KMutex> lock(m_dmtx);
        if (m_autoCommit)
            return true;

        SQLRETURN r = SQLEndTran(SQL_HANDLE_DBC, m_hdbc, SQL_COMMIT);
        return CheckSqlState(SQL_HANDLE_DBC, m_hdbc, r);
    }

    bool KOdbcClient::Rollback()
    {
        KLockGuard<KMutex> lock(m_dmtx);
        SQLRETURN r = SQLEndTran(SQL_HANDLE_DBC, m_hdbc, SQL_ROLLBACK);
        return CheckSqlState(SQL_HANDLE_DBC, m_hdbc, r);
    }

    std::string KOdbcClient::GetConnStr(const DataBaseConfig& prop)
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

    bool KOdbcClient::CheckSqlState(SQLSMALLINT htype, SQLHANDLE handle, SQLRETURN r)
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
                fprintf(stderr, "check sql state error:%s\n", std::string((char*)&ss.msgtext[0], ss.msglen).c_str());
                i++;
            }
        }

        return (r == SQL_SUCCESS || r == SQL_SUCCESS_WITH_INFO);
    }
};