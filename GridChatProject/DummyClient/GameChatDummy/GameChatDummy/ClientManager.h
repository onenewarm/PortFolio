#pragma once
#include "GameClient.h"
#include "LoginClient.h"
#include "ChatClient.h"
#include "Global.h"

using namespace onenewarm;


class ClientManager
{
private:
	static unsigned __stdcall Update(void* argv);

public:
	ClientManager();
	void Start();
	void Stop() { }

public:
	LoginClient* m_LoginClient;
	GameClient* m_GameClient;
	ChatClient* m_ChatClient;

private:
	vector<HANDLE> m_Threads;

};

extern ClientManager g_ClientManager;

