#ifndef _STRING_UTIL_HPP_
#define _STRING_UTIL_HPP_
#include <string>
#include <string.h>
#include <sstream>
#include <iconv.h>
#include <stdint.h>
/**
字符串处理类
**/
namespace klib {
    class KStringUtility
    {
    public:
        // 插入字符串 //
        static void Insert(const std::string& key, const std::string& is, std::string& src);
        // 替换字符串 //
        static void Replace(const std::string& news, const std::string& olds, std::string& src);
        // 分割字符串，使用字符串delim任意一个字符作为分隔符 //
        template <typename StringContainer>
        static void SplitString(const std::string& src, const std::string& delim, StringContainer& dest, bool reserveempty = false);
        // 分割字符串，使用整个字符串delim作为分隔符 //
        template <typename StringContainer>
        static void SplitString2(const std::string& src, const std::string& delim, StringContainer& dest, bool reserveempty = false);
        // 合并字符串 //
        template <typename StringContainer>
        static std::string JoinString(const StringContainer& src, const std::string& delim);
        // 去掉字符串两边的空白字符 //
        static std::string TrimString(const std::string& s);
        // 字符串转换成utf8 //
        static bool ToUTF8(const std::string& instr, std::string& outstr);
        // 转换成大写 //
        static std::string ToUpper(const std::string& str);
        // 转换成小写 //
        static std::string ToLower(const std::string& str);
        // 数字转字符串 //
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
