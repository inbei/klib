#ifndef _KTCPWRITER_HPP_
#define _KTCPWRITER_HPP_
#if defined(WIN32)
#include <WS2tcpip.h>
#endif
#include "KTcpBase.h"
#include <cstdio>
#include "util/KTime.h"
#include "thread/KEventObject.h"

namespace klib {
    class KTcpWriter :public KEventObject<KBuffer>
    {
    public:
        KTcpWriter();
        bool Start(KTcpBase* base);

    protected:
        virtual bool IsReady() const;
        virtual void ProcessEvent(const KBuffer& ev);

    private:
        KTcpBase* m_base;
    };
};
#endif //_KTCPWRITER_HPP_
