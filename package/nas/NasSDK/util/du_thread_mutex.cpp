#include "du_thread_mutex.h"
#include <string.h>
#include <iostream>
#include <cassert>


DU_ThreadMutex::DU_ThreadMutex()
{
    int rc;
    pthread_mutexattr_t attr;
    rc = pthread_mutexattr_init(&attr);
    assert(rc == 0);

    rc = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);
    assert(rc == 0);

    rc = pthread_mutex_init(&_mutex, &attr);
    assert(rc == 0);

    rc = pthread_mutexattr_destroy(&attr);
    assert(rc == 0);

    if(rc != 0)
    {
        throw DU_ThreadMutex_Exception("[DU_ThreadMutex::DU_ThreadMutex] pthread_mutexattr_init error", rc);
    }
}

DU_ThreadMutex::~DU_ThreadMutex()
{
    int rc = 0;
    rc = pthread_mutex_destroy(&_mutex);
    if(rc != 0)
    {
        cerr << "[DU_ThreadMutex::~DU_ThreadMutex] pthread_mutex_destroy error:" << string(strerror(rc)) << endl;
    }
//    assert(rc == 0);
}

void DU_ThreadMutex::lock() const
{
    int rc = pthread_mutex_lock(&_mutex);
    if(rc != 0)
    {
        if(rc == EDEADLK)
    	{
            throw DU_ThreadMutex_Exception("[DU_ThreadMutex::lock] pthread_mutex_lock dead lock error", rc);
    	}
    	else
    	{
            throw DU_ThreadMutex_Exception("[DU_ThreadMutex::lock] pthread_mutex_lock error", rc);
    	}
    }
}

bool DU_ThreadMutex::tryLock() const
{
    int rc = pthread_mutex_trylock(&_mutex);
    if(rc != 0 && rc != EBUSY)
    {
        if(rc == EDEADLK)
    	{
            throw DU_ThreadMutex_Exception("[DU_ThreadMutex::tryLock] pthread_mutex_trylock dead lock error", rc);
    	}
    	else
    	{
            throw DU_ThreadMutex_Exception("[DU_ThreadMutex::tryLock] pthread_mutex_trylock error", rc);
    	}
    }
    return (rc == 0);
}

void DU_ThreadMutex::unlock() const
{
    int rc = pthread_mutex_unlock(&_mutex);
    if(rc != 0)
    {
        throw DU_ThreadMutex_Exception("[DU_ThreadMutex::unlock] pthread_mutex_unlock error", rc);
    }
}

int DU_ThreadMutex::count() const
{
    return 0;
}

void DU_ThreadMutex::count(int c) const
{
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DU_ThreadRecMutex::DU_ThreadRecMutex()
: _count(0)
{
    int rc;

    pthread_mutexattr_t attr;
    rc = pthread_mutexattr_init(&attr);
    if(rc != 0)
    {
		throw DU_ThreadMutex_Exception("[DU_ThreadRecMutex::DU_ThreadRecMutex] pthread_mutexattr_init error", rc);
    }
    rc = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    if(rc != 0)
    {
		throw DU_ThreadMutex_Exception("[DU_ThreadRecMutex::DU_ThreadRecMutex] pthread_mutexattr_settype error", rc);
    }

    rc = pthread_mutex_init(&_mutex, &attr);
    if(rc != 0)
    {
		throw DU_ThreadMutex_Exception("[DU_ThreadRecMutex::DU_ThreadRecMutex] pthread_mutex_init error", rc);
    }

    rc = pthread_mutexattr_destroy(&attr);
    if(rc != 0)
    {
		throw DU_ThreadMutex_Exception("[DU_ThreadRecMutex::DU_ThreadRecMutex] pthread_mutexattr_destroy error", rc);
    }
}

DU_ThreadRecMutex::~DU_ThreadRecMutex()
{
	while (_count)
	{
		unlock();
	}

    int rc = 0;
    rc = pthread_mutex_destroy(&_mutex);
    if(rc != 0)
    {
        cerr << "[DU_ThreadRecMutex::~DU_ThreadRecMutex] pthread_mutex_destroy error:" << string(strerror(rc)) << endl;
    }
//    assert(rc == 0);
}

int DU_ThreadRecMutex::lock() const
{
    int rc = pthread_mutex_lock(&_mutex);
    if(rc != 0)
    {
		throw DU_ThreadMutex_Exception("[DU_ThreadRecMutex::lock] pthread_mutex_lock error", rc);
    }

    if(++_count > 1)
    {
		rc = pthread_mutex_unlock(&_mutex);
		assert(rc == 0);
    }

	return rc;
}

int DU_ThreadRecMutex::unlock() const
{
    if(--_count == 0)
    {
		int rc = 0;
		rc = pthread_mutex_unlock(&_mutex);
		return rc;
    }
	return 0;
}

bool DU_ThreadRecMutex::tryLock() const
{
    int rc = pthread_mutex_trylock(&_mutex);
    if(rc != 0 )
    {
		if(rc != EBUSY)
		{
			throw DU_ThreadMutex_Exception("[DU_ThreadRecMutex::tryLock] pthread_mutex_trylock error", rc);
		}
    }
    else if(++_count > 1)
    {
        rc = pthread_mutex_unlock(&_mutex);
        if(rc != 0)
        {
            throw DU_ThreadMutex_Exception("[DU_ThreadRecMutex::tryLock] pthread_mutex_unlock error", rc);
        }
    }
    return (rc == 0);
}

bool DU_ThreadRecMutex::willUnlock() const
{
    return _count == 1;
}

int DU_ThreadRecMutex::count() const
{
    int c   = _count;
    _count  = 0;
    return c;
}

void DU_ThreadRecMutex::count(int c) const
{
    _count = c;
}


