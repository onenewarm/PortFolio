#pragma once

#include <stdio.h>
#include <vector>

namespace onenewarm {
    class NetworkMonitor {
    public:
        struct NetworkStats {
            ULONGLONG bytesRecv;    // 초당 수신 바이트
            ULONGLONG bytesSent;    // 초당 송신 바이트
        };

        NetworkMonitor();
        ~NetworkMonitor();

        NetworkStats GetNetworkStats();    // 초당 송수신량 반환

    private:
        ULONGLONG m_PrevRecv;
        ULONGLONG m_PrevSent;
    };
}