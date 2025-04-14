#include <Winsock2.h>
#include <WS2tcpip.h>
#include <netioapi.h>
#include <Utils\MonitorHelper\NetworkMonitor.h>
#include <Utils\CCrashDump.h>
#include <string>
#include <synchapi.h>

#pragma comment(lib, "iphlpapi.lib")

using namespace onenewarm;

std::vector<MIB_IF_ROW2> s_NetInterfaces;
bool s_IsSetNetInterface = false;

onenewarm::NetworkMonitor::NetworkMonitor() : m_PrevRecv(0ULL), m_PrevSent(0ULL) {
    if (InterlockedExchange8((char*)&s_IsSetNetInterface, 1) == 0)
    {
        PMIB_IF_TABLE2 pIfTable2 = NULL;
        DWORD dwRet = GetIfTable2(&pIfTable2);
        if (dwRet == NO_ERROR && pIfTable2 != NULL) {
            MIB_IF_ROW2* buffer = &pIfTable2->Table[0];

            for (ULONG i = 0; i < pIfTable2->NumEntries; i++) {
                if (buffer[i].InOctets > 0 || buffer[i].OutOctets > 0)
                {
                    std::wstring desc(buffer[i].Description);
                    if ((buffer[i].Type == IF_TYPE_ETHERNET_CSMACD ||
                        buffer[i].Type == IF_TYPE_IEEE80211) &&
                        (buffer[i].OperStatus == IfOperStatusUp) &&
                        (desc.find(L"Filter") == std::wstring::npos) &&
                        (desc.find(L"Npcap") == std::wstring::npos) &&
                        (desc.find(L"QoS") == std::wstring::npos))
                    {
                        s_NetInterfaces.push_back(buffer[i]);
                        ::wprintf(L"%s\n", buffer[i].Alias);
                    }

                }
            }

            FreeMibTable(pIfTable2);
        }
        else {
            CCrashDump::Crash();
        }
    }
    
    // 초기 측정
    GetNetworkStats();
}

onenewarm::NetworkMonitor::~NetworkMonitor() {
}

onenewarm::NetworkMonitor::NetworkStats NetworkMonitor::GetNetworkStats() {
    NetworkStats stats = { 0ULL, 0ULL };

    ULONGLONG totalBytesRecv = 0ULL;
    ULONGLONG totalBytesSent = 0ULL;

    DWORD getIfEntryRet;

    for (int cnt = 0; cnt < s_NetInterfaces.size(); ++cnt) {
        getIfEntryRet = GetIfEntry2(&s_NetInterfaces[cnt]);
        if (getIfEntryRet != NO_ERROR) {
            return stats;
        }

        totalBytesRecv += s_NetInterfaces[cnt].InOctets;
        totalBytesSent += s_NetInterfaces[cnt].OutOctets;
    }

    stats.bytesRecv = totalBytesRecv - m_PrevRecv;
    stats.bytesSent = totalBytesSent - m_PrevSent;

    m_PrevRecv = totalBytesRecv;
    m_PrevSent = totalBytesSent;

    return stats;
}