#pragma once
#include <Utils\MonitorHelper\CCpuUsage.h>
#include <Utils\MonitorHelper\NetworkMonitor.h>
#include <Pdh.h>
#include <Windows.h>


#pragma comment(lib,"Pdh.lib")

namespace onenewarm
{
	class PerformanceMonitor
	{
		static constexpr const wchar_t* PROCESS_PRIVATE_BYTES = L"\\Process(%s)\\Private Bytes";
		static constexpr const wchar_t* PROCESS_POOL_NONPAGED_BYTES = L"\\Process(%s)\\Pool Nonpaged Bytes";
		static constexpr const wchar_t* AVALIABLE_MBYTES = L"\\Memory\\Available MBytes";
		static constexpr const wchar_t* POOL_NONPAGED_BYTES = L"\\Memory\\Pool Nonpaged Bytes";
		static constexpr const wchar_t* TCP_RETRANSMITTED_PER_SECOND = L"\\TCPv4\\Segments Retransmitted/sec";

	public:
		//-------------------------------------------------------------------------------------
		// 
		//    MONITOR_INFO는 외부에서 모니터링 클래스의 데이터를 받아갈 수 있는 구조체 입니다.
		//
		//------------------------------------------------------------------------------------

		struct MONITOR_INFO
		{
			float m_ProcessorUserUsage;
			float m_ProcessorKernelUsage;
			float m_ProcessorTotalUsage;
			float m_ProcessUserUsage;
			float m_ProcessKernelUsage;
			float m_ProcessTotalUsage;

			double m_ProcessPrivateBytes;
			double m_AvailableMBytes;
			double m_ProcessNonPagedPoolBytes;
			double m_NonPagedPoolBytes;

			double m_TCPReTPS;
			ULONGLONG m_NetSentBytes;
			ULONGLONG m_NetRecvBytes;
		};


		PerformanceMonitor();
		void CollectMonitorData(MONITOR_INFO* destInfo);
	private:
		void AddCounter(OUT PDH_HCOUNTER* counter, const WCHAR* path, bool isProcessPath = false);

	private:
		CCpuUsage m_CpuUsage;
		NetworkMonitor m_NetMonitor;

		PDH_HQUERY m_Query;
		PDH_HCOUNTER m_PrivateMemCounter;
		PDH_HCOUNTER m_ProcNonPagedPoolCounter;
		PDH_HCOUNTER m_AvaliableMemCounter;
		PDH_HCOUNTER m_NonPagedPoolCounter;
		PDH_HCOUNTER m_TCPRetransmittedCounter;

	};

};