#pragma once
#include <Windows.h>
#include <synchapi.h>

class CSRWLOCK
{
public:
	void Lock();
	void UnLock();
	CSRWLOCK() { InitializeSRWLock(&_lock); }
private:
	SRWLOCK _lock;
};

