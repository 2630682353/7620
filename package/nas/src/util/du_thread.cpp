#include "du_thread.h"
#include <cerrno>

DU_ThreadControl::DU_ThreadControl(pthread_t thread) : _thread(thread)
{
}

DU_ThreadControl::DU_ThreadControl() : _thread(pthread_self())
{
}

void DU_ThreadControl::join()
{
    if(pthread_self() == _thread)
    {
        throw DU_ThreadThreadControl_Exception("[DU_ThreadControl::join] can't be called in the same thread");
    }

    void* ignore = 0;
    int rc = pthread_join(_thread, &ignore);
    if(rc != 0)
    {
        throw DU_ThreadThreadControl_Exception("[DU_ThreadControl::join] pthread_join error ", rc);
    }
}

void DU_ThreadControl::detach()
{
    if(pthread_self() == _thread)
    {
        throw DU_ThreadThreadControl_Exception("[DU_ThreadControl::join] can't be called in the same thread");
    }

    int rc = pthread_detach(_thread);
    if(rc != 0)
    {
        throw DU_ThreadThreadControl_Exception("[DU_ThreadControl::join] pthread_join error", rc);
    }
}

pthread_t DU_ThreadControl::id() const
{
    return _thread;
}

void DU_ThreadControl::sleep(long millsecond)
{
    struct timespec ts;
    ts.tv_sec = millsecond / 1000;
    ts.tv_nsec = millsecond % 1000;
    nanosleep(&ts, 0);
}

void DU_ThreadControl::yield()
{
    sched_yield();
}

DU_Thread::DU_Thread() : _running(false),_tid(-1)
{
}

void DU_Thread::threadEntry(DU_Thread *pThread)
{
    pThread->_running = true;

    {
        DU_ThreadLock::Lock sync(pThread->_lock);
        pThread->_lock.notifyAll();
    }

    try
    {
        pThread->run();
    }
    catch(...)
    {
        pThread->_running = false;
        throw;
    }
    pThread->_running = false;
}

DU_ThreadControl DU_Thread::start()
{
    DU_ThreadLock::Lock sync(_lock);

    if(_running)
    {
        throw DU_ThreadThreadControl_Exception("[DU_Thread::start] thread has start");
    }

    int ret = pthread_create(&_tid,
                   0,
                   (void *(*)(void *))&threadEntry,
                   (void *)this);

    if(ret != 0)
    {
        throw DU_ThreadThreadControl_Exception("[DU_Thread::start] thread start error", ret);
    }

    _lock.wait();

    return DU_ThreadControl(_tid);
}

DU_ThreadControl DU_Thread::getThreadControl() const
{
    return DU_ThreadControl(_tid);
}

bool DU_Thread::isAlive() const
{
    return _running;
}

