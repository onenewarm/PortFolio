#include "GameServer.h"

using namespace onenewarm;

int main()
{
	GameServer* gameServer = (GameServer*)_aligned_malloc(sizeof(GameServer), 64);

	new(gameServer) GameServer();

	gameServer->Start(L"FighterServer.cnf");

	Sleep(INFINITE);

	return 0;

}