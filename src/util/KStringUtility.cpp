#include "util/KStringUtility.h"
#include <cstdio>

namespace klib {
    void KStringUtility::Insert(const std::string& key, const std::string& is, std::string& src)
    {
        size_t pos = 0;
        while ((pos = src.find(key, pos)) != std::string::npos)
        {
            src.insert(pos, is);
            pos += (key.size() + is.size());
        }
    }

    void KStringUtility::Replace(const std::string& news, const std::string& olds, std::string& src)
    {
        size_t pos = 0;
        while ((pos = src.find(olds, pos)) != std::string::npos)
        {
            src.replace(pos, olds.size(), news);
            pos += news.size();
        }
    }

    std::string KStringUtility::TrimString(const std::string& s)
    {
        static const std::string delim = " \t\r\n";
        std::string::size_type sp = s.find_first_not_of(delim);
        if (sp == std::string::npos)
        {
            return "";
        }
        else
        {
            return s.substr(sp, s.find_last_not_of(delim) - sp + 1);
        }
    }

    bool KStringUtility::ToUTF8(const std::string& instr, std::string& outstr)
    {
        if (instr.empty())
        {
            return false;
        }

        size_t inlen = instr.size();
        char* inbuf = const_cast<char*>(instr.c_str());

        size_t outlen = 2 * inlen + 1;
        char* outbuf = new char[outlen];
        memset(outbuf, 0, outlen);

        char* in = inbuf;
        char* out = outbuf;
        iconv_t cd = iconv_open("UTF-8", "GB18030");//  gb18030 to utf8
#if defined(WIN32) || defined(AIX)
        int rc = iconv(cd, (const char**)&in, &inlen, &out, &outlen);
#else
        int rc = iconv(cd, &in, &inlen, &out, &outlen);
#endif
        iconv_close(cd);

        outlen = strlen(outbuf);
        outstr.assign(outbuf);
        delete[] outbuf;
        return rc != -1;
    }

    std::string KStringUtility::ToUpper(const std::string& str)
    {
        std::string result;
        std::string::const_iterator it = str.begin();
        while (it != str.end())
        {
            const char& c = *it;
            result.push_back((c <= 'z' && c >= 'a') ? c - 32 : c);
            ++it;
        }
        return result;
    }

    std::string KStringUtility::ToLower(const std::string& str)
    {
        std::string result;
        std::string::const_iterator it = str.begin();
        while (it != str.end())
        {
            const char& c = *it;
            result.push_back((c <= 'Z' && c >= 'A') ? c + 32 : c);
            ++it;
        }
        return result;
    }

    std::string KStringUtility::DoubleToString(double dval)
    {
        char buf[64] = { 0 };
        sprintf(buf, "%lf", dval);
        return std::string(buf);
    }

    std::string KStringUtility::FloatToString(float fval)
    {
        char buf[64] = { 0 };
        sprintf(buf, "%f", fval);
        return std::string(buf);
    }

    std::string KStringUtility::Int64ToString(int64_t ival)
    {
        char buf[64] = { 0 };
        sprintf(buf, "%lld", ival);
        return std::string(buf);
    }

    std::string KStringUtility::Int32ToString(int32_t ival)
    {
        char buf[64] = { 0 };
        sprintf(buf, "%d", ival);
        return std::string(buf);
    }

    std::string KStringUtility::Uint32ToString(uint32_t ival)
    {
        char buf[64] = { 0 };
        sprintf(buf, "%u", ival);
        return std::string(buf);
    }
};