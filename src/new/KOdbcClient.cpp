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

    KOdbcSql::KOdbcSql(SQLHANDLE stmt) 
        :m_stmt(stmt)
    {
        m_sqlType2cType[SQL_CHAR] = SQL_C_CHAR;
        m_sqlType2cType[SQL_VARCHAR] = SQL_C_CHAR;
        m_sqlType2cType[SQL_LONGVARCHAR] = SQL_C_CHAR;
        m_sqlType2cType[SQL_NUMERIC] = SQL_C_CHAR;
        m_sqlType2cType[SQL_DECIMAL] = SQL_C_CHAR;
        m_sqlType2cType[SQL_WCHAR] = SQL_C_WCHAR;
        m_sqlType2cType[SQL_WVARCHAR] = SQL_C_WCHAR;
        m_sqlType2cType[SQL_WLONGVARCHAR] = SQL_C_WCHAR;
        m_sqlType2cType[SQL_BIT] = SQL_C_BIT;
        m_sqlType2cType[SQL_REAL] = SQL_C_FLOAT;
        m_sqlType2cType[SQL_GUID] = SQL_C_GUID;
        m_sqlType2cType[SQL_FLOAT] = SQL_C_DOUBLE;
        m_sqlType2cType[SQL_DOUBLE] = SQL_C_DOUBLE;
        m_sqlType2cType[SQL_BINARY] = SQL_C_BINARY;
        m_sqlType2cType[SQL_VARBINARY] = SQL_C_BINARY;
        m_sqlType2cType[SQL_LONGVARBINARY] = SQL_C_BINARY;
        m_sqlType2cType[SQL_TYPE_DATE] = SQL_C_TYPE_DATE;
        m_sqlType2cType[SQL_TYPE_TIME] = SQL_C_TYPE_TIME;
        m_sqlType2cType[SQL_TYPE_TIMESTAMP] = SQL_C_TYPE_TIMESTAMP;

        m_cType2sqlType[SQL_C_CHAR] = SQL_CHAR;
        m_cType2sqlType[SQL_C_WCHAR] = SQL_WCHAR;
        m_cType2sqlType[SQL_C_BIT] = SQL_BIT;
        m_cType2sqlType[SQL_C_FLOAT] = SQL_REAL;
        m_cType2sqlType[SQL_C_DOUBLE] = SQL_DOUBLE;
        m_cType2sqlType[SQL_C_BINARY] = SQL_BINARY;
        m_cType2sqlType[SQL_C_GUID] = SQL_GUID;
    }

    bool KOdbcSql::BindParam(const char* fmt, ...)
    {
        static const std::string longfmt("ld");
        static const std::string doublefmt("lf");
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
        va_start(args, fmt);
        int pos = 0;
        size_t slen = strlen(fmt);
        for (; i <= numPara && pos < slen; ++i)
        {
            SQLRETURN r = SQLDescribeParam(stmt, i, &sqltype, &colsize, &decimaldigits, &nullable);
            if (!KOdbcClient::CheckSqlState(SQL_HANDLE_STMT, stmt, r))
                break;
            // Bind the memory to the parameter. Assume that we only have input parameters. 
            while (pos < slen && fmt[pos] == ' ') ++pos;
            if (!(pos < slen && fmt[pos++] == '%'))break;
            // %d int32_t， %lld int64_t， %s string %f float %llf  double  %c char *//
            switch (fmt[pos++])
            {
            case 'd':
            {
                r = SQLBindParameter(stmt, i, SQL_PARAM_INPUT, SQL_C_LONG/*EnumToCType(SqlTypeToEnum(sqltype))*/,
                    SQL_INTEGER, colsize, decimaldigits, (char*)&va_arg(args, int32_t), sizeof(int32_t), 0);
                break;
            }
            case 'f':
            {
                r = SQLBindParameter(stmt, i, SQL_PARAM_INPUT, GetCType(sqltype),
                    sqltype, colsize, decimaldigits, (char*)&va_arg(args, float), sizeof(float), 0);
                break;
            }
            case 'l':
            {
                if (pos + 2 > slen) goto end;
                std::string flag = std::string(fmt + pos, 2);
                if (flag == longfmt)
                {
                    r = SQLBindParameter(stmt, i, SQL_PARAM_INPUT, GetCType(sqltype),
                        sqltype, colsize, decimaldigits, (char*)&va_arg(args, int64_t), sizeof(int64_t), 0);
                }
                else if (flag == doublefmt)
                {
                    r = SQLBindParameter(stmt, i, SQL_PARAM_INPUT, GetCType(sqltype),
                        sqltype, colsize, decimaldigits, (char*)&va_arg(args, double), sizeof(double), 0);
                }
                else
                {
                    goto end;
                }
                pos += 2;
                break;
            }
            case 'c':
            {
                char* c = va_arg(args, char*);
                r = SQLBindParameter(stmt, i, SQL_PARAM_INPUT, GetCType(sqltype),
                    sqltype, colsize, decimaldigits, c, strlen(c), 0);
                break;
            }
            case 's':
            {
                const std::string& s = va_arg(args, std::string);
                r = SQLBindParameter(stmt, i, SQL_PARAM_INPUT, GetCType(sqltype),
                    sqltype, colsize, decimaldigits, const_cast<char *>(s.c_str()), s.size(), 0);
                break;
            }
            default:
                goto end;
            }
            if (!KOdbcClient::CheckSqlState(SQL_HANDLE_STMT, stmt, r))
                break;
        }

        end:
        va_end(args);
        return numPara + 1 == i;
    }

    bool KOdbcSql::Execute()
    {
        SQLHANDLE& stmt = m_stmt;
        SQLRETURN r = SQLExecute(stmt);
        return KOdbcClient::CheckSqlState(SQL_HANDLE_STMT, stmt, r) && InitializeBuffer(stmt, m_buffer);
    }

    bool KOdbcSql::Next()
    {
        if (m_buffer.dat.empty())
            return false;
        return (SQLFetch(m_stmt) != SQL_NO_DATA);
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
            r = SQLBindCol(stmt, i + 1, buf.dat[i].ctype, buf.dat[i].valbuf,
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
        r.ctype = GetCType(cd.sqltype);
    }

    SQLSMALLINT KOdbcSql::GetCType(SQLSMALLINT sqlType, bool bsigned /*= false*/)
    {
        std::map<SQLSMALLINT, SQLSMALLINT>::const_iterator it = m_sqlType2cType.find(sqlType);
        if (it != m_sqlType2cType.end())
            return it->second;
        switch (sqlType)
        {
        case SQL_TINYINT:
            return bsigned ? SQL_C_STINYINT : SQL_C_UTINYINT;
        case SQL_SMALLINT:
            return bsigned ? SQL_C_SSHORT : SQL_C_USHORT;
        case SQL_INTEGER:
            return bsigned ? SQL_C_SLONG : SQL_C_ULONG;
        case SQL_BIGINT:
            return bsigned ? SQL_C_SBIGINT : SQL_C_UBIGINT;
        default:
            return SQL_C_DEFAULT;
        }
    }

    SQLSMALLINT KOdbcSql::GetSqlType(SQLSMALLINT ctype, bool& bsigned)
    {
        std::map<SQLSMALLINT, SQLSMALLINT>::const_iterator it = m_cType2sqlType.find(ctype);
        if (it != m_cType2sqlType.end())
            return it->second;
        switch (ctype)
        {
        case SQL_C_STINYINT:
        {
            bsigned = true;
            return SQL_TINYINT;
        }
        case SQL_C_UTINYINT:
        {
            bsigned = false;
            return SQL_TINYINT;
        }
        case SQL_C_SSHORT:
        {
            bsigned = true;
            return SQL_SMALLINT;
        }
        case SQL_C_USHORT:
        {
            bsigned = false;
            return SQL_SMALLINT;
        }
        case SQL_C_SLONG:
        {
            bsigned = true;
            return SQL_INTEGER;
        }
        case SQL_C_ULONG:
        {
            bsigned = false;
            return SQL_INTEGER;
        }
        case SQL_C_SBIGINT:
        {
            bsigned = true;
            return SQL_BIGINT;
        }
        case SQL_C_UBIGINT:
        {
            bsigned = false;
            return SQL_BIGINT;
        }
        default:
            return SQL_DEFAULT;
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