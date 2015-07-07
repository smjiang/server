#ifndef __MY_LOCK_H__
#define __MY_LOCK_H__

#ifdef WIN32 /* WIN32 */
#include <WinSock2.h>
#include <windows.h>
#else /* posix */
#include <pthread.h>
#endif /* posix end */

class CLock
{
public:
	CLock();
	~CLock();

	void Lock();
	void Unlock();

private:

#ifdef WIN32 /* WIN32 */
	CRITICAL_SECTION	m_cs;
#else /* posix */
	pthread_mutex_t		m_cs;
#endif /* posix end */

	CLock(const CLock&);
	CLock& operator = (const CLock&);
};

class CAutoLock
{
public:
	CAutoLock(CLock &lock);
	~CAutoLock();

private:
	CLock	&m_lock;
};

#endif

