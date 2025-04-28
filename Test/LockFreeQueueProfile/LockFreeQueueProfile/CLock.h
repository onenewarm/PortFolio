#pragma once

class CLock
{
public:
	void Lock();
	void UnLock();
	CLock() : _lock(0), _compareVal(1) {}
private:
	unsigned int _lock;
	unsigned int _compareVal;
};