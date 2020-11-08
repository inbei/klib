#pragma once
#ifndef _STRING_UTIL_HPP_
#define _STRING_UTIL_HPP_
#include <string>
#include <string.h>
#include <sstream>
#include <iconv.h>
#include <stdint.h>

namespace klib {
    class KStringUtility
    {
    public:
        /************************************
        * Description: 插入字符串
        * Method:    Insert
        * FullName:  KStringUtility::Insert
        * Access:    public static
        * Returns:   void
        * Qualifier:
        * Parameter: const std::string & key
        * Parameter: const std::string & is
        * Parameter: std::string & src
        ************************************/
        static void Insert(const std::string& key, const std::string& is, std::string& src);

        /************************************
        * Description: 替换字符串
        * Method:    Replace
        * FullName:  KStringUtility::Replace
        * Access:    public static
        * Returns:   void
        * Qualifier:
        * Parameter: const std::string & news
        * Parameter: const std::string & olds
        * Parameter: std::string & src
        ************************************/
        static void Replace(const std::string& news, const std::string& olds, std::string& src);

        /************************************
        * Description: 分割字符串
        * Method:    SplitString
        * FullName:  KStringUtility::SplitString
        * Access:    public static
        * Returns:   void
        * Qualifier:
        * Parameter: const std::string & src
        * Parameter: const std::string & delim
        * Parameter: StringContainer & dest
        * Parameter: bool reserveempty
        ************************************/
        template <typename StringContainer>
        static void SplitString(const std::string& src, const std::string& delim, StringContainer& dest, bool reserveempty = false);

        /************************************
        * Description: 分割字符串
        * Method:    SplitString2
        * FullName:  KStringUtility::SplitString2
        * Access:    public static
        * Returns:   void
        * Qualifier:
        * Parameter: const std::string & src
        * Parameter: const std::string & delim
        * Parameter: StringContainer & dest
        * Parameter: bool reserveempty
        ************************************/
        template <typename StringContainer>
        static void SplitString2(const std::string& src, const std::string& delim, StringContainer& dest, bool reserveempty = false);

        /************************************
        * Description: 合并字符串
        * Method:    JoinString
        * FullName:  KStringUtility::JoinString
        * Access:    public static
        * Returns:   std::string
        * Qualifier:
        * Parameter: const StringContainer & src
        * Parameter: const std::string & delim
        ************************************/
        template <typename StringContainer>
        static std::string JoinString(const StringContainer& src, const std::string& delim);

        /************************************
        * Description: 去掉字符串两边的空白字符
        * Method:    TrimString
        * FullName:  KStringUtility::TrimString
        * Access:    public static
        * Returns:   std::string
        * Qualifier:
        * Parameter: const std::string & s
        ************************************/
        static std::string TrimString(const std::string& s);

        /************************************
        * Description: 字符串转换成utf8
        * Method:    ToUTF8
        * FullName:  KStringUtility::ToUTF8
        * Access:    public static
        * Returns:   bool
        * Qualifier:
        * Parameter: const std::string & instr
        * Parameter: std::string & outstr
        ************************************/
        static bool ToUTF8(const std::string& instr, std::string& outstr);

        static std::string ToUpper(const std::string& str);

        static std::string ToLower(const std::string& str);

        static std::string DoubleToString(double dval);
        static std::string FloatToString(float fval);
        static std::string Int64ToString(int64_t ival);
        static std::string Int32ToString(int32_t ival);
        static std::string Uint32ToString(uint32_t ival);
    };

    template <typename StringContainer>
    std::string KStringUtility::JoinString(const StringContainer& src, const std::string& delim)
    {
        std::ostringstream out;
        typename StringContainer::const_iterator it = src.begin();
        while (it != src.end())
        {
            out << *it;
            ++it;
            if (it != src.end())
            {
                out << delim;
            }
        }
        return out.str();
    };

    template <typename StringContainer>
    void KStringUtility::SplitString2(const std::string& src, const std::string& delim, StringContainer& dest, bool reserveempty /*= false*/)
    {
        size_t pos = 0;
        size_t prev = 0;
        while ((pos = src.find(delim, prev)) != std::string::npos)
        {
            if (reserveempty || (!reserveempty && pos > prev))
            {
                dest.insert(dest.end(), src.substr(prev, pos - prev));
            }

            prev = pos + delim.size();
        }

        if (prev < src.size())
        {
            dest.insert(dest.end(), src.substr(prev, src.size() - prev));
        }
    };

    template <typename StringContainer>
    void KStringUtility::SplitString(const std::string& src, const std::string& delim, StringContainer& dest, bool reserveempty /*= false*/)
    {
        size_t pos = 0;
        size_t prev = (reserveempty ? 0 : src.find_first_not_of(delim, 0));
        while ((pos = src.find_first_of(delim, prev)) != std::string::npos)
        {
            dest.insert(dest.end(), src.substr(prev, pos - prev));
            if (reserveempty)
            {
                prev = pos + 1;
            }
            else
            {
                size_t tmp = src.find_first_not_of(delim, pos);
                if (tmp == std::string::npos)
                {
                    prev = src.size();
                    break;
                }
                else
                {
                    prev = tmp;
                }
            }
        }

        if (prev < src.size())
        {
            dest.insert(dest.end(), src.substr(prev, src.size() - prev));
        }
    };
};
#endif // !_STRING_UTIL_HPP_
