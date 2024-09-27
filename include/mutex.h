#ifndef MUTEX_H
#define MUTEX_H

#include <pthread.h>

namespace KSC {

template<class T>
struct ScopedLockImpl {
public:
    ScopedLockImpl(T& mtx)
        : m_mtx(mtx) {
        m_mtx.lock();
        m_locked = true;
    }

    ~ScopedLockImpl() {
        m_mtx.unlock();
    }

    void lock() {
        if (!m_locked) {
            m_mtx.lock();
            m_locked = true;
        }
    }

    void unlock() {
        if (m_locked) {
            m_mtx.unlock();
            m_locked = false;
        }
    }

private:
    T& m_mtx;
    bool m_locked;
};

class spin_mutex {
public:
    spin_mutex() {
        pthread_spin_init(&m_mtx, 0);
    }

    ~spin_mutex() {
        pthread_spin_destroy(&m_mtx);
    }

    void lock() {
        pthread_spin_lock(&m_mtx);
    }

    void unlock() {
        pthread_spin_unlock(&m_mtx);
    }

    spin_mutex(const spin_mutex& other) = delete;
    spin_mutex& operator=(const spin_mutex& other) = delete;

private:
    pthread_spinlock_t m_mtx;
};

using spin_lock = ScopedLockImpl<spin_mutex>;

};




#endif