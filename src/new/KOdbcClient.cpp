#include "new/KOdbcClient.h"
namespace klib
{
    // 字段信息
    struct KOdbcField
    {
        SQLUSMALLINT colnum;
        CharType colname[256];
        SQLSMALLINT namelen;

        SQLSMALLINT sqltype;
        SQLULEN colsize;
        SQLSMALLINT decimaldigits;
        SQLSMALLINT nullable;
    };

    //错误信息
    struct KOdbcSqlState
    {
        CharType         state[16];
        SQLINTEGER      nativerr;
        CharType         msgtext[SQL_MAX_MESSAGE_LENGTH];
        SQLSMALLINT     msglen;
    };

    std::map<SQLSMALLINT, SQLSMALLINT> GetSql2CType()
    {
        std::map<SQLSMALLINT, SQLSMALLINT> types;
        types[SQL_CHAR] = SQL_C_CHAR;
        types[SQL_VARCHAR] = SQL_C_CHAR;
        types[SQL_LONGVARCHAR] = SQL_C_CHAR;
        types[SQL_NUMERIC] = SQL_C_CHAR;
        types[SQL_DECIMAL] = SQL_C_CHAR;
        types[SQL_WCHAR] = SQL_C_CHAR;
        types[SQL_WVARCHAR] = SQL_C_CHAR;
        types[SQL_WLONGVARCHAR] = SQL_C_CHAR;
        types[SQL_BIT] = SQL_C_BIT;
        types[SQL_REAL] = SQL_C_FLOAT;
        types[SQL_GUID] = SQL_C_GUID;
        types[SQL_FLOAT] = SQL_C_DOUBLE;
        types[SQL_DOUBLE] = SQL_C_DOUBLE;
        types[SQL_BINARY] = SQL_C_BINARY;
        types[SQL_VARBINARY] = SQL_C_BINARY;
        types[SQL_LONGVARBINARY] = SQL_C_BINARY;
        types[SQL_TYPE_DATE] = SQL_C_TYPE_DATE;
        types[SQL_TYPE_TIME] = SQL_C_TYPE_TIME;
        types[SQL_TYPE_TIMESTAMP] = SQL_C_TYPE_TIMESTAMP;
        return types;
    }

    std::map<SQLSMALLINT, SQLSMALLINT> GetC2SqlType()
    {
        std::map<SQLSMALLINT, SQLSMALLINT> types;
        types[SQL_C_CHAR] = SQL_CHAR;
        types[SQL_C_WCHAR] = SQL_WCHAR;
        types[SQL_C_BIT] = SQL_BIT;
        types[SQL_C_FLOAT] = SQL_REAL;
        types[SQL_C_DOUBLE] = SQL_DOUBLE;
        types[SQL_C_BINARY] = SQL_BINARY;
        types[SQL_C_GUID] = SQL_GUID;
        return types;
    }

    std::map<SQLSMALLINT, SQLSMALLINT> KOdbcSql::SqlType2cType = GetSql2CType();

    std::map<SQLSMALLINT, SQLSMALLINT> KOdbcSql::CType2sqlType = GetC2SqlType();


    KOdbcSql::KOdbcSql(SQLHANDLE stmt) 
        :m_stmt(stmt)
    {
                
    }

    bool KOdbcSql::BindParam(const char* fmt, ...)
    {
        static const std::string longfmt("ld");
        static const std::string ulongfmt("lu");
        static const std::string doublefmt("lf");
        CleanParams();
        SQLHANDLE& stmt = m_stmt;
        // Check to see if there are any parameters. If so, process them. 
        SQLSMALLINT numPara = 0;
        SQLRETURN r = SQLNumParams(stmt, &numPara);
        if (!KOdbcClient::CheckSqlState(SQL_HANDLE_STMT, stmt, r))
            return false;
        SQLSMALLINT   sqltype = SQL_DEFAULT, decimaldigits = -1, nullable = -1;
        SQLUINTEGER   colsize = 0;
        int i = 1;
        va_list args;
        va_start(args, fmt);
        int pos = 0;
        int slen = strlen(fmt);
        for (; i <= numPara && pos < slen; ++i)
        {
            // mysql does't support this api
            SQLRETURN r = SQLDescribeParam(stmt, i, &sqltype, &colsize, &decimaldigits, &nullable);
            if (!KOdbcClient::CheckSqlState(SQL_HANDLE_STMT, stmt, r))
                break;
            // Bind the memory to the parameter. Assume that we only have input parameters. 
            while (pos < slen && fmt[pos] == ' ') ++pos;
            if (!(pos < slen && fmt[pos++] == '%'))break;
            switch (fmt[pos++])
            {
            case 'd':
            {
                KBuffer buf(sizeof(int32_t));
                memcpy(buf.GetData(), &va_arg(args, int32_t), sizeof(int32_t));
                m_paraBufs.push_back(buf);
                printf("int32_t param:[%d]\n", *reinterpret_cast<int32_t*>(buf.GetData()));
                r = SQLBindParameter(stmt, i, SQL_PARAM_INPUT, SQL_C_LONG,
                    SQL_INTEGER, 0, 0, (SQLPOINTER)buf.GetData(), 0, NULL);
                break;
            }
            case 'u':
            {
                KBuffer buf(sizeof(uint32_t));
                memcpy(buf.GetData(), &va_arg(args, uint32_t), sizeof(uint32_t));
                m_paraBufs.push_back(buf);
                printf("uint32_t param:[%u]\n", *reinterpret_cast<uint32_t*>(buf.GetData()));
                r = SQLBindParameter(stmt, i, SQL_PARAM_INPUT, SQL_C_LONG,
                    SQL_INTEGER, 0, 0, (SQLPOINTER)buf.GetData(), 0, NULL);
                break;
            }
            case 'f':
            {
                KBuffer buf(sizeof(float));
                memcpy(buf.GetData(), &va_arg(args, float), sizeof(float));
                m_paraBufs.push_back(buf);
                printf("float param:[%f]\n", *reinterpret_cast<float*>(buf.GetData()));
                r = SQLBindParameter(stmt, i, SQL_PARAM_INPUT, SQL_C_FLOAT,
                    SQL_FLOAT, 0, 0, (SQLPOINTER)buf.GetData(), 0, NULL);
                break;
            }
            case 'l':
            {
                if (pos + 2 > slen) goto end;
                std::string flag = std::string(fmt + pos, 2);
                if (flag == longfmt)
                {
                    KBuffer buf(sizeof(int64_t));
                    memcpy(buf.GetData(), &va_arg(args, int64_t), sizeof(int64_t));
                    m_paraBufs.push_back(buf);
                    printf("int64_t param:[%lld]\n", *reinterpret_cast<int64_t*>(buf.GetData()));
                    r = SQLBindParameter(stmt, i, SQL_PARAM_INPUT, SQL_C_SBIGINT,
                        SQL_BIGINT, 0, 0, (SQLPOINTER)buf.GetData(), 0, NULL);
                }
                else if (flag == ulongfmt)
                {
                    KBuffer buf(sizeof(uint64_t));
                    memcpy(buf.GetData(), &va_arg(args, uint64_t), sizeof(uint64_t));
                    m_paraBufs.push_back(buf);
                    printf("uint64_t param:[%llu]\n", *reinterpret_cast<uint64_t*>(buf.GetData()));
                    r = SQLBindParameter(stmt, i, SQL_PARAM_INPUT, SQL_C_SBIGINT,
                        SQL_BIGINT, 0, 0, (SQLPOINTER)buf.GetData(), 0, NULL);
                }
                else if (flag == doublefmt)
                {
                    KBuffer buf(sizeof(double));
                    memcpy(buf.GetData(), &va_arg(args, double), sizeof(double));
                    m_paraBufs.push_back(buf);
                    printf("double param:[%llf]\n", *reinterpret_cast<double*>(buf.GetData()));
                    r = SQLBindParameter(stmt, i, SQL_PARAM_INPUT, SQL_C_DOUBLE,
                        SQL_DOUBLE, 0, 0, (SQLPOINTER)buf.GetData(), 0, NULL);
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
                r = SQLBindParameter(stmt, i, SQL_PARAM_INPUT, SQL_C_CHAR,
                    SQL_VARCHAR, strlen(c), 0, (SQLPOINTER)c, strlen(c), NULL);
                break;
            }
            case 's':
            {
                const std::string& s = va_arg(args, std::string);
                KBuffer buf(s.size());
                memcpy(buf.GetData(), s.c_str(), s.size());
                m_paraBufs.push_back(buf);
                printf("string param:[%s]\n", s.c_str());
                r = SQLBindParameter(stmt, i, SQL_PARAM_INPUT, SQL_C_CHAR,
                    SQL_VARCHAR, s.size(), 0, (SQLPOINTER)buf.GetData(), s.size(), NULL);
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
        bool rc = KOdbcClient::CheckSqlState(SQL_HANDLE_STMT, stmt, r) && DescribeHeader(stmt, m_buffer);
        CleanParams();
        return rc;
    }

    bool KOdbcSql::Next()
    {
        if (m_buffer.empty())
            return false;
        return (SQLFetch(m_stmt) != SQL_NO_DATA);
    }

    void KOdbcSql::Release()
    {
        if (IsValid())
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
    }

    bool KOdbcSql::DescribeHeader(SQLHANDLE stmt, KOdbcRow& buf)
    {
        //获取列数
        SQLSMALLINT numCols = 0;
        SQLRETURN r = SQLNumResultCols(stmt, &numCols);
        if (!KOdbcClient::CheckSqlState(SQL_HANDLE_STMT, stmt, r))
            return false;

        if (numCols == 0)
            return true;

        // 获取字段信息
        buf.Release();
        buf.resize(numCols);
        for (SQLSMALLINT i = 0; i < numCols; i++)
        {
            KOdbcField cd;
            r = SQLDescribeCol(stmt, (SQLSMALLINT)i + 1, cd.colname, sizeof(cd.colname), &cd.namelen,
                &cd.sqltype, &cd.colsize, &cd.decimaldigits, &cd.nullable);
            if (!KOdbcClient::CheckSqlState(SQL_HANDLE_STMT, stmt, r))
                return false;

            if (cd.colsize == 0x7fffffff)// TEXT for sqlite
            {
                cd.sqltype = SQL_C_CHAR;
                cd.colsize = 256;
            }

            DescribeField(cd, buf[i]);
            r = SQLBindCol(stmt, i + 1, buf[i].ctype, buf[i].valbuf,
                buf[i].bufsize, (SQLLEN*)&(buf[i].valsize));
            if (!KOdbcClient::CheckSqlState(SQL_HANDLE_STMT, stmt, r))
                return false;
        }
        return true;
    }

    void KOdbcSql::DescribeField(const KOdbcField& cd, KOdbcValue& r)
    {
        r.name = std::string((char*)&cd.colname[0], cd.namelen);
        r.precision = cd.decimaldigits;
        r.nullable = (cd.nullable == SQL_NO_NULLS ? false : true);
        r.bufsize = cd.colsize;
        r.valbuf = new char[r.bufsize]();
        r.sqltype = cd.sqltype;
        r.ctype = GetCType(cd.sqltype);
    }

    SQLSMALLINT KOdbcSql::GetCType(SQLSMALLINT sqlType, bool bsigned /*= false*/)
    {
        std::map<SQLSMALLINT, SQLSMALLINT>::const_iterator it = SqlType2cType.find(sqlType);
        if (it != SqlType2cType.end())
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
        std::map<SQLSMALLINT, SQLSMALLINT>::const_iterator it = CType2sqlType.find(ctype);
        if (it != CType2sqlType.end())
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

    void KOdbcSql::CleanParams()
    {
        std::vector<KBuffer>::iterator it = m_paraBufs.begin();
        while (it != m_paraBufs.end())
        {
            it->Release();
            ++it;
        }
        m_paraBufs.clear();
    }


    KOdbcClient::KOdbcClient(const KOdbcConfig& prop) :m_conf(prop), m_henv(SQL_NULL_HENV), m_hdbc(SQL_NULL_HDBC), m_autoCommit(true)
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
            printf("OdbcCli connect failed, null driver\n");
            return false;
        }

        SQLSMALLINT sz = 1024;
        char outstr[1024] = { 0 };
        try
        {
            r = SQLDriverConnect(m_hdbc, NULL, (CharType*)connstr.c_str(), SQL_NTS,
                (CharType*)outstr, sizeof(outstr), &sz, SQL_DRIVER_NOPROMPT);
        }
        catch (const std::exception&e)
        {
            printf("<%s> exception:[%s]\n", __FUNCTION__, e.what());
            return false;
        }

        /*r = SQLConnect(_hdbc,
            (CharType *)_prop.host.c_str(), SQL_NTS,
            (CharType *)_prop.username.c_str(), SQL_NTS,
            (CharType *)_prop.passwd.c_str(), SQL_NTS);*/

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

    KOdbcSql KOdbcClient::Prepare(const std::string& sql)
    {
        KLockGuard<KMutex> lock(m_dmtx);
        SQLHANDLE stmt = SQLHANDLE(SQL_NULL_HSTMT);
        //获取操作句柄
        SQLRETURN r = SQLAllocHandle(SQL_HANDLE_STMT, m_hdbc, &stmt);
        if (!CheckSqlState(SQL_HANDLE_STMT, stmt, r))
            return KOdbcSql(SQL_NULL_HSTMT);

        r = SQLSetStmtAttr(stmt, SQL_ATTR_USE_BOOKMARKS, (SQLPOINTER)SQL_UB_OFF, SQL_IS_INTEGER);
        if (!CheckSqlState(SQL_HANDLE_STMT, stmt, r))
        {
            SQLFreeHandle(SQL_HANDLE_STMT, stmt);
            return KOdbcSql(SQL_NULL_HSTMT);
        }

        r = SQLPrepare(stmt, (CharType*)sql.c_str(), SQL_NTS);
        if (!KOdbcClient::CheckSqlState(SQL_HANDLE_STMT, stmt, r))
        {
            SQLFreeHandle(SQL_HANDLE_STMT, stmt);
            return KOdbcSql(SQL_NULL_HSTMT);
        }

        return KOdbcSql(stmt);
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

    std::string KOdbcClient::GetConnStr(const KOdbcConfig& prop)
    {
        // 使用创建数据源时显示的驱动程序名称（必须完全一致）
        char constr[256] = { 0 };
        switch (prop.drvtype)
        {
        case mysql:
            //MySQL ODBC 8.0 Unicode Driver
            //libmyodbc8w.so
            /*
            klib::KOdbcConfig config(klib::mysql);
            config.drvname = "MySQL ODBC 8.0 ANSI Driver";
            config.host = "172.25.11.102";
            config.passwd = "mics";
            config.username = "mics";
            config.dbname = "micssta";
            */
            sprintf(constr, "DRIVER={%s};SERVER=%s;PORT=%d;"
                "DATABASE=%s;USER=%s;PASSWORD=%s;CHARSET=GB18030;OPTION=3;",
                m_conf.drvname.c_str(), m_conf.host.c_str(), m_conf.port, 
                m_conf.dbname.c_str(), m_conf.username.c_str(), m_conf.passwd.c_str());
            break;
        case db2:
            //IBM DB2 ODBC DRIVER
            //libdb2.so
            /*
            klib::KOdbcConfig config(klib::db2);
            config.drvname = "IBM DATA SERVER DRIVER for ODBC - E:/software/snack2/db/clidriver";
            config.host = "172.25.11.21";
            config.passwd = "mics@db2";
            config.username = "db2inst1";
            config.dbname = "micsltz";
            */
            sprintf(constr, "driver={%s};hostname=%s;database=%s;"
                "port=%d;protocol=TCPIP;uid=%s;pwd=%s",
                m_conf.drvname.c_str(), m_conf.host.c_str(), m_conf.dbname.c_str(),
                m_conf.port ,m_conf.username.c_str(), m_conf.passwd.c_str());
            break;
        case oracle:
            //Oracle in instantclient_19_5
            //libsqora.so.19.1
            /*klib::KOdbcConfig config(klib::oracle);
            config.drvname = "Oracle in instantclient_18_5";
            config.host = "10.41.10.11";
            config.passwd = "L18mics";
            config.username = "mics";
            config.dbname = "orapdb19";*/
            sprintf(constr, "Driver={%s};UID=%s;PWD=%s;DBQ=//%s:%d/%s;",
                m_conf.drvname.c_str(), m_conf.username.c_str(), m_conf.passwd.c_str(),
                m_conf.host.c_str(), m_conf.port, m_conf.dbname.c_str());
            break;
        case sqlite:
        {
            sprintf(constr, "DRIVER={%s};Database=%s;",
                m_conf.drvname.c_str(), m_conf.dbname.c_str());
            break;
        }
        case sqlserver:
            sprintf(constr, "Driver={%s};Server=%s;Database=%s;"
                "Uid=%s;Pwd=%s;",
                m_conf.drvname.c_str(), m_conf.host.c_str(), m_conf.dbname.c_str(),
                m_conf.username.c_str(), m_conf.passwd.c_str());
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
            KOdbcSqlState ss;
            SQLRETURN tmp;
            while (i <= numrecs && (tmp = SQLGetDiagRec(htype, handle, i, ss.state, &ss.nativerr,
                ss.msgtext, sizeof(ss.msgtext), &ss.msglen)) != SQL_NO_DATA)
            {
                printf("sql state %s:%s\n", ((r == SQL_ERROR)?"error":"info"),  std::string((char*)&ss.msgtext[0], ss.msglen).c_str());
                i++;
            }
        }

        return (r == SQL_SUCCESS || r == SQL_SUCCESS_WITH_INFO);
    }

    KOdbcValue::KOdbcValue()
        :bufsize(-1), nullable(true), ctype(SQL_C_DEFAULT), 
        precision(0), valbuf(NULL), valsize(-1)
    {

    }

    void KOdbcValue::Clone(const KOdbcValue& r)
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

    void KOdbcValue::Release()
    {
        if (valbuf)
        {
            delete[] valbuf;
            valbuf = NULL;
        }
    }

    bool KOdbcValue::StringVal(std::string& val) const
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

    bool KOdbcValue::DoubleVal(double& val) const
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

    bool KOdbcValue::IntegerVal(int64_t& val) const
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

    bool KOdbcValue::BinaryVal(char* val) const
    {
        return GetBinary(val);
    }

    bool KOdbcValue::GetBool(bool& val) const
    {
        if (IsNull())
            return false;
        if (ctype != SQL_C_BIT)
            return false;
        val = 0x01 & valbuf[0];
        return true;
    }

    bool KOdbcValue::GetBinary(char* val) const
    {
        if (IsNull())
            return false;
        if (ctype != SQL_C_BINARY)
            return false;
        memmove(val, valbuf, valsize);
        return true;
    }

    bool KOdbcValue::GetStr(std::string& val) const
    {
        if (IsNull())
            return false;
        if (ctype != SQL_C_CHAR)
            return false;
        std::string(valbuf, valsize).swap(val);
        return true;
    }

    bool KOdbcValue::GetFloat(float& val) const
    {
        if (IsNull())
            return false;
        if (ctype != SQL_C_FLOAT)
            return false;
        val = *(float*)(valbuf);
        return true;
    }

    bool KOdbcValue::GetDouble(double& val) const
    {
        if (IsNull())
            return false;
        if (ctype != SQL_C_DOUBLE)
            return false;
        val = *(double*)(valbuf);
        return true;
    }

    bool KOdbcValue::GetInt32(int32_t& val) const
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

    bool KOdbcValue::GetUint32(uint32_t& val) const
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

    bool KOdbcValue::GetInt64(int64_t& val) const
    {
        if (IsNull())
            return false;
        if (ctype != SQL_C_SBIGINT)
            return false;
        val = *(int64_t*)(valbuf);
        return true;
    }

    bool KOdbcValue::GetUint64(uint64_t& val) const
    {
        if (IsNull())
            return false;
        if (ctype != SQL_C_UBIGINT)
            return false;
        val = *(uint64_t*)(valbuf);
        return true;
    }

    bool KOdbcValue::GetGuid(std::string& val) const
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

    bool KOdbcValue::GetDate(std::string& val) const
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

    bool KOdbcValue::GetTime(std::string& val) const
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

    bool KOdbcValue::GetTimestamp(std::string& val) const
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

    void KOdbcRow::Clone(const KOdbcRow& other)
    {
        std::vector<KOdbcValue>::const_iterator it = other.begin();
        while (it != other.end())
        {
            KOdbcValue val;
            val.Clone(*it);
            push_back(val);
            ++it;
        }
    }

    void KOdbcRow::Release()
    {
        std::vector<KOdbcValue>::iterator it = begin();
        while (it != end())
        {
            it->Release();
            ++it;
        }
        clear();
    }

    KOdbcConfig::KOdbcConfig(KOdbcType type) :drvtype(type), port(0)
    {
        switch (drvtype)
        {
        case klib::mysql:
            port = 3306;
            break;
        case klib::db2:
            port = 50000;
            break;
        case klib::oracle:
            port = 1521;
            break;
        default:
            break;
        }
    }

};