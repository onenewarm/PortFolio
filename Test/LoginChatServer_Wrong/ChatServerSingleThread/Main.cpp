#include "ChatServer.h"

using namespace onenewarm;

int main()
{
	ChatServer* server = (ChatServer*)_aligned_malloc(sizeof(ChatServer), 64);
	new(server) ChatServer;
	
	server->Start(L"ChatServer.cnf");

	while (true)
	{
		Sleep(INFINITE);
	}

	server->~ChatServer();
	_aligned_free(server);

	return 0;
}