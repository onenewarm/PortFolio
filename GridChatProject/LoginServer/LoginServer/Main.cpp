#include "LoginServer.h"

int main() {
	LoginServer* server = (LoginServer*)_aligned_malloc(sizeof(LoginServer), 64);
	new(server) LoginServer;

	server->Start(L"LoginServer.cnf");

	Sleep(INFINITE);

	server->~LoginServer();
	_aligned_free(server);

	return 0;
}