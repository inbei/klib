#include "KTime.h"
#include "KError.h"
namespace klib {
    bool KTime::NowDateTime(DateTime& datetime)
    {
#ifdef WIN32
        SYSTEMTIME st;
        GetLocalTime(&st);
        datetime.year = st.wYear;
        datetime.month = st.wMonth;
        datetime.day = st.wDay;
        datetime.hour = st.wHour;
        datetime.minute = st.wMinute;
        datetime.second = st.wSecond;
        datetime.milliSecond = st.wMilliseconds;
        return true;
#else
        timeval st;
        if (gettimeofday(&st, NULL) == 0)
        {
            tm* pt = localtime(&st.tv_sec);
            if (pt)
            {
                datetime.year = pt->tm_year + 1900;
                datetime.month = pt->tm_mon + 1;
                datetime.day = pt->tm_mday;
                datetime.hour = pt->tm_hour;
                datetime.minute = pt->tm_min;
                datetime.second = pt->tm_sec;
                datetime.milliSecond = st.tv_usec / 1000;
                return true;
            }
        }
#endif
        return false;
    }

    std::string KTime::FormatDateTime(const std::string timeString)
    {
        std::string timeStr(timeString);
        if (timeStr.size() < 17)
        {
            timeStr.append(timeStr.size() - 1, '0');
        }
        // yyyy-mm-dd hh:nn:ss.ccc
        char buf[24] = { 0 };
        const char* src = timeString.c_str();
        size_t dstOffset = 0;
        size_t srcOffset = 0;
        memmove(buf + dstOffset, src + srcOffset, 4);
        srcOffset += 4;
        dstOffset += 4;
        buf[dstOffset++] = '-';

        memmove(buf + dstOffset, src + srcOffset, 2);
        srcOffset += 2;
        dstOffset += 2;
        buf[dstOffset++] = '-';

        memmove(buf + dstOffset, src + srcOffset, 2);
        srcOffset += 2;
        dstOffset += 2;
        buf[dstOffset++] = ' ';

        memmove(buf + dstOffset, src + srcOffset, 2);
        srcOffset += 2;
        dstOffset += 2;
        buf[dstOffset++] = ':';

        memmove(buf + dstOffset, src + srcOffset, 2);
        srcOffset += 2;
        dstOffset += 2;
        buf[dstOffset++] = ':';

        memmove(buf + dstOffset, src + srcOffset, 2);
        srcOffset += 2;
        dstOffset += 2;
        buf[dstOffset++] = '.';

        memmove(buf + dstOffset, src + srcOffset, 3);
        srcOffset += 3;
        dstOffset += 3;

        return std::string(buf);
    }

    void KTime::NowMillisecond(uint64_t& millisec)
    {
#ifdef WIN32
        timeb t;
        ftime(&t);
        millisec = uint64_t(t.time) * 1000 + t.millitm;
#else
        timeval tv;
        if (gettimeofday(&tv, NULL) == 0)
        {
            millisec = (uint64_t(tv.tv_sec) * 1000 + tv.tv_usec / 1000);
        }
        else
        {
            millisec = 0;
        }
#endif
    }

	void KTime::NowMicrosecond(uint64_t& microsec)
	{
#ifdef WIN32
#define EPOCHFILETIME   (116444736000000000UL)
		FILETIME ft;
		LARGE_INTEGER li;
		GetSystemTimeAsFileTime(&ft);
		li.LowPart = ft.dwLowDateTime;
		li.HighPart = ft.dwHighDateTime;
		microsec = (li.QuadPart - EPOCHFILETIME) / 10;
#else
		timeval tv;
		if (gettimeofday(&tv, NULL) == 0)
		{
			microsec = (uint64_t(tv.tv_sec) * 1000000 + tv.tv_usec);
		}
		else
		{
			microsec = 0;
		}
#endif
    }

    void KTime::NowSecond(time_t& seconds)
    {
#ifdef WIN32
        timeb t;
        ftime(&t);
        seconds = t.time;
#else
        timeval tv;
        if (gettimeofday(&tv, NULL) == 0)
        {
            seconds = tv.tv_sec;
        }
        else
        {
            seconds = 0;
        }
#endif
    }

    void KTime::NowTime(timespec& abstime)
    {
#ifdef WIN32
        timeb t;
        ftime(&t);
        abstime.tv_sec = (long)t.time;
        abstime.tv_nsec = t.millitm * 1000000;
#else
        timeval tv;
        if (gettimeofday(&tv, NULL) == 0)
        {
            abstime.tv_sec = tv.tv_sec;
            abstime.tv_nsec = tv.tv_usec * 1000;
        }
        else
        {
            abstime.tv_sec = 0;
            abstime.tv_nsec = 0;
        }
#endif
    }

    void KTime::GetTime(timespec& abstime, const size_t& millisec)
    {
        timespec ts;
        NowTime(ts);

        int nsec = ts.tv_nsec + (millisec % 1000) * 1000000;
        abstime.tv_nsec = nsec % 1000000000;
        abstime.tv_sec = ts.tv_sec + nsec / 1000000000 + millisec / 1000;
    }

    void KTime::MSleep(int millisec)
    {
#if defined(WIN32)
        Sleep(millisec);
#else
        if (millisec >= 1000)
        {
            sleep(millisec / 1000);
            millisec %= 1000;
        }
        usleep(millisec * 1000);
#endif
    }

    void KTime::GetDateTimeElement(const std::string& fmt, const char* keyword, const char* intfmt, uint32_t value, char* buf)
    {
        size_t yp = fmt.find(keyword);
        if (yp != std::string::npos)
        {
            char* start = buf + yp;
            size_t len = strlen(keyword);
            char tmp = *(start + len);
#ifdef WIN32
            sprintf_s(start, len + 1, intfmt, value);
#else
            sprintf(start, intfmt, value);
#endif
            * (start + len) = tmp;
        }
    }

    bool KTime::NowDateTime(const std::string& fmt, std::string& datetime)
    {
        DateTime dt;
        if (NowDateTime(dt))
        {
            char buf[32] = { 0 };
#ifdef WIN32
            strcpy_s(buf, fmt.size() + 1, fmt.data());
#else
            strcpy(buf, fmt.data());
#endif
            // year
            GetDateTimeElement(fmt, "yyyy", "%04d", dt.year, buf);
            // month
            GetDateTimeElement(fmt, "mm", "%02d", dt.month, buf);
            // day
            GetDateTimeElement(fmt, "dd", "%02d", dt.day, buf);
            // hour
            GetDateTimeElement(fmt, "hh", "%02d", dt.hour, buf);
            // minute
            GetDateTimeElement(fmt, "nn", "%02d", dt.minute, buf);
            // second
            GetDateTimeElement(fmt, "ss", "%02d", dt.second, buf);
            // millisecond
            GetDateTimeElement(fmt, "ccc", "%03d", dt.milliSecond, buf);
            datetime = buf;
            return true;
        }
        else
        {
            datetime = KError::ErrorStr(KError::ErrorCode());
            return false;
        }
    }
};