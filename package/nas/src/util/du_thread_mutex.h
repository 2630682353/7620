#ifndef __DU_THREAD_MUTEX_H
#define __DU_THREAD_MUTEX_H

#include "du_lock.h"


/////////////////////////////////////////////////
/** 
 * @file du_thread_mutex.h 
 * @brief 线程锁互斥类(修改至ICE源码). 
 */
             
/////////////////////////////////////////////////
class DU_ThreadCond;

/**
 * @brief 线程互斥对象
 */
struct DU_ThreadMutex_Exception : public DU_Lock_Exception
{
    DU_ThreadMutex_Exception(const string &buffer) : DU_Lock_Exception(buffer){};
    DU_ThreadMutex_Exception(const string &buffer, int err) : DU_Lock_Exception(buffer, err){};
    ~DU_ThreadMutex_Exception() throw() {};
};

/**
* @brief 线程锁 . 
*  
* 不可重复加锁，即同一个线程不可以重复加锁 
*  
* 通常不直接使用，和DU_Monitor配合使用，即DU_ThreadLock; 
*/
class DU_ThreadMutex
{
public:

    DU_ThreadMutex();
    virtual ~DU_ThreadMutex();

    /**
     * @brief 加锁
     */
    void lock() const;

    /**
     * @brief 尝试锁
     * 
     * @return bool
     */
    bool tryLock() const;

    /**
     * @brief 解锁
     */
    void unlock() const;

    /**
	 * @brief 加锁后调用unlock是否会解锁， 
	 *  	  给DU_Monitor使用的 永远返回true
     * @return bool
     */
    bool willUnlock() const { return true;}

protected:

    // noncopyable
    DU_ThreadMutex(const DU_ThreadMutex&);
    void operator=(const DU_ThreadMutex&);

	/**
     * @brief 计数
	 */
    int count() const;

    /**
     * @brief 计数
	 */
    void count(int c) const;

    friend class DU_ThreadCond;

protected:
    mutable pthread_mutex_t _mutex;

};

/**
* @brief 线程锁类. 
*  
* 采用线程库实现
**/
class DU_ThreadRecMutex
{
public:

    /**
    * @brief 构造函数
    */
    DU_ThreadRecMutex();

    /**
    * @brief 析够函数
    */
    virtual ~DU_ThreadRecMutex();

    /**
	* @brief 锁, 调用pthread_mutex_lock. 
	*  
    * return : 返回pthread_mutex_lock的返回值
    */
    int lock() const;

    /**
	* @brief 解锁, pthread_mutex_unlock. 
	*  
    * return : 返回pthread_mutex_lock的返回值
    */
    int unlock() const;

    /**
	* @brief 尝试锁, 失败抛出异常. 
	*  
    * return : true, 成功锁; false 其他线程已经锁了
    */
    bool tryLock() const;

    /**
     * @brief 加锁后调用unlock是否会解锁, 给DU_Monitor使用的
     * 
     * @return bool
     */
    bool willUnlock() const;
protected:

	/**
     * @brief 友元类
     */
    friend class DU_ThreadCond;

	/**
     * @brief 计数
	 */
    int count() const;

    /**
     * @brief 计数
	 */
    void count(int c) const;

private:
    /**
    锁对象
    */
    mutable pthread_mutex_t _mutex;
	mutable int _count;
};

#endif

