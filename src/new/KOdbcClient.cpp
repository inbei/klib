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

    KOdbcSql::KOdbcSql(SQLHANDLE stmt) 
        :m_stmt(stmt)
    {
        m_sqlType2cType[SQL_CHAR] = SQL_C_CHAR;
        m_sqlType2cType[SQL_VARCHAR] = SQL_C_CHAR;
        m_sqlType2cType[SQL_LONGVARCHAR] = SQL_C_CHAR;
        m_sqlType2cType[SQL_NUMERIC] = SQL_C_CHAR;
        m_sqlType2cType[SQL_DECIMAL] = SQL_C_CHAR;
        m_sqlType2cType[SQL_WCHAR] = SQL_C_CHAR;
        m_sqlType2cType[SQL_WVARCHAR] = SQL_C_CHAR;
        m_sqlType2cType[SQL_WLONGVARCHAR] = SQL_C_CHAR;
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
        size_t slen = strlen(fmt);
        for (; i <= numPara && pos < slen; ++i)
        {
            // mysql does't support this api
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
                KBuffer buf(sizeof(int32_t));
                memcpy(buf.GetData(), &va_arg(args, int32_t), sizeof(int32_t));
                m_paraBufs.push_back(buf);
                printf("int32_t param:[%d]\n", *reinterpret_cast<int32_t*>(buf.GetData()));
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
            fprintf(stderr, "OdbcCli connect failed, null driver\n");
            return false;
        }

        SQLSMALLINT sz = 1024;
        char outstr[1024] = { 0 };
        r = SQLDriverConnect(m_hdbc, NULL, (CharType*)connstr.c_str(), SQL_NTS,
            (CharType*)outstr, sizeof(outstr), &sz, SQL_DRIVER_NOPROMPT);

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
            KOdbcSqlState ss;
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

};