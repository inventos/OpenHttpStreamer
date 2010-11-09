#ifndef __pool_hh__d457af2e_c210_4ace_9202_cd6de95ac808
#define __pool_hh__d457af2e_c210_4ace_9202_cd6de95ac808

#include <boost/pool/pool.hpp>

template<class T> class PoolMixin {
private:
    static boost::pool<> m_pool;
public:
    static void *operator new(size_t sz) {
        return m_pool.malloc();
    }
    static void operator delete(void *p) {
        if(p) m_pool.free(p);
    }
};

template<class T> boost::pool<> PoolMixin<T>::m_pool(sizeof(T));

#endif
