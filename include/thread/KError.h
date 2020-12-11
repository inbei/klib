#ifndef _ERROR_HPP_
#define _ERROR_HPP_
#include <string>
#if defined(WIN32)
#include <windows.h>
#else
#include <errno.h>
#include <string.h>
#endif
#include <sstream>
#include <cstdio>
namespace klib {
    class KError
    {
    public:
#if defined(WIN32)
        static std::string WinErrorStr(int rc);
#endif

        static std::string StdErrorStr(int rc);

        static int ErrorCode();

        static std::string ErrorStr(int ec);
    };
};
#endif // _ERROR_HPP_

