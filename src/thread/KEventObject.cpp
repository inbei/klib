#include "thread/KEventObject.h"

namespace klib {
    KMutex KEventBase::s_eobjmtx;
    AtomicInteger<uint32_t> KEventBase::s_eobjid;
    std::map<uint32_t, KEventBase*> KEventBase::s_eobjmap;
};