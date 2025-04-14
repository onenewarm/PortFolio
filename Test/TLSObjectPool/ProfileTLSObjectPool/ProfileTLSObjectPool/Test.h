#pragma once
#include "ObjectPool.hpp"
#include <iostream>
#include "TestData.h"
#include <process.h>
#include <vector>

using namespace onenewarm;

#define COUNT_OF_THREADS 64
#define COUNT_OF_REPEAT 30000
#define COUNT_OF_LOOP 30
#define FILE_NAME_SIZE 256



struct ThreadParam
{
	HANDLE completeHandle;
	int dataSize;
	void* poolPtr;
};


HANDLE s_ThreadEvents[COUNT_OF_THREADS];
ThreadParam s_ThreadParams[COUNT_OF_THREADS];

LARGE_INTEGER s_AllocTotalProfile[COUNT_OF_THREADS];
LARGE_INTEGER s_FreeTotalProfile[COUNT_OF_THREADS];

vector<LARGE_INTEGER> s_TLSAllocProfile;
vector<LARGE_INTEGER> s_TLSFreeProfile;

vector<LARGE_INTEGER> s_NewProfile;
vector<LARGE_INTEGER> s_DeleteProfile;

HANDLE s_StartEvent;


void CreateAllocFileName(char* dest, int size)
{
	struct tm newtime;
	__time64_t long_time;

	_time64(&long_time);

	_localtime64_s(&newtime, &long_time);

	char fileName[FILE_NAME_SIZE];

	sprintf_s(fileName, ".\\alloc.csv");

	if (strlen(fileName) > size)
	{
		wprintf(L"CreateFileName fault. Destination size is too small.");
		return;
	}

	memcpy_s(dest, size, fileName, strlen(fileName) + 1);

	return;
}

void CreateFreeFileName(char* dest, int size)
{
	struct tm newtime;
	__time64_t long_time;

	_time64(&long_time);

	_localtime64_s(&newtime, &long_time);

	char fileName[FILE_NAME_SIZE];

	sprintf_s(fileName, ".\\free.csv");

	if (strlen(fileName) > size)
	{
		wprintf(L"CreateFileName fault. Destination size is too small.");
		return;
	}

	memcpy_s(dest, size, fileName, strlen(fileName) + 1);

	return;
}


void OutToCSV()
{
	char AllocFileName[FILE_NAME_SIZE] = { 0 };
	CreateAllocFileName(AllocFileName, FILE_NAME_SIZE);

	char FreeFileName[FILE_NAME_SIZE] = { 0 };
	CreateFreeFileName(FreeFileName, FILE_NAME_SIZE);

	LARGE_INTEGER freq;
	QueryPerformanceFrequency(&freq);

	FILE* allocFile; FILE* freeFile;

	errno_t openErr1 = fopen_s(&allocFile, AllocFileName, "w+");
	errno_t openErr2 = fopen_s(&freeFile, FreeFileName, "w+");
	
	if (openErr1 != 0 || openErr2 != 0 || allocFile == NULL || freeFile == NULL)
	{
		wcout << openErr1 << " " << openErr2;
		return;
	}

	// csv 헤더 입력
	fprintf(allocFile, ",");
	for (int cnt = 0; cnt < s_TLSAllocProfile.size() - 1 ; ++cnt)
	{
		fprintf(allocFile, "%d,", cnt + 1);
	}
	fprintf(allocFile, "%lld\n", s_TLSAllocProfile.size() - 1);
	
	fprintf(freeFile, ",");
	for (int cnt = 0; cnt < s_TLSAllocProfile.size() - 1; ++cnt)
	{
		fprintf(freeFile, "%d,", cnt + 1);
	}
	fprintf(freeFile, "%lld\n", s_TLSAllocProfile.size() - 1);

	// TLS ALLOC
	fprintf(allocFile, "TLS ALLOC,");
	for (int cnt = 0; cnt < s_TLSAllocProfile.size() - 1; ++cnt)
	{
		fprintf(allocFile, "%lld,", s_TLSAllocProfile[cnt].QuadPart);
	}
	fprintf(allocFile, "%lld\n", s_TLSAllocProfile[s_TLSAllocProfile.size() - 1].QuadPart);

	// New
	fprintf(allocFile, "New,");
	for (int cnt = 0; cnt < s_NewProfile.size() - 1; ++cnt)
	{
		fprintf(allocFile, "%lld,", s_NewProfile[cnt].QuadPart);
	}
	fprintf(allocFile, "%lld\n", s_NewProfile[s_NewProfile.size() - 1].QuadPart);

	// TLS Free
	fprintf(freeFile, "Free,");
	for (int cnt = 0; cnt < s_TLSFreeProfile.size() - 1; ++cnt)
	{
		fprintf(freeFile, "%lld,", s_TLSFreeProfile[cnt].QuadPart);
	}
	fprintf(freeFile, "%lld\n", s_TLSFreeProfile[s_TLSFreeProfile.size() - 1].QuadPart);

	// Delete
	fprintf(freeFile, "Delete,");
	for (int cnt = 0; cnt < s_DeleteProfile.size() - 1; ++cnt)
	{
		fprintf(freeFile, "%lld,", s_DeleteProfile[cnt].QuadPart);
	}
	fprintf(freeFile, "%lld\n", s_DeleteProfile[s_DeleteProfile.size() - 1].QuadPart);

	// 저장
	fclose(allocFile);
	fclose(freeFile);
}


template <typename T, int CHUNK_SIZE>
unsigned WINAPI TLSObjectThread(void* argv)
{
	int threadIdx = (int)argv;
	TLSObjectPool<T, CHUNK_SIZE>* pool = (TLSObjectPool<T, CHUNK_SIZE>*)s_ThreadParams[threadIdx].poolPtr;
	s_AllocTotalProfile[threadIdx].QuadPart = 0;
	s_FreeTotalProfile[threadIdx].QuadPart = 0;

	T** datas = (T**)malloc(COUNT_OF_REPEAT * sizeof(T*));

	LARGE_INTEGER start;
	LARGE_INTEGER end;

	int loopCnt = COUNT_OF_LOOP;

	while (loopCnt--)
	{
		for (int cnt = 0; cnt < COUNT_OF_REPEAT; ++cnt)
		{
			datas[cnt] = pool->Alloc();
		}


		for (int cnt = 0; cnt < COUNT_OF_REPEAT; ++cnt)
		{
			pool->Free(datas[cnt]);
		}
	}

	SetEvent(s_ThreadParams[threadIdx].completeHandle);
	WaitForSingleObject(s_StartEvent, INFINITE);

	
	loopCnt = COUNT_OF_LOOP;

	while (loopCnt--)
	{
		
		QueryPerformanceCounter(&start);
		for (int cnt = 0; cnt < COUNT_OF_REPEAT; ++cnt)
		{
			datas[cnt] = pool->Alloc();
		}
		QueryPerformanceCounter(&end);

		s_AllocTotalProfile[threadIdx].QuadPart += end.QuadPart - start.QuadPart;

		QueryPerformanceCounter(&start);
		for (int cnt = 0; cnt < COUNT_OF_REPEAT; ++cnt)
		{
			pool->Free(datas[cnt]);
		}

		QueryPerformanceCounter(&end);

		s_FreeTotalProfile[threadIdx].QuadPart += end.QuadPart - start.QuadPart;
	}
	
	free(datas);

	pool->FreeChunk();

	SetEvent(s_ThreadParams[threadIdx].completeHandle);

	return 0;
}

template <typename T>
unsigned WINAPI NewDeleteThread(void* argv)
{
	int threadIdx = (int)argv;
	s_AllocTotalProfile[threadIdx].QuadPart = 0;
	s_FreeTotalProfile[threadIdx].QuadPart = 0;

	T** datas = (T**)malloc(COUNT_OF_REPEAT * sizeof(T*));

	LARGE_INTEGER start;
	LARGE_INTEGER end;



	int loopCnt = COUNT_OF_LOOP;
	while (loopCnt--)
	{
		for (int cnt = 0; cnt < COUNT_OF_REPEAT; ++cnt)
		{
			datas[cnt] = new T;
		}
		for (int cnt = 0; cnt < COUNT_OF_REPEAT; ++cnt)
		{
			delete datas[cnt];
		}
	}

	SetEvent(s_ThreadParams[threadIdx].completeHandle);
	WaitForSingleObject(s_StartEvent, INFINITE);

	
	loopCnt = COUNT_OF_LOOP;

	while (loopCnt--)
	{
		
		QueryPerformanceCounter(&start);
		for (int cnt = 0; cnt < COUNT_OF_REPEAT; ++cnt)
		{
			datas[cnt] = new T;
		}
		QueryPerformanceCounter(&end);
		s_AllocTotalProfile[threadIdx].QuadPart += end.QuadPart - start.QuadPart;
		
		QueryPerformanceCounter(&start);
		for (int cnt = 0; cnt < COUNT_OF_REPEAT; ++cnt)
		{
			delete datas[cnt];
		}
		QueryPerformanceCounter(&end);
		s_FreeTotalProfile[threadIdx].QuadPart += end.QuadPart - start.QuadPart;
	}
	

	delete datas;
	SetEvent(s_ThreadParams[threadIdx].completeHandle);

	return 0;
}


template <typename T, int CHUNK_SIZE>
void TestFunc(int threadCnt = 12)
{
	TLSObjectPool<T, CHUNK_SIZE> tlsObjectPool= TLSObjectPool<T, CHUNK_SIZE>((threadCnt * COUNT_OF_REPEAT / CHUNK_SIZE) * 2, false);
	
	// Event 초기화
	for (int cnt = 0; cnt < threadCnt; ++cnt)
	{
		s_ThreadParams[cnt].dataSize = sizeof(T);
		s_ThreadParams[cnt].poolPtr = (void*)&tlsObjectPool;
	}

	HANDLE threads[64];


	// TLSObject 성능확인
	unsigned int threadId;
	for (int cnt = 0; cnt < threadCnt; ++cnt)
	{
		threads[cnt] = (HANDLE)_beginthreadex(NULL, 0, TLSObjectThread<T, CHUNK_SIZE>, (void*)cnt, 0, &threadId);
	}

	WaitForMultipleObjects(threadCnt, s_ThreadEvents, true, INFINITE);
	// 시작!
	SetEvent(s_StartEvent);

	for (int cnt = 0; cnt < threadCnt; ++cnt)
	{
		ResetEvent(s_ThreadEvents[cnt]);
	}

	// 모든 스레드가 종료 될 떄 까지 대기
	WaitForMultipleObjects(threadCnt, s_ThreadEvents, true, INFINITE);
	ResetEvent(s_StartEvent);

	for (int cnt = 0; cnt < threadCnt; ++cnt)
	{
		CloseHandle(threads[cnt]);
	}
	
	LARGE_INTEGER allocTotalProfileTime;
	allocTotalProfileTime.QuadPart = 0;
	for (int cnt = 0; cnt < threadCnt; ++cnt)
	{
		allocTotalProfileTime.QuadPart += s_AllocTotalProfile[cnt].QuadPart;
	}
	allocTotalProfileTime.QuadPart;
	s_TLSAllocProfile.push_back(allocTotalProfileTime);

	LARGE_INTEGER freeTotalProfileTime;
	freeTotalProfileTime.QuadPart = 0;
	for (int cnt = 0; cnt < threadCnt; ++cnt)
	{
		freeTotalProfileTime.QuadPart += s_FreeTotalProfile[cnt].QuadPart;
	}
	freeTotalProfileTime.QuadPart;
	s_TLSFreeProfile.push_back(freeTotalProfileTime);

	for (int cnt = 0; cnt < threadCnt; ++cnt)
	{
		ResetEvent(s_ThreadEvents[cnt]);
	}
	
	
	// NewDelete 성능확인
	for (int cnt = 0; cnt < threadCnt; ++cnt)
	{
		threads[cnt] = (HANDLE)_beginthreadex(NULL, 0, NewDeleteThread<T>, (void*)cnt, 0, &threadId);
	}
	
	WaitForMultipleObjects(threadCnt, s_ThreadEvents, true, INFINITE);
	// 시작!
	SetEvent(s_StartEvent);

	for (int cnt = 0; cnt < threadCnt; ++cnt)
	{
		ResetEvent(s_ThreadEvents[cnt]);
	}

	// 모든 스레드가 종료 될 떄 까지 대기
	WaitForMultipleObjects(threadCnt, s_ThreadEvents, true, INFINITE);
	ResetEvent(s_StartEvent);

	for (int cnt = 0; cnt < threadCnt; ++cnt)
	{
		CloseHandle(threads[cnt]);
	}


	LARGE_INTEGER newTotalProfileTime;
	newTotalProfileTime.QuadPart = 0;
	for (int cnt = 0; cnt < threadCnt; ++cnt)
	{
		newTotalProfileTime.QuadPart += s_AllocTotalProfile[cnt].QuadPart;
	}
	newTotalProfileTime.QuadPart;
	s_NewProfile.push_back(newTotalProfileTime);

	LARGE_INTEGER deleteTotalProfileTime;
	deleteTotalProfileTime.QuadPart = 0;
	for (int cnt = 0; cnt < threadCnt; ++cnt)
	{
		deleteTotalProfileTime.QuadPart += s_FreeTotalProfile[cnt].QuadPart;
	}
	deleteTotalProfileTime.QuadPart;
	s_DeleteProfile.push_back(deleteTotalProfileTime);
	

	for (int cnt = 0; cnt < threadCnt; ++cnt)
	{
		ResetEvent(s_ThreadEvents[cnt]);
	}
	

	printf("Complete %d / %d\n", sizeof(T), CHUNK_SIZE);
}

