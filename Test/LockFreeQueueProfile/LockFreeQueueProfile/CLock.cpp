#include <windows.h>
#include "CLock.h"


#pragma comment(lib, "Synchronization.lib")

using namespace std;

void CLock::Lock()
{
	while (1)
	{
		if (InterlockedExchange(&_lock, 1) == 0) return;

		WaitOnAddress(&_lock, &_compareVal, 4, INFINITE);
	}
	
}

void CLock::UnLock()
{
	InterlockedExchange(&_lock, 0);
	WakeByAddressSingle(&_lock);
}
