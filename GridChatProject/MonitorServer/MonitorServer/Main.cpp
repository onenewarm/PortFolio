#include "MonitorLanServer.h"
#include "MonitorNetServer.h"


int main()
{
	MonitorNetServer* netServer = (MonitorNetServer*)_aligned_malloc(sizeof(MonitorNetServer), 64);
	new(netServer) MonitorNetServer();
	MonitorLanServer* lanServer = (MonitorLanServer*)_aligned_malloc(sizeof(MonitorLanServer), 64);
	new(lanServer) MonitorLanServer(netServer);
	netServer->Start(L"NetMonitorServer.cnf");
	lanServer->Start(L"LanMonitorServer.cnf");

	Sleep(INFINITE);
}