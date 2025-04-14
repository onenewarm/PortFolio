#include <Utils\PerformanceMonitor.h>
#include <stdio.h>
#include <psapi.h>
#include <strsafe.h>
#include <pdhmsg.h>

void onenewarm::PerformanceMonitor::CollectMonitorData(MONITOR_INFO* destInfo)
{ 
	PDH_FMT_COUNTERVALUE counterVal;
	ZeroMemory(&counterVal, sizeof(counterVal));

	PDH_STATUS status;

	do {
		m_CpuUsage.UpdateCpuTime();
		status = PdhCollectQueryData(m_Query);
		if (status != ERROR_SUCCESS) break;

		destInfo->m_ProcessorKernelUsage = m_CpuUsage.ProcessorKernel();
		destInfo->m_ProcessorUserUsage = m_CpuUsage.ProcessorUser();
		destInfo->m_ProcessorTotalUsage = m_CpuUsage.ProcessorTotal();

		destInfo->m_ProcessKernelUsage = m_CpuUsage.ProcessKernel();
		destInfo->m_ProcessUserUsage = m_CpuUsage.ProcessUser();
		destInfo->m_ProcessTotalUsage = m_CpuUsage.ProcessTotal();

		PdhGetFormattedCounterValue(m_PrivateMemCounter, PDH_FMT_DOUBLE, NULL, &counterVal);
		if (counterVal.CStatus == 0) destInfo->m_ProcessPrivateBytes = counterVal.doubleValue;
		else destInfo->m_ProcessPrivateBytes = -1;

		PdhGetFormattedCounterValue(m_AvaliableMemCounter, PDH_FMT_DOUBLE, NULL, &counterVal);
		if (counterVal.CStatus == 0) destInfo->m_AvailableMBytes = counterVal.doubleValue;
		else destInfo->m_AvailableMBytes = -1;

		PdhGetFormattedCounterValue(m_ProcNonPagedPoolCounter, PDH_FMT_DOUBLE, NULL, &counterVal);
		if (counterVal.CStatus == 0) destInfo->m_ProcessNonPagedPoolBytes = counterVal.doubleValue;
		else destInfo->m_ProcessNonPagedPoolBytes = -1;

		PdhGetFormattedCounterValue(m_NonPagedPoolCounter, PDH_FMT_DOUBLE, NULL, &counterVal);
		if (counterVal.CStatus == 0) destInfo->m_NonPagedPoolBytes = counterVal.doubleValue;
		else destInfo->m_NonPagedPoolBytes = -1;
		
		PdhGetFormattedCounterValue(m_TCPRetransmittedCounter, PDH_FMT_DOUBLE, NULL, &counterVal);
		if (counterVal.CStatus == 0)destInfo->m_TCPReTPS = counterVal.doubleValue;
		else destInfo->m_TCPReTPS = -1;

		NetworkMonitor::NetworkStats netStats;
		netStats = m_NetMonitor.GetNetworkStats();
		destInfo->m_NetRecvBytes = netStats.bytesRecv;
		destInfo->m_NetSentBytes = netStats.bytesSent;

		return;
	} while (0);

	//CCrashDump::Crash();
}

void onenewarm::PerformanceMonitor::AddCounter(OUT PDH_HCOUNTER* counter, const WCHAR* path, bool isProcessPath)
{
	if (isProcessPath == true) {
		WCHAR processName[MAX_PATH];
		WCHAR fullPath[4096];
		DWORD nameSize = ::GetModuleBaseName(GetCurrentProcess(), NULL, processName, MAX_PATH);
		processName[nameSize - 4] = L'\0';
		StringCchPrintf(fullPath, sizeof(fullPath) / 2, path, processName);
		if (PdhAddCounter(m_Query, fullPath, NULL, counter) != ERROR_SUCCESS)
			DebugBreak();
	}
	else {
		if (PdhAddCounter(m_Query, path, NULL, counter) != ERROR_SUCCESS)
			DebugBreak();
	}
}



onenewarm::PerformanceMonitor::PerformanceMonitor() : m_Query(INVALID_HANDLE_VALUE), m_PrivateMemCounter(INVALID_HANDLE_VALUE), m_ProcNonPagedPoolCounter(INVALID_HANDLE_VALUE), m_AvaliableMemCounter(INVALID_HANDLE_VALUE), m_NonPagedPoolCounter(INVALID_HANDLE_VALUE), m_TCPRetransmittedCounter(INVALID_HANDLE_VALUE)
{
	bool crashFlag = true;

	PDH_STATUS status;

	do {
		// PDH 쿼리 핸들 생성
		status = PdhOpenQuery(NULL, NULL, &m_Query);
		if (status != ERROR_SUCCESS) break;

		AddCounter(&m_PrivateMemCounter, PROCESS_PRIVATE_BYTES, true);
		AddCounter(&m_ProcNonPagedPoolCounter, PROCESS_POOL_NONPAGED_BYTES, true);
		AddCounter(&m_AvaliableMemCounter, AVALIABLE_MBYTES);
		AddCounter(&m_NonPagedPoolCounter, POOL_NONPAGED_BYTES);
		AddCounter(&m_TCPRetransmittedCounter, TCP_RETRANSMITTED_PER_SECOND);

		// 첫 갱신
		status = PdhCollectQueryData(m_Query);
		if (status != ERROR_SUCCESS) break;

		crashFlag = false;
	} while (0);

	// TODO : Crash
	// if(crashFlag == true) CCrashDump::Crash();

}
