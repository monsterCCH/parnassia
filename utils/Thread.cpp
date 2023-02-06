#include "Thread.h"
#include <stdio.h>
#include <unistd.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <linux/unistd.h>

#include <utility>
#include "CurrentThread.h"
#include "Exception.h"
#include "logger.h"
#include "TimeStamp.h"

pid_t gettid()
{
    return static_cast<pid_t>(::syscall(SYS_gettid));
}

void afterFork()
{
    CurrentThread::t_cachedTid = 0;
    CurrentThread::t_threadName = "main";
    CurrentThread::tid();
    // no need to call pthread_atfork(NULL, NULL, &afterFork);
}

class ThreadNameInitializer
{
public:
    ThreadNameInitializer()
    {
        CurrentThread::t_threadName = "main";
        CurrentThread::tid();
        pthread_atfork(nullptr, nullptr, &afterFork);
    }
};

ThreadNameInitializer init;

struct ThreadData
{
    typedef Thread::ThreadFunc ThreadFunc;
    ThreadFunc func_;
    std::string name_;
    pid_t* tid_;
    CountDownLatch* latch_;

    ThreadData(ThreadFunc func,
               std::string  name,
               pid_t* tid,
               CountDownLatch* latch)
        : func_(std::move(func)),
          name_(std::move(name)),
          tid_(tid),
          latch_(latch)
    { }

    void runInThread()
    {
        *tid_ = CurrentThread::tid();
        tid_ = nullptr;
        latch_->countDown();
        latch_ = nullptr;

        CurrentThread::t_threadName = name_.empty() ? "Thread" : name_.c_str();
        ::prctl(PR_SET_NAME, CurrentThread::t_threadName);
        try
        {
            func_();
            CurrentThread::t_threadName = "finished";
        }
        catch (const Exception& ex)
        {
            CurrentThread::t_threadName = "crashed";
            LOG->error("exception caught in Thread {}", name_.c_str());
            LOG->error("reason: {}", ex.what());
            LOG->error("stack trace: {}", ex.stackTrace());
            abort();
        }
        catch (const std::exception& ex)
        {
            CurrentThread::t_threadName = "crashed";
            LOG->error("exception caught in Thread {}", name_.c_str());
            LOG->error("reason: {}", ex.what());
            abort();
        }
        catch (...)
        {
            CurrentThread::t_threadName = "crashed";
            LOG->error("unknown exception caught in Thread {}", name_.c_str());
            throw; // rethrow
        }
    }
};

void* startThread(void* obj)
{
    ThreadData* data = static_cast<ThreadData*>(obj);
    data->runInThread();
    delete data;
    return nullptr;
}



void CurrentThread::cacheTid()
{
    if (t_cachedTid == 0)
    {
        t_cachedTid = gettid();
        t_tidStringLength = snprintf(t_tidString, sizeof t_tidString, "%5d ", t_cachedTid);
    }
}

bool CurrentThread::isMainThread()
{
    return tid() == ::getpid();
}

void CurrentThread::sleepUsec(int64_t usec)
{
    struct timespec ts = { 0, 0 };
    ts.tv_sec = static_cast<time_t>(usec / Timestamp::kMicroSecondsPerSecond);
    ts.tv_nsec = static_cast<long>(usec % Timestamp::kMicroSecondsPerSecond * 1000);
    ::nanosleep(&ts, nullptr);
}

std::atomic_int Thread::numCreated_{0};

Thread::Thread(ThreadFunc func, std::string  n)
    : started_(false),
      joined_(false),
      pthreadId_(0),
      tid_(0),
      func_(std::move(func)),
      name_(std::move(n)),
      latch_(1)
{
    setDefaultName();
}

Thread::~Thread()
{
    if (started_ && !joined_)
    {
        pthread_detach(pthreadId_);
    }
}

void Thread::setDefaultName()
{
    int num = numCreated_.fetch_add(1);
    if (name_.empty())
    {
        char buf[32];
        snprintf(buf, sizeof buf, "Thread%d", num);
        name_ = buf;
    }
}

void Thread::start()
{
    assert(!started_);
    started_ = true;
    ThreadData* data = new ThreadData(func_, name_, &tid_, &latch_);
    if (pthread_create(&pthreadId_, nullptr, &startThread, data))
    {
        started_ = false;
        delete data; // or no delete?
        LOG->error("Failed in pthread_create");
    }
    else
    {
        latch_.wait();
        assert(tid_ > 0);
    }
}

int Thread::join()
{
    assert(started_);
    assert(!joined_);
    joined_ = true;
    return pthread_join(pthreadId_, NULL);
}
