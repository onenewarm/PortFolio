#include <Windows.h>
#include <process.h>

#include "CLock.h"
#include "SpinLock.h"
#include "LockFreeQueue.hpp"
#include "Profiler.h"
#include <vector>
#include "CSRWLOCK.h"
#include <queue>

#pragma comment(lib, "Winmm.lib")

using namespace onenewarm;

#define TEST_COUNT 1000
#define LOOP_COUNT 1000
#define SLEEP_TIME 10000

HANDLE StartEvent = CreateEvent(NULL, true, false, NULL);
HANDLE DeQEvent = CreateEvent(NULL, true, false, NULL);
LONG deqCnt = 0;
__int64 resultsPush[10][100];
__int64 resultsPop[10][100];
__int64 totalPush = 0;
__int64 totalPop = 0;

template<typename T>
class CQueue
{
    struct Node
    {
        T data;
        Node* front;
        Node* rear;
    };

public:
    CQueue()
    {
        pool = new ObjectPool<Node>(320000);
        front = pool->Alloc();
        rear = pool->Alloc();

        front->rear = rear;
        front->front = NULL;
        rear->front = front;
        rear->rear = NULL;

        size = 0;
    }

    void Push(T data)
    {
        Node* node = pool->Alloc();
        rear->rear = node;
        node->data = data;
        node->front = rear;
        node->rear = NULL;
        rear = node;
        ++size;
    }

    bool Pop(T* out)
    {
        if (size == 0) return false;
        *out = front->data;
        Node* old = front;
        front = front->rear;
        size--;
        pool->Free(old);
        return true;
    }

private:
    Node* front;
    Node* rear;
    int size;
    ObjectPool<Node>* pool;
};


template <typename LOCK>
struct ThreadParams
{
    int threadSize;
    LOCK* lock;
    int resultRowIdx;
    int resultColIdx;
};


CQueue<__int64>* q = new CQueue<__int64>();

LockFreeQueue<__int64>* lfq;

template <typename LOCK>
unsigned WINAPI Test(void* argv)
{
    ThreadParams<LOCK>* params = (ThreadParams<LOCK>*)argv;


    int threadSize = params->threadSize;
    int row = params->resultRowIdx;
    int col = params->resultColIdx;
    LOCK* lock = (params->lock);
    


    resultsPush[row][col] = 0;
    resultsPop[row][col] = 0;

    WaitForSingleObject(StartEvent, INFINITE);
    
    int loopCnt = LOOP_COUNT;

    LARGE_INTEGER start, end;

    //QueryPerformanceCounter(&start);
    while(loopCnt--)
    {
        
        QueryPerformanceCounter(&start);
        for (int cnt = 0; cnt < TEST_COUNT; ++cnt)
        {
            
            lock->Lock();
            q->Push(cnt);
            lock->UnLock();
            
        }
        QueryPerformanceCounter(&end);
        resultsPush[row][col] += (end.QuadPart - start.QuadPart);
        

        //if (InterlockedIncrement(&deqCnt) == threadSize)
        //{
        //    deqCnt = 0;
        //    SetEvent(DeQEvent);
        //}
        //else
        //{
        //    WaitForSingleObject(DeQEvent, INFINITE);
        //}
        

        long long data;
        
        QueryPerformanceCounter(&start);
        for (int cnt = 0; cnt < TEST_COUNT; ++cnt)
        {
            
            
            lock->Lock();
            q->Pop(&data);
            lock->UnLock();
            
        }
        QueryPerformanceCounter(&end);
        resultsPop[row][col] += (end.QuadPart - start.QuadPart);
    }
    //QueryPerformanceCounter(&end);

    InterlockedAdd64(&totalPush, resultsPush[row][col]);
    InterlockedAdd64(&totalPop, resultsPop[row][col]);
    
    resultsPush[row][col] = 0;
    resultsPop[row][col] = 0;

    return 0;
}


unsigned WINAPI LockFreeQTest(void* argv)
{
    ThreadParams<int>* params = (ThreadParams<int>*)argv;

    int threadSize = params->threadSize;
    int row = params->resultRowIdx;
    int col = params->resultColIdx;

    resultsPush[row][col] = 0;
    resultsPop[row][col] = 0;

    WaitForSingleObject(StartEvent, INFINITE);

    int loopCnt = LOOP_COUNT;

    LARGE_INTEGER start, end;

    //QueryPerformanceCounter(&start);
    while(loopCnt--)
    {
        QueryPerformanceCounter(&start);
        for (int cnt = 0; cnt < TEST_COUNT; ++cnt)
        {
            
            lfq->Enqueue(cnt);
            
           
        }
        QueryPerformanceCounter(&end);
        resultsPush[row][col] += (end.QuadPart - start.QuadPart);

        //if (InterlockedIncrement(&deqCnt) == threadSize)
        //{
        //    deqCnt = 0;
        //    SetEvent(DeQEvent);
        //}
        //else
        //{
        //    WaitForSingleObject(DeQEvent, INFINITE);
        //}

        
        __int64 data;

        QueryPerformanceCounter(&start);
        for (int cnt = 0; cnt < TEST_COUNT; ++cnt)
        {
            
            lfq->Dequeue(&data);
            
        }
        QueryPerformanceCounter(&end);
        resultsPop[row][col] += (end.QuadPart - start.QuadPart);
    }
    //QueryPerformanceCounter(&end);

    InterlockedAdd64(&totalPush, resultsPush[row][col]);
    InterlockedAdd64(&totalPop, resultsPop[row][col]);

    resultsPush[row][col] = 0;
    resultsPop[row][col] = 0;

    return 0;
}


int main()
{
    timeBeginPeriod(1);

    lfq = new LockFreeQueue<__int64>(10000000);
    CSRWLOCK srwLock;
    SpinLock spinLock;


    ThreadParams<SpinLock> spinLockParam[100];
    ThreadParams<CSRWLOCK> srwLockParam[100];
    ThreadParams<int> LFQParam[100];

    unsigned int threadId;
    std::vector<HANDLE> handles;

    for (int threadCnt = 2; threadCnt <= 32; threadCnt++)
    {

        for (int cnt = 0; cnt < threadCnt; ++cnt)
        {
            spinLockParam[cnt].threadSize = threadCnt;
            spinLockParam[cnt].resultRowIdx = 0;
            spinLockParam[cnt].lock = &spinLock;
            srwLockParam[cnt].threadSize = threadCnt;
            srwLockParam[cnt].resultRowIdx = 1;
            srwLockParam[cnt].lock = &srwLock;
            LFQParam[cnt].threadSize = threadCnt;
            LFQParam[cnt].resultRowIdx = 2;
        }



        // SpinLock 테스트
        int createThreadCnt = threadCnt;
        
        while (createThreadCnt--)
        {
            spinLockParam[createThreadCnt].resultColIdx = createThreadCnt;
            handles.push_back((HANDLE)_beginthreadex(NULL, 0, Test<SpinLock>, &spinLockParam[createThreadCnt], 0, &threadId));
        }
        ::Sleep(1000);
        ::SetEvent(StartEvent);
        WaitForMultipleObjects(handles.size(), &handles[0], TRUE, INFINITE);
        for (int cnt = 0; cnt < handles.size(); ++cnt)
        {
            CloseHandle(handles[cnt]);
        }
        handles.clear();
        
        ::printf("SpinLock / thread : %d / Push : %lld / Pop : %lld\n", threadCnt, totalPush / threadCnt, totalPop / threadCnt);
        totalPush = 0;
        totalPop = 0;
        ::ResetEvent(StartEvent);
        ::ResetEvent(DeQEvent);


        // SRWLOCK 테스트
        createThreadCnt = threadCnt;
        while (createThreadCnt--)
        {
            srwLockParam[createThreadCnt].resultColIdx = createThreadCnt;
            handles.push_back((HANDLE)_beginthreadex(NULL, 0, Test<CSRWLOCK>, &srwLockParam[createThreadCnt], 0, &threadId));
        }
        ::Sleep(1000);
        ::SetEvent(StartEvent);
        WaitForMultipleObjects(handles.size(), &handles[0], TRUE, INFINITE);
        for (int cnt = 0; cnt < handles.size(); ++cnt)
        {
            CloseHandle(handles[cnt]);
        }
        handles.clear();
        ::printf("SRWLOCK / thread : %d / Push : %lld / Pop : %lld\n", threadCnt, totalPush / threadCnt, totalPop / threadCnt);
        totalPush = 0;
        totalPop = 0;
        ::ResetEvent(StartEvent);
        ::ResetEvent(DeQEvent);


        // 락프리큐 테스트 
        createThreadCnt = threadCnt;
        while (createThreadCnt--)
        {
            LFQParam[createThreadCnt].resultColIdx = createThreadCnt;
            handles.push_back((HANDLE)_beginthreadex(NULL, 0, LockFreeQTest, &LFQParam[createThreadCnt], 0, &threadId));
        }
        ::Sleep(1000);
        ::SetEvent(StartEvent);
        WaitForMultipleObjects(handles.size(), &handles[0], TRUE, INFINITE);
        for (int cnt = 0; cnt < handles.size(); ++cnt)
        {
            CloseHandle(handles[cnt]);
        }
        handles.clear();

        ::printf("LFQ / thread : %d / Push : %lld / Pop : %lld\n\n", threadCnt, totalPush / threadCnt, totalPop / threadCnt);
        totalPush = 0;
        totalPop = 0;
        ::ResetEvent(StartEvent);
        ::ResetEvent(DeQEvent);
    }

    ::Sleep(INFINITE);
}

