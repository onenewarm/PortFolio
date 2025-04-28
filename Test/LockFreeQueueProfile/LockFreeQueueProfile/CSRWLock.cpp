#include "CSRWLOCK.h"

void CSRWLOCK::Lock()
{
	AcquireSRWLockExclusive(&_lock);
}

void CSRWLOCK::UnLock()
{
	ReleaseSRWLockExclusive(&_lock);
}
