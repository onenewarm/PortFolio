#pragma once
#include <wchar.h>
#include <windows.h>

//#define PROFILE
//#define RDTSCP

namespace onenewarm
{

/*------------------------------

사용 예시

PRO_BEGIN(L"태그이름");
[측정을 원하는 코드 부분]
PRO_END(L"태그이름");

측정을 원하는 코드 부분이 걸린 시간을 QueryPerformanceCounter를 이용하여 측정한다.

결과 출력을 원하는 경우
ProfileDataOutText(L"파일경로"); 를 이용하여 결과를 받을 수 있다.

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
