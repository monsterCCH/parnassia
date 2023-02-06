#ifndef PARNASSIA_TRINERVIS_CONDITION_H
#define PARNASSIA_TRINERVIS_CONDITION_H
#include "base.h"
#include "Mutex.h"

class Condition : noncopyable
{
public:
    explicit Condition(MutexLock& mutex)
        : mutex_(mutex)
    {
        MCHECK(pthread_cond_init(&pcond_, nullptr));
    }

    ~Condition()
    {
        MCHECK(pthread_cond_destroy(&pcond_));
    }

    void wait()
    {
        MutexLock::UnassignGuard ug(mutex_);
        MCHECK(pthread_cond_wait(&pcond_, mutex_.getPthreadMutex()));
    }

    // returns true if time out, false otherwise.
    bool waitForSeconds(double seconds);

    void notify()
    {
        MCHECK(pthread_cond_signal(&pcond_));
    }

    void notifyAll()
    {
        MCHECK(pthread_cond_broadcast(&pcond_));
    }

private:
    MutexLock& mutex_;
    pthread_cond_t pcond_;
};

#endif   // PARNASSIA_TRINERVIS_CONDITION_H
