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
/**
错误信息类
**/
namespace klib {
    class KError
    {
    public:
#if defined(WIN32)
        /************************************
        * Method:    windows错误信息
        * Returns:   
        * Parameter: rc
        *************************************/
        static std::string WinErrorStr(int rc);
#endif

        /************************************
        * Method:    错误信息
        * Returns:   
        * Parameter: rc
        *************************************/
        static std::string StdErrorStr(int rc);

        /************************************
        * Method:    错误码
        * Returns:   
        *************************************/
        static int ErrorCode();

        /************************************
        * Method:    错误信息
        * Returns:   
        * Parameter: ec
        *************************************/
        static std::string ErrorStr(int ec);
    };
};
#endif // _ERROR_HPP_

