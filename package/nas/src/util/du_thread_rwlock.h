#ifndef __DU_THREAD_RWLOCK_H
#define __DU_THREAD_RWLOCK_H

#include <pthread.h>
#include "du_lock.h"

#if !defined(linux) && (defined(__linux) || defined(__linux__))
#define linux
#endif

using namespace std;


/////////////////////////////////////////////////
/**
 * @file du_thread_rwlock.h
 * @brief 读写锁
 *
 */

/////////////////////////////////////////////////

/**
 * @brief读写锁异常类
 */
struct DU_ThreadRW_Exception : public DU_Exception
{
    DU_ThreadRW_Exception(const string &buffer) : DU_Exception(buffer){};
    DU_ThreadRW_Exception(const string &buffer, int err) : DU_Exception(buffer, err){};
    ~DU_ThreadRW_Exception() throw() {};
};


#if !defined(linux) || (defined __USE_UNIX98 || defined __USE_XOPEN2K)
class DU_ThreadRWLocker
{
public:
	/**
     * @brief 构造函数
     */
	DU_ThreadRWLocker();

    /**
     * @brief 析够函数
     */
	~DU_ThreadRWLocker();

	/**
	 *@brief 读锁定,调用pthread_rwlock_rdlock
	 *return : 失败则抛异常DU_ThreadRW_Exception
	 */
	void ReadLock() const;

	/**
	 *@brief 写锁定,调用pthread_rwlock_wrlock
	 *return : 失败则抛异常DU_ThreadRW_Exception
	 */
	void WriteLock() const;

	/**
	 *@brief 尝试读锁定,调用pthread_rwlock_tryrdlock
	 *return : 失败则抛异常DU_ThreadRW_Exception
	 */
	void TryReadLock() const;

	/**
	 *@brief 尝试写锁定,调用pthread_rwlock_trywrlock
	 *return : 失败则抛异常DU_ThreadRW_Exception
	 */
	void TryWriteLock() const ;

	/**
	 *@brief 解锁,调用pthread_rwlock_unlock
	 *return : 失败则抛异常DU_ThreadRW_Exception
	 */
	void Unlock() const;

private:

	mutable pthread_rwlock_t m_sect;

	// noncopyable
	DU_ThreadRWLocker(const DU_ThreadRWLocker&);
	DU_ThreadRWLocker& operator=(const DU_ThreadRWLocker&);

};

typedef DU_RW_RLockT<DU_ThreadRWLocker> DU_ThreadRLock;
typedef DU_RW_WLockT<DU_ThreadRWLocker> DU_ThreadWLock;

#endif

#endif


