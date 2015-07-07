#include "CLock.h"

CLock::CLock()
{
#ifdef WIN32
	InitializeCriticalSection(&m_cs);
#else
	pthread_mutexattr_t lock_attr; /* silver add */

	pthread_mutexattr_init(&lock_attr); /* silver add */
	pthread_mutexattr_settype(&lock_attr, PTHREAD_MUTEX_RECURSIVE); /* silver add */

	pthread_mutex_init(&m_cs, &lock_attr);

	pthread_mutexattr_destroy(&lock_attr); /* silver add */
#endif
}

CLock::~CLock()
{
#ifdef WIN32
	DeleteCriticalSection(&m_cs);
#else
	pthread_mutex_destroy(&m_cs);
#endif
}

void CLock::Lock()
{
#ifdef WIN32
	EnterCriticalSection(&m_cs);
#else
	pthread_mutex_lock(&m_cs);
#endif
}

void CLock::Unlock()
{
#ifdef WIN32
	LeaveCriticalSection(&m_cs);
#else
	pthread_mutex_unlock(&m_cs);
#endif
}

///////////////////////////////////////////////////////////////////////////////
CAutoLock::CAutoLock(CLock &lock) : m_lock(lock)
{
	m_lock.Lock();
}

CAutoLock::~CAutoLock()
{
	m_lock.Unlock();
}



