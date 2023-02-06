#ifndef _TRINERVIS_THREAD_H
#define _TRINERVIS_THREAD_H

#include "base.h"
#include <functional>
#include <memory>
#include <pthread.h>
#include <atomic>
#include "CountDownLatch.h"

class Thread : public noncopyable
{
public:
    typedef std::function<void ()> ThreadFunc;

    explicit Thread(ThreadFunc, std::string name = std::string());
    ~Thread();

    void start();
    int join(); // return pthread_join()

    [[nodiscard]] bool started() const { return started_; }
    [[nodiscard]] pid_t tid() const { return tid_; }
    [[nodiscard]] const std::string& name() const { return name_; }

    static int numCreated() { return numCreated_.load(); }

private:
    void setDefaultName();

    bool       started_;
    bool       joined_;
    pthread_t  pthreadId_;
    pid_t      tid_;
    ThreadFunc func_;
    std::string     name_;
    CountDownLatch latch_;

    static std::atomic_int numCreated_;
};

#endif   // _TRINERVIS_THREAD_H
