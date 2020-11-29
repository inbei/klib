#include "thread/KError.h"
namespace klib {
#if defined(WIN32)
    std::string KError::WinErrorStr(int rc)
    {
        LPSTR es;
        FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER |
            FORMAT_MESSAGE_IGNORE_INSERTS |
            FORMAT_MESSAGE_FROM_SYSTEM,
            NULL,
            rc,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPSTR)&es,
            0,
            NULL);
        char buf[256] = { 0 };
        sprintf_s(buf, "err code:[%d], err str:[%s]", rc, es);
        LocalFree(es);
        std::string s(buf);
        return s;
    }
#endif
    std::string KError::StdErrorStr(int rc)
    {
        char buf[256] = { 0 };
        sprintf(buf, "err code:[%d], err str:[%s]", rc, strerror(rc));
        return std::string(buf);
    }

    int KError::ErrorCode()
    {
#if defined(WIN32)
        return GetLastError();
#else
        return errno;
#endif
    }

    std::string KError::ErrorStr(int ec)
    {

#if defined(WIN32)
        return WinErrorStr(ec);
#else
        return StdErrorStr(ec);
#endif
    }
};