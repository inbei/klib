#include "KCondVariable.h"
namespace klib {
    KCondVariable::KCondVariable()
    {
        int rc = pthread_cond_init(&m_pcond, NULL);
        if (rc != 0)
            throw KException(__FILE__, __LINE__, KError::StdErrorStr(rc).c_str());
    }

    KCondVariable::~KCondVariable()
    {
        int rc = pthread_cond_destroy(&m_pcond);
        if (rc != 0)
            throw KException(__FILE__, __LINE__, KError::StdErrorStr(rc).c_str());
    }

    void KCondVariable::Notify() const
    {
        int rc = pthread_cond_signal(&m_pcond);
        if (rc != 0)
            throw KException(__FILE__, __LINE__, KError::StdErrorStr(rc).c_str());
    }

    void KCondVariable::NotifyAll() const
    {
        int rc = pthread_cond_broadcast(&m_pcond);
        if (rc != 0)
            throw KException(__FILE__, __LINE__, KError::StdErrorStr(rc).c_str());
    }
};