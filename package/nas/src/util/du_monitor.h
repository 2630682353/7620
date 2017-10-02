#ifndef _DU_MONITOR_H
#define _DU_MONITOR_H

#include "du_thread_mutex.h"
#include "du_thread_cond.h"

/////////////////////////////////////////////////
/** 
 * @file du_monitor.h 
 * @brief 锁监控器类(修改至ICE源码).  
 */           
/////////////////////////////////////////////////
/**
 * @brief 线程锁监控模板类.
 * 通常线程锁，都通过该类来使用，而不是直接用DU_ThreadMutex、DU_ThreadRecMutex 
 *  
 * 该类将DU_ThreadMutex/DU_ThreadRecMutex 与DU_ThreadCond结合起来； 
 */
template <class T, class P>
class DU_Monitor
{
public:

    /**
     * @brief 定义锁控制对象
     */
    typedef DU_LockT<DU_Monitor<T, P> > Lock;
    typedef DU_TryLockT<DU_Monitor<T, P> > TryLock;

    /**
     * @brief 构造函数
     */
    DU_Monitor() : _nnotify(0)
    {
    }

    /**
     * @brief 析够
     */
    virtual ~DU_Monitor()
    {
    }

    /**
     * @brief 锁
     */
    void lock() const
    {
        _mutex.lock();
        _nnotify = 0;
    }

    /**
     * @brief 解锁, 根据上锁的次数通知
     */
    void unlock() const
    {
        notifyImpl(_nnotify);
        _mutex.unlock();
    }

    /**
     * @brief 尝试锁.
     *
     * @return bool
     */
    bool tryLock() const
    {
        bool result = _mutex.tryLock();
        if(result)
        {
            _nnotify = 0;
        }
        return result;
    }

    /**
     * @brief 等待,当前调用线程在锁上等待，直到事件通知，
     */
    void wait() const
    {
        notifyImpl(_nnotify);

        try
        {
            _cond.wait(_mutex);
        }
        catch(...)
        {
            _nnotify = 0;
            throw;
        }

        _nnotify = 0;
    }

    /**
	 * @brief 等待时间,当前调用线程在锁上等待，直到超时或有事件通知
	 *  
	 * @param millsecond 等待时间
     * @return           false:超时了, ture:有事件来了
     */
    bool timedWait(int millsecond) const
    {
        notifyImpl(_nnotify);

        bool rc;

        try
        {
            rc = _cond.timedWait(_mutex, millsecond);
        }
        catch(...)
        {
            _nnotify = 0;
            throw;
        }

        _nnotify = 0;
        return rc;
    }

    /**
	 * @brief 通知某一个线程醒来 
	 *  
	 * 通知等待在该锁上某一个线程醒过来 ,调用该函数之前必须加锁, 
	 *  
	 * 在解锁的时候才真正通知 
     */
    void notify()
    {
        if(_nnotify != -1)
        {
            ++_nnotify;
        }
    }

    /**
	 * @brief 通知等待在该锁上的所有线程醒过来，
	 * 注意调用该函数时必须已经获得锁.
	 *  
	 * 该函数调用前之必须加锁, 在解锁的时候才真正通知 
     */
    void notifyAll()
    {
        _nnotify = -1;
    }

protected:

    /**
	 * @brief 通知实现. 
	 *  
     * @param nnotify 上锁的次数
     */
    void notifyImpl(int nnotify) const
    {
        if(nnotify != 0)
        {
            if(nnotify == -1)
            {
                _cond.broadcast();
                return;
            }
            else
            {
                while(nnotify > 0)
                {
                    _cond.signal();
                    --nnotify;
                }
            }
        }
    }

private:

	 /** 
	  * @brief noncopyable
	  */
    DU_Monitor(const DU_Monitor&);
    void operator=(const DU_Monitor&);

protected:

	/**
	 * 上锁的次数
	 */
    mutable int     _nnotify;
    mutable P       _cond;
    T               _mutex;
};

/**
 * 普通线程锁
 */
typedef DU_Monitor<DU_ThreadMutex, DU_ThreadCond> DU_ThreadLock;

/**
 * 循环锁(一个线程可以加多次锁)
 */
typedef DU_Monitor<DU_ThreadRecMutex, DU_ThreadCond> DU_ThreadRecLock;

#endif

