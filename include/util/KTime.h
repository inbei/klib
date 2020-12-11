#ifndef _TIME_HPP_
#define _TIME_HPP_
#include <string>
#include <string.h>
#ifdef WIN32
#include <Windows.h>
#include <sys/timeb.h>
#include <pthread.h>
#else
#include <sys/time.h>
#include <unistd.h>
#endif // WIN32
#include <stdint.h>

namespace klib {
    struct DateTime
    {
        uint32_t year;
        uint32_t month;
        uint32_t day;
        uint32_t hour;
        uint32_t minute;
        uint32_t second;
        uint32_t milliSecond;
    };

    class KTime
    {
    public:
        // 获取当前时间
        static bool NowDateTime(DateTime& datetime);
        /*
        * yyyy year
        * mm month
        * dd day
        * hh hour
        * nn minute
        * ss second
        * ccc millisecond
        */
        static bool NowDateTime(const std::string& fmt, std::string& datetime);
        // 格式化时间
        static std::string FormatDateTime(const std::string timeString);
        // 获取当前时间毫秒数
        static void NowMillisecond(uint64_t& millisec);
        // 获取当前时间微秒数
        static void NowMicrosecond(uint64_t& microsec);
        // 获取当前时间秒数
        static void NowSecond(time_t& seconds);
        // 获取当前timespec时间
        static void NowTime(timespec& abstime);
        // 获取当前时间
        static void GetTime(timespec& abstime, const size_t& millisec);
        // 睡眠millisec毫秒
        static void MSleep(int millisec);
    private:
        static void GetDateTimeElement(const std::string& fmt, const char* keyword,
            const char* intfmt, uint32_t value, char* buf);

    };
};
#endif // !_TIME_HPP_
