#pragma once

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
        /************************************
        * Description: 获取当前时间
        * Method:    NowDateTime
        * FullName:  KTime::NowDateTime
        * Access:    public static
        * Returns:   bool
        * Qualifier:
        * Parameter: DateTime & datetime
        ************************************/
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
        /************************************
        * Description: 根据格式获取当前时间
        * Method:    NowDateTime
        * FullName:  KTime::NowDateTime
        * Access:    public static
        * Returns:   bool
        * Qualifier:
        * Parameter: const std::string & fmt
        * Parameter: std::string & datetime
        ************************************/
        static bool NowDateTime(const std::string& fmt, std::string& datetime);

        static std::string FormatDateTime(const std::string timeString);

        /************************************
        * Description: 获取当前时间毫秒数
        * Method:    NowMillisecond
        * FullName:  KTime::NowMillisecond
        * Access:    public static
        * Returns:   void
        * Qualifier:
        * Parameter: uint64_t & millisec
        ************************************/
        static void NowMillisecond(uint64_t& millisec);

        static void NowMicrosecond(uint64_t& microsec);
        /************************************
        * Description: 获取当前时间秒数
        * Method:    NowSecond
        * FullName:  KTime::NowSecond
        * Access:    public static
        * Returns:   void
        * Qualifier:
        * Parameter: time_t & seconds
        ************************************/
        static void NowSecond(time_t& seconds);

        /************************************
        * Description: 获取当前timespec时间
        * Method:    NowTime
        * FullName:  KTime::NowTime
        * Access:    public static
        * Returns:   void
        * Qualifier:
        * Parameter: timespec & abstime
        ************************************/
        static void NowTime(timespec& abstime);

        static void GetTime(timespec& abstime, const size_t& millisec);

        /************************************
        * Description: 睡眠millisec毫秒
        * Method:    MSleep
        * FullName:  KTime::MSleep
        * Access:    public static
        * Returns:   void
        * Qualifier:
        * Parameter: int millisec
        ************************************/
        static void MSleep(int millisec);
    private:
        static void GetDateTimeElement(const std::string& fmt, const char* keyword,
            const char* intfmt, uint32_t value, char* buf);

    };
};
#endif // !_TIME_HPP_
