#pragma once

class SpinLock
{
public:
	SpinLock();

	void Lock();
	void UnLock();

private:
	long _mlock;

};