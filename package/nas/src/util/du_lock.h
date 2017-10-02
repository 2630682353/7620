#ifndef _DU_LOCK_H
#define _DU_LOCK_H

#include <string>
#include <stdexcept>
#include <cerrno>
#include "du_ex.h"

using namespace std;

/////////////////////////////////////////////////
/**
 * @file du_lock.h 
 * @brief  锁类
 */           
/////////////////////////////////////////////////


/**
* @brief  锁异常
*/
struct DU_Lock_Exception : public DU_Exception
{
    DU_Lock_Exception(const string &buffer) : DU_Exception(buffer){};
    DU_Lock_Exception(const string &buffer, int err) : DU_Exception(buffer, err){};
    ~DU_Lock_Exception() throw() {};
};

/**
 * @brief  锁模板类其他具体锁配合使用，
 * 构造时候加锁，析够的时候解锁
 */
template <typename T>
class DU_LockT
{
public:

    /**
	 * @brief  构造函数，构造时枷锁
	 *  
     * @param mutex 锁对象
     */
    DU_LockT(const T& mutex) : _mutex(mutex)
    {
        _mutex.lock();
        _acquired = true;
    }

    /**
     * @brief  析构，析构时解锁
     */
    virtual ~DU_LockT()
    {
        if (_acquired)
        {
            _mutex.unlock();
        }
    }

    /**
     * @brief  上锁, 如果已经上锁,则抛出异常
     */
    void acquire() const
    {
        if (_acquired)
        {
            throw DU_Lock_Exception("thread has locked!");
        }
        _mutex.lock();
        _acquired = true;
    }

    /**
     * @brief  尝试上锁.
     *
     * @return  成功返回true，否则返回false
     */
    bool tryAcquire() const
    {
        _acquired = _mutex.tryLock();
        return _acquired;
    }

    /**
     * @brief  释放锁, 如果没有上过锁, 则抛出异常
     */
    void release() const
    {
        if (!_acquired)
        {
            throw DU_Lock_Exception("thread hasn't been locked!");
        }
        _mutex.unlock();
        _acquired = false;
    }

    /**
     * @brief  是否已经上锁.
     *
     * @return  返回true已经上锁，否则返回false
     */
    bool acquired() const
    {
        return _acquired;
    }

protected:

    /**
	 * @brief 构造函数
	 * 用于锁尝试操作，与DU_LockT相似
	 *  
     */
    DU_LockT(const T& mutex, bool) : _mutex(mutex)
    {
        _acquired = _mutex.tryLock();
    }

private:

    // Not implemented; prevents accidental use.
    DU_LockT(const DU_LockT&);
    DU_LockT& operator=(const DU_LockT&);

protected:
    /**
     * 锁对象
     */
    const T&        _mutex;

    /**
     * 是否已经上锁
     */
    mutable bool _acquired;
};

/**
 * @brief  尝试上锁
 */
template <typename T>
class DU_TryLockT : public DU_LockT<T>
{
public:

    DU_TryLockT(const T& mutex) : DU_LockT<T>(mutex, true)
    {
    }
};

/**
 * @brief  空锁, 不做任何锁动作
 */
class DU_EmptyMutex
{
public:
    /**
	* @brief  写锁.
	*  
    * @return int, 0 正确
    */
    int lock()  const   {return 0;}

    /**
    * @brief  解写锁
    */
    int unlock() const  {return 0;}

    /**
	* @brief  尝试解锁. 
	*  
    * @return int, 0 正确
    */
    bool trylock() const {return true;}
};

/**
 * @brief  读写锁读锁模板类
 * 构造时候加锁，析够的时候解锁
 */

template <typename T>
class DU_RW_RLockT
{
public:
    /**
	 * @brief  构造函数，构造时枷锁
	 *
     * @param lock 锁对象
     */
	DU_RW_RLockT(T& lock)
		: _rwLock(lock),_acquired(false)
	{
		_rwLock.ReadLock();
		_acquired = true;
	}

    /**
	 * @brief 析构时解锁
     */
	~DU_RW_RLockT()
	{
		if (_acquired)
		{
			_rwLock.Unlock();
		}
	}
private:
	/**
	 *锁对象
	 */
	const T& _rwLock;

    /**
     * 是否已经上锁
     */
    mutable bool _acquired;

	DU_RW_RLockT(const DU_RW_RLockT&);
	DU_RW_RLockT& operator=(const DU_RW_RLockT&);
};

template <typename T>
class DU_RW_WLockT
{
public:
    /**
	 * @brief  构造函数，构造时枷锁
	 *
     * @param lock 锁对象
     */
	DU_RW_WLockT(T& lock)
		: _rwLock(lock),_acquired(false)
	{
		_rwLock.WriteLock();
		_acquired = true;
	}
    /**
	 * @brief 析构时解锁
     */
	~DU_RW_WLockT()
	{
		if(_acquired)
		{
			_rwLock.Unlock();
		}
	}
private:
	/**
	 *锁对象
	 */
	const T& _rwLock;
    /**
     * 是否已经上锁
     */
    mutable bool _acquired;

	DU_RW_WLockT(const DU_RW_WLockT&);
	DU_RW_WLockT& operator=(const DU_RW_WLockT&);
};

#endif

