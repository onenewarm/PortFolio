#include "Profiler.h"
#include <iostream>
#include <windows.h>
#include <wchar.h>
#include <stdint.h>
#include <unordered_map>
#include <intrin.h>

using namespace std;

//#define RDTSCP

namespace onenewarm
{
#define PROFILE_MAX_IDX 10000

	/*--------------------------------------------
	
	**PROFILE_SAMPLE**

	Profile의 통계를 내는 데이터를 모아둔다.
	szName에 태그이름을 저장한다.
	lStartTime에 PRO_BEGIN을 호출 한 시점의 시간을 저장한다.
	PRO_END에서 구한 시간에서 lStartTime을 빼서 한 번의 쌍이 끝난 시간을 계산한다.
	계산한 값은 iTotalTime에 합산된다.
	iMin, iMax는 위에서 구한 시간의 최소, 최댓값을 뜻한다.
	이 값은 2개씩 구해지며 이 값들은 프로파일링 대상에서 제외된다.
	iCall은 호출 횟수로 누적된 iTotalTime 값을 이 값으로 나눔으로써 결과를 출력한다.
	
	--------------------------------------------*/

	struct PROFILE_SAMPLE
	{
		WCHAR			szName[64];			// 프로파일 샘플 이름.

#ifdef RDTSCP
		unsigned __int64 iStartTime;
#else
		LARGE_INTEGER	lStartTime;			// 프로파일 샘플 실행 시간.
#endif
		__int64			iTotalTime;			// 전체 사용시간 카운터 Time.	(출력시 호출회수로 나누어 평균 구함)
		__int64			iMin[2];			// 최소 사용시간 카운터 Time.	(초단위로 계산하여 저장 / [0] 가장최소 [1] 다음 최소 [2])
		__int64			iMax[2];			// 최대 사용시간 카운터 Time.	(초단위로 계산하여 저장 / [0] 가장최대 [1] 다음 최대 [2])

		__int64			iCall;				// 누적 호출 횟수.

	};

	/*---------------------------
	
	**ProfileManager**
	
	ProfileSample을 관리한다.

	----------------------------*/

	class ProfileManager
	{
	public:
		PROFILE_SAMPLE m_Samples[30]; // 태그로 구분 되는 PROFILE_SAMPLE 최대 30개
		int m_Size; // 태그가 다른 sample의 개수
		DWORD m_ThreadID; // 객체를 소유한 스레드ID
		bool m_ResetFlag;

	public:
		ProfileManager();
	};

	
	static LARGE_INTEGER s_Freq; // queryperformancefrequency 값
	__declspec(thread) DWORD tls_ProfileIdx; // 스레드 별 s_ProfileManager와 매핑에 사용되는 인덱스
	static ProfileManager s_ProfileManager[PROFILE_MAX_IDX]; // 스레드에게 할당 될 ProfileManager 최대 개수 50개(스레드)
	static DWORD s_ProfileIdx; // s_ProfileManager의 인덱스를 가리키는 값

	PROFILE_SAMPLE* ProfileSearch(const WCHAR* szName);
	void CreateFileName(WCHAR* dest, const WCHAR* dirName, int size);

	ProfileManager::ProfileManager() : m_Size(0), m_ResetFlag(false), m_ThreadID(0)
	{
#ifndef RDTSCP
		if (s_Freq.QuadPart == 0) QueryPerformanceFrequency(&s_Freq);
#endif

		for (int iCnt = 0; iCnt < _countof(m_Samples); ++iCnt)
		{
			m_Samples[iCnt].iMin[0] = { INT64_MAX };
			m_Samples[iCnt].iMin[1] = { INT64_MAX };
			m_Samples[iCnt].iMax[0] = { INT64_MIN };
			m_Samples[iCnt].iMax[1] = { INT64_MIN };
		}
	}

	PROFILE_SAMPLE* ProfileSearch(const WCHAR* szName)
	{
		int size = s_ProfileManager[tls_ProfileIdx - 1].m_Size;
		for (int iCnt = 0; iCnt < size; ++iCnt)
		{
			if (wcscmp(s_ProfileManager[tls_ProfileIdx - 1].m_Samples[iCnt].szName, szName) == 0)
			{
				return &s_ProfileManager[tls_ProfileIdx - 1].m_Samples[iCnt];
			}
		}
		return nullptr;
	}

	void ProfileBegin(const WCHAR* szName)
	{
		if (tls_ProfileIdx == 0)
		{
			tls_ProfileIdx = InterlockedIncrement(&s_ProfileIdx);
			s_ProfileManager[tls_ProfileIdx - 1].m_ThreadID = GetCurrentThreadId();
		}

		if (s_ProfileManager[tls_ProfileIdx - 1].m_ResetFlag)
		{
			for (int iCnt = 0; iCnt < s_ProfileManager[tls_ProfileIdx - 1].m_Size ; ++iCnt)
			{
				s_ProfileManager[tls_ProfileIdx - 1].m_Samples[iCnt].iCall = 0;
				s_ProfileManager[tls_ProfileIdx - 1].m_Samples[iCnt].iTotalTime = 0;
				s_ProfileManager[tls_ProfileIdx - 1].m_Samples[iCnt].szName[0] = 0;
				s_ProfileManager[tls_ProfileIdx - 1].m_Samples[iCnt].iMin[0] = { INT64_MAX };
				s_ProfileManager[tls_ProfileIdx - 1].m_Samples[iCnt].iMin[1] = { INT64_MAX };
				s_ProfileManager[tls_ProfileIdx - 1].m_Samples[iCnt].iMax[0] = { INT64_MIN };
				s_ProfileManager[tls_ProfileIdx - 1].m_Samples[iCnt].iMax[1] = { INT64_MIN };
			}
			s_ProfileManager[tls_ProfileIdx - 1].m_ResetFlag = false;
			s_ProfileManager[tls_ProfileIdx - 1].m_Size = 0;
		}

		PROFILE_SAMPLE* pProfileSample = ProfileSearch(szName);

#ifdef RDTSCP
		if (pProfileSample != nullptr && pProfileSample->iStartTime != 0)
			DebugBreak();
#else
		if (pProfileSample != nullptr && pProfileSample->lStartTime.QuadPart != 0)
			DebugBreak();
#endif

		
		if (pProfileSample == nullptr)
		{
			pProfileSample = &s_ProfileManager[tls_ProfileIdx - 1].m_Samples[s_ProfileManager[tls_ProfileIdx - 1].m_Size++];
			wcscpy_s(pProfileSample->szName, szName);
		}

#ifdef RDTSCP
		unsigned int ui;
		pProfileSample->iStartTime = __rdtscp(&ui);
#else
		QueryPerformanceCounter(&pProfileSample->lStartTime);
#endif
	}

	void ProfileEnd(const WCHAR* szName)
	{
#ifdef RDTSCP
		unsigned int ui;
		unsigned __int64 endTime;
		endTime = __rdtscp(&ui);
#else
		LARGE_INTEGER endTime;
		QueryPerformanceCounter(&endTime);
#endif


		PROFILE_SAMPLE* pProfileSample = ProfileSearch(szName);

		if (pProfileSample == nullptr)
			return;

		__int64 spendedTime;

#ifdef RDTSCP
		spendedTime = endTime - pProfileSample->iStartTime;
#else
		spendedTime = endTime.QuadPart - pProfileSample->lStartTime.QuadPart;
#endif

		pProfileSample->iTotalTime += spendedTime;

		for (int iCnt = 0; iCnt < 2; ++iCnt)
		{
			if (pProfileSample->iMax[iCnt] < spendedTime)
			{
				__int64 _prevMax = pProfileSample->iMax[iCnt];
				pProfileSample->iMax[iCnt] = spendedTime;

				if (_prevMax != INT64_MIN)
				{
					for (int minCnt = 0; minCnt < 2; ++minCnt)
					{
						if (pProfileSample->iMin[minCnt] > _prevMax)
						{
							pProfileSample->iMin[minCnt] = _prevMax;
							break;
						}
					}
				}
				
				break;
			}

			if (pProfileSample->iMin[iCnt] > spendedTime)
			{
				pProfileSample->iMin[iCnt] = spendedTime;
				break;
			}
		}

		pProfileSample->iCall++;

#ifdef RDTSCP
		pProfileSample->iStartTime = 0;
#else
		pProfileSample->lStartTime.QuadPart = 0;
#endif
	}

	void CreateFileName(WCHAR* dest, const WCHAR* dirName, int size)
	{
		struct tm newtime;
		__time64_t long_time;

		_time64(&long_time);

		_localtime64_s(&newtime, &long_time);

		WCHAR fileName[256];

		wsprintf(fileName, L".\\%s\\PROFILE_%d%d%d_%d%d%d.txt", dirName, (newtime.tm_year + 1900),
			newtime.tm_mon + 1, newtime.tm_mday, newtime.tm_hour, newtime.tm_min, newtime.tm_sec);

		if (wcslen(fileName) > size)
		{
			wprintf(L"CreateFileName fault. Destination size is too small.");
			return;
		}

		wcscpy_s(dest, size, fileName);

		return;
	}

	void ProfileDataOutText(const WCHAR* szFileName)
	{
		WCHAR fileName[256];
		CreateFileName(fileName, szFileName, 256);

		FILE* fp = NULL;

		errno_t openErr = _wfopen_s(&fp, fileName, L"w+, ccs = UTF-16LE");

		if (openErr != 0 || fp == NULL)
		{
			wcout << openErr;
			return;
		}


		for (DWORD iCnt = 0; iCnt < s_ProfileIdx; ++iCnt)
		{
			// 표 헤더 출력
			fwprintf(fp, L"------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------\n");

#ifdef RDTSCP
			fwprintf(fp, L"    ThreadID     |        Tag         |        Average Time (CLK)        |          Min Time (CLK)          |            Max Time (CLK)             |        Calls        \n");
			fwprintf(fp, L"------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------\n");
#else
			fwprintf(fp, L"    ThreadID    |        Tag         |         Average Time (㎲)          |          Min Time (㎲)           |          Max Time (㎲)           |        Calls        \n");
			fwprintf(fp, L"------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------\n");
#endif
			for (int iSample = 0; iSample < s_ProfileManager[iCnt].m_Size; ++iSample)
			{
				__int64 totalTime = 
					s_ProfileManager[iCnt].m_Samples[iSample].iTotalTime 
					- s_ProfileManager[iCnt].m_Samples[iSample].iMax[0] - s_ProfileManager[iCnt].m_Samples[iSample].iMax[1]
					- s_ProfileManager[iCnt].m_Samples[iSample].iMin[0] - s_ProfileManager[iCnt].m_Samples[iSample].iMin[1];

				

#ifdef RDTSCP
				// CLK 단위로 출력
				fwprintf(fp, L"%#14x   | %-16s | %30lld(CLK)  | %30lld(CLK)  | %30lld(CLK)  | %20lld  \n",
					s_ProfileManager[iCnt].m_ThreadID,
					s_ProfileManager[iCnt].m_Samples[iSample].szName,
					totalTime / s_ProfileManager[iCnt].m_Samples[iSample].iCall,
					s_ProfileManager[iCnt].m_Samples[iSample].iMin[0],
					s_ProfileManager[iCnt].m_Samples[iSample].iMax[0],
					s_ProfileManager[iCnt].m_Samples[iSample].iCall);
#else
				// 마이크로세컨드(㎲) 단위로 출력
				long double microTotalTime;

				if (totalTime < 0) microTotalTime = -1;
				else
				{
					microTotalTime = (long double)totalTime / (s_ProfileManager[iCnt].m_Samples[iSample].iCall - 4) / s_Freq.QuadPart * 1000000;
				}

				fwprintf(fp, L"%#14x   | %-16s | %30.4Lf㎲    | %30.4Lf㎲    | %30.4Lf㎲    | %20lld  \n",
					s_ProfileManager[iCnt].m_ThreadID,
					s_ProfileManager[iCnt].m_Samples[iSample].szName,
					microTotalTime,
					(long double)s_ProfileManager[iCnt].m_Samples[iSample].iMin[0] / s_Freq.QuadPart * 1000000,
					(long double)s_ProfileManager[iCnt].m_Samples[iSample].iMax[0] / s_Freq.QuadPart * 1000000,
					s_ProfileManager[iCnt].m_Samples[iSample].iCall);
#endif


				
			}
			fwprintf(fp, L"------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------\n\n\n");
		}

		fclose(fp);
	}

	void ProfileReset(void)
	{
		for (DWORD iCnt = 0; iCnt < s_ProfileIdx; ++iCnt)
		{
			s_ProfileManager[iCnt].m_ResetFlag = true;
		}
	}

}

