#include "KException.h"
#include <string.h>
namespace klib {
    KException::KException(const char* file, int line, const char* msg) :std::runtime_error(Join(file, line, msg))
    {

    }

    std::string KException::Join(const char* file, int line, const char* msg)
    {
        char *s = new char[strlen(file) + sizeof(line) + strlen(msg) + 3]();
        sprintf(s, "%s:%d:%s", file, line, msg);
        std::string r(s);
        delete[] s;
        return r;
    }
};