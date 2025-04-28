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

	Profile�� ��踦 ���� �����͸� ��Ƶд�.
	szName�� �±��̸��� �����Ѵ�.
	lStartTime�� PRO_BEGIN�� ȣ�� �� ������ �ð��� �����Ѵ�.
	PRO_END���� ���� �ð����� lStartTime�� ���� �� ���� ���� ���� �ð��� ����Ѵ�.
	����� ���� iTotalTime�� �ջ�ȴ�.
	iMin, iMax�� ������ ���� �ð��� �ּ�, �ִ��� ���Ѵ�.
	�� ���� 2���� �������� �� ������ �������ϸ� ��󿡼� ���ܵȴ�.
	iCall�� ȣ�� Ƚ���� ������ iTotalTime ���� �� ������ �������ν� ����� ����Ѵ�.
	
	--------------------------------------------*/

	struct PROFILE_SAMPLE
	{
		WCHAR			szName[64];			// �������� ���� �̸�.

#ifdef RDTSCP
		unsigned __int64 iStartTime;
#else
		LARGE_INTEGER	lStartTime;			// �������� ���� ���� �ð�.
#endif
		__int64			iTotalTime;			// ��ü ���ð� ī���� Time.	(��½� ȣ��ȸ���� ������ ��� ����)
		__int64			iMin[2];			// �ּ� ���ð� ī���� Time.	(�ʴ����� ����Ͽ� ���� / [0] �����ּ� [1] ���� �ּ� [2])
		__int64			iMax[2];			// �ִ� ���ð� ī���� Time.	(�ʴ����� ����Ͽ� ���� / [0] �����ִ� [1] ���� �ִ� [2])

		__int64			iCall;				// ���� ȣ�� Ƚ��.

	};

	/*---------------------------
	
	**ProfileManager**
	
	ProfileSample�� �����Ѵ�.

	----------------------------*/

	class ProfileManager
	{
	public:
		PROFILE_SAMPLE m_Samples[30]; // �±׷� ���� �Ǵ� PROFILE_SAMPLE �ִ� 30��
		int m_Size; // �±װ� �ٸ� sample�� ����
		DWORD m_ThreadID; // ��ü�� ������ ������ID
		bool m_ResetFlag;

	public:
		ProfileManager();
	};

	
	static LARGE_INTEGER s_Freq; // queryperformancefrequency ��
	__declspec(thread) DWORD tls_ProfileIdx; // ������ �� s_ProfileManager�� ���ο� ���Ǵ� �ε���
	static ProfileManager s_ProfileManager[PROFILE_MAX_IDX]; // �����忡�� �Ҵ� �� ProfileManager �ִ� ���� 50��(������)
	static DWORD s_ProfileIdx; // s_ProfileManager�� �ε����� ����Ű�� ��

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
			// ǥ ��� ���
			fwprintf(fp, L"------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------\n");

#ifdef RDTSCP
			fwprintf(fp, L"    ThreadID     |        Tag         |        Average Time (CLK)        |          Min Time (CLK)          |            Max Time (CLK)             |        Calls        \n");
			fwprintf(fp, L"------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------\n");
#else
			fwprintf(fp, L"    ThreadID    |        Tag         |         Average Time (��)          |          Min Time (��)           |          Max Time (��)           |        Calls        \n");
			fwprintf(fp, L"------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------\n");
#endif
			for (int iSample = 0; iSample < s_ProfileManager[iCnt].m_Size; ++iSample)
			{
				__int64 totalTime = 
					s_ProfileManager[iCnt].m_Samples[iSample].iTotalTime 
					- s_ProfileManager[iCnt].m_Samples[iSample].iMax[0] - s_ProfileManager[iCnt].m_Samples[iSample].iMax[1]
					- s_ProfileManager[iCnt].m_Samples[iSample].iMin[0] - s_ProfileManager[iCnt].m_Samples[iSample].iMin[1];

				

#ifdef RDTSCP
				// CLK ������ ���
				fwprintf(fp, L"%#14x   | %-16s | %30lld(CLK)  | %30lld(CLK)  | %30lld(CLK)  | %20lld  \n",
					s_ProfileManager[iCnt].m_ThreadID,
					s_ProfileManager[iCnt].m_Samples[iSample].szName,
					totalTime / s_ProfileManager[iCnt].m_Samples[iSample].iCall,
					s_ProfileManager[iCnt].m_Samples[iSample].iMin[0],
					s_ProfileManager[iCnt].m_Samples[iSample].iMax[0],
					s_ProfileManager[iCnt].m_Samples[iSample].iCall);
#else
				// ����ũ�μ�����(��) ������ ���
				long double microTotalTime;

				if (totalTime < 0) microTotalTime = -1;
				else
				{
					microTotalTime = (long double)totalTime / (s_ProfileManager[iCnt].m_Samples[iSample].iCall - 4) / s_Freq.QuadPart * 1000000;
				}

				fwprintf(fp, L"%#14x   | %-16s | %30.4Lf��    | %30.4Lf��    | %30.4Lf��    | %20lld  \n",
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

