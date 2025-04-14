#pragma once
#include <wchar.h>
#include <windows.h>

//#define PROFILE
//#define RDTSCP

namespace onenewarm
{

/*------------------------------

��� ����

PRO_BEGIN(L"�±��̸�");
[������ ���ϴ� �ڵ� �κ�]
PRO_END(L"�±��̸�");

������ ���ϴ� �ڵ� �κ��� �ɸ� �ð��� QueryPerformanceCounter�� �̿��Ͽ� �����Ѵ�.

��� ����� ���ϴ� ���
ProfileDataOutText(L"���ϰ��"); �� �̿��Ͽ� ����� ���� �� �ִ�.

-------------------------------*/

	void ProfileBegin(const WCHAR* szName);
	void ProfileEnd(const WCHAR* szName);
	void ProfileDataOutText(const WCHAR* szFileName);
	void ProfileReset(void);

#ifdef PROFILE
#define PRO_BEGIN(TagName)				\
	do {								\
		ProfileBegin(TagName);			\
	}while(0)							\

#define PRO_END(TagName)				\
	do {								\
		ProfileEnd(TagName); 			\
	}while(0)							\

#else
#define PRO_BEGIN(TagName)
#define PRO_END(TagName)
#endif

}
