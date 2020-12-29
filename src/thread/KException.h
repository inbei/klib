#ifndef _EXCEPTION_HPP_
#define _EXCEPTION_HPP_
#include <stdexcept>
#include <string>
#if !defined(WIN32)
#include <errno.h>
#endif
#include <sstream>
#include <cstdio>
namespace klib {
    class KException :public std::runtime_error
    {
    public:
        explicit KException(const char* file, int  line, const char* msg);

    private:
        static std::string Join(const char* file, int  line, const char* msg);
    };
};
#endif // !_EXCEPTION_HPP_

