#pragma once

#include <stdio.h>
#include <vector>

namespace onenewarm {
    class NetworkMonitor {
    public:
        struct NetworkStats {
            ULONGLONG bytesRecv;    // �ʴ� ���� ����Ʈ
            ULONGLONG bytesSent;    // �ʴ� �۽� ����Ʈ
        };

        NetworkMonitor();
        ~NetworkMonitor();

        NetworkStats GetNetworkStats();    // �ʴ� �ۼ��ŷ� ��ȯ

    private:
        ULONGLONG m_PrevRecv;
        ULONGLONG m_PrevSent;
    };
}