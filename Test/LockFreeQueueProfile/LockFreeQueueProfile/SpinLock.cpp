#include <Windows.h>
#include "SpinLock.h"
#include <iostream>


SpinLock::SpinLock() : _mlock(0)
{
}

void SpinLock::Lock()
{
	while (1)
	{
		if (InterlockedExchange(&_mlock, 1) == 0) return;

		for(int cnt = 0 ; cnt < 1024 ; ++cnt)
			YieldProcessor();
	}
}

void SpinLock::UnLock()
{
	InterlockedExchange(&_mlock, 0);
}
