#ifndef PARNASSIA_TRINERVIS_COUNTDOWNLATCH_H
#define PARNASSIA_TRINERVIS_COUNTDOWNLATCH_H
#include "base.h"
#include "Mutex.h"
#include "Condition.h"

class CountDownLatch : noncopyable
{
public:

    explicit CountDownLatch(int count);

    void wait();

    void countDown();

    int getCount() const;

private:
    mutable MutexLock mutex_;
    Condition condition_ GUARDED_BY(mutex_);
    int count_ GUARDED_BY(mutex_);
};
#endif   // PARNASSIA_TRINERVIS_COUNTDOWNLATCH_H
