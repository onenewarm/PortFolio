#include "ClientManager.h"
#include <process.h>


int dx[8] = { -1, -1, 0, 1, 1, 1, 0, -1 };
int dy[8] = { 0, -1, -1, -1, 0, 1, 1, 1 };

// 클라 매니저
ClientManager g_ClientManager;

void ActionLoginConnect(Character* character);
void ActionLoginLogin(Character* character);
void ActionGameConnect(Character* character);
void ActionChatConnect(Character* character);
void ActionGameChatLogin(Character* character);
void ActionPlay(Character* character);

unsigned __stdcall ClientManager::Update(void* argv)
{
	// 캐릭터들의 쓰기 담당 스레드
	// 50개의 캐릭터의 행동만 담당 

	__int64 randSeed = (__int64) & argv;
	srand((unsigned int)randSeed);


	int accountStartIdx = (int)argv;
	int accountEndIdx = accountStartIdx + 50;

	DWORD curTime = timeGetTime();
	DWORD nextFrameTime = curTime += 20;

	while (true)
	{
		Sleep(1);

		curTime = timeGetTime();
		if (curTime >= nextFrameTime)
		{
			nextFrameTime += 20;

			if (g_IsPause == true) continue;

			for (int cnt = accountStartIdx; cnt < accountEndIdx; ++cnt)
			{
				Character* character = &g_Characters[cnt];

				if (character->m_IsDisconnected == true) continue;

				bool isTryDisconnect = false;

				switch (character->m_Status)
				{
				case en_CHARACTER_STATUS::LOGIN_CONNECT:
					if (character->m_NextActionTime > curTime) break;
					if (g_IsStopConnect == false) ActionLoginConnect(character);
					break;
				case en_CHARACTER_STATUS::LOGIN_LOGIN:
					if (character->m_NextActionTime > curTime) break;
					ActionLoginLogin(character);
					break;
				case en_CHARACTER_STATUS::GAME_CONNECT:
					if (character->m_NextActionTime > curTime) break;
					ActionGameConnect(character);
					break;
				case en_CHARACTER_STATUS::CHAT_CONNECT:
					if (character->m_NextActionTime > curTime) break;
					ActionChatConnect(character);
					break;
				case en_CHARACTER_STATUS::GAME_CHAT_LOGIN:
					if (character->m_NextActionTime > curTime) break;
					//isTryDisconnect = true;
					ActionGameChatLogin(character);
					break;
				case en_CHARACTER_STATUS::PLAY:
					isTryDisconnect = true;
					ActionPlay(character);
					break;
				default :
					break;
				}

				if (isTryDisconnect == true && (rand() % 100 < g_RandDisconnect))
				{
					InterlockedIncrement(&g_Monitor_DisconnectCount);
					InterlockedExchange8((char*)&character->m_IsDisconnected, 1);
					g_ClientManager.m_GameClient->Disconnect(character->m_GameSessionId);
					g_ClientManager.m_ChatClient->Disconnect(character->m_ChatSessionId);
				}
				::Sleep(0);
			}
		}
	}

	return 0;
}


ClientManager::ClientManager()
{
	// Client들
	m_LoginClient = new LoginClient();
	m_GameClient = new GameClient();
	m_ChatClient = new ChatClient();
}

void ClientManager::Start()
{
	m_LoginClient->Start(L"LoginClient.cnf");
	m_GameClient->Start(L"GameClient.cnf");
	m_ChatClient->Start(L"ChatClient.cnf");

	int updateThreadCnt = ((g_CharacterSize - 1) / 50) + 1;

	unsigned int threadId;

	for (int cnt = 0; cnt < updateThreadCnt; ++cnt)
	{
		HANDLE hThread = (HANDLE)_beginthreadex(NULL, 0, Update, (void*)(cnt * 50), 0, &threadId);
		m_Threads.push_back(hThread);
	}
}


void ActionLoginConnect(Character* character)
{
	int randNum = rand() % 100;
	if (randNum >= g_RandConnect) return;

	InterlockedIncrement64(&g_ClientManager.m_LoginClient->m_Monitor_ConnectTotal);
	if (g_ClientManager.m_LoginClient->Connect(&character->m_LoginSessionId) == false)
	{
		InterlockedIncrement(&g_ClientManager.m_LoginClient->m_Monitor_ConnetFail);
		return;
	}
	g_ClientManager.m_LoginClient->m_AccountNos[(WORD)(character->m_LoginSessionId)] = character->m_AccountNo;

	character->m_NextActionTime = timeGetTime() + g_DelayLogin;

	character->m_Status = en_CHARACTER_STATUS::LOGIN_LOGIN;
}

void ActionLoginLogin(Character* character)
{
	if (character->m_IsRequestedLogin == true) return;

	int randNum = rand() % 100;
	if (randNum >= g_RandContents) return;

	g_ClientManager.m_LoginClient->Request_CS_Login(character->m_LoginSessionId, character->m_AccountNo, (char*)character->m_SessionKey);
	character->m_IsRequestedLogin = true;
}

void ActionGameConnect(Character* character)
{
	int randNum = rand() % 100;
	if (randNum >= g_RandConnect) return;

	InterlockedIncrement64(&g_ClientManager.m_GameClient->m_Monitor_ConnectTotal);
	if (g_ClientManager.m_GameClient->Connect(&character->m_GameSessionId) == false)
	{
		InterlockedIncrement(&g_ClientManager.m_GameClient->m_Monitor_ConnetFail);
		return;
	}
	InterlockedIncrement16(&character->m_ConnectCount);
	g_ClientManager.m_GameClient->m_AccountNos[character->m_GameSessionId & 0x7fff] = character->m_AccountNo;

	character->m_Status = en_CHARACTER_STATUS::CHAT_CONNECT;
}

void ActionChatConnect(Character* character)
{
	int randNum = rand() % 100;
	if (randNum >= g_RandConnect) return;

	InterlockedIncrement64(&g_ClientManager.m_ChatClient->m_Monitor_ConnectTotal);
	if (g_ClientManager.m_ChatClient->Connect(&character->m_ChatSessionId) == false)
	{
		InterlockedIncrement(&g_ClientManager.m_ChatClient->m_Monitor_ConnetFail);
		return;
	}
	InterlockedIncrement16(&character->m_ConnectCount);
	g_ClientManager.m_ChatClient->m_AccountNos[character->m_ChatSessionId & 0x7fff] = character->m_AccountNo;

	character->m_NextActionTime = timeGetTime() + g_DelayLogin;
	character->m_Status = en_CHARACTER_STATUS::GAME_CHAT_LOGIN;
	character->m_IsRequestedLogin = false;
}

void ActionGameChatLogin(Character* character)
{
	if (character->m_IsRequestedLogin == true) return;

	int randNum = rand() % 100;
	if (randNum >= g_RandContents) return;

	g_ClientManager.m_GameClient->Request_CS_Login(character->m_GameSessionId, character->m_AccountNo, character->m_GameSessionKey);
	g_ClientManager.m_ChatClient->Request_CS_Login(character->m_ChatSessionId, character->m_ID, character->m_Nickname, character->m_AccountNo, character->m_ChatSessionKey);

	character->m_IsRequestedLogin = true;
}

void ActionPlay(Character* character)
{
	DWORD curTime = timeGetTime();

	if (character->m_IsCharacterCreated == false) return;

	// 게임
	if (character->m_IsMove == true)
	{
		short nx = character->m_X + dx[character->m_Direction] * dfSPEED_PLAYER_X;
		short ny = character->m_Y + dy[character->m_Direction] * dfSPEED_PLAYER_Y;

		if (nx < dfRANGE_MOVE_LEFT) nx = dfRANGE_MOVE_LEFT;
		else if (nx >= dfRANGE_MOVE_RIGHT) nx = dfRANGE_MOVE_RIGHT - 1;

		if (ny < dfRANGE_MOVE_TOP) ny = dfRANGE_MOVE_TOP;
		else if (ny >= dfRANGE_MOVE_BOTTOM) ny = dfRANGE_MOVE_BOTTOM - 1;

		if (character->m_X == nx && character->m_Y == ny)
		{
			character->m_NextActionTime = 0;
		}
		else
		{
			character->m_X = nx;
			character->m_Y = ny;
		}



		if (UpdateSect(character) == true)
		{
			g_ClientManager.m_ChatClient->Request_CS_SectorMove(character->m_ChatSessionId, character->m_AccountNo, character->m_SectorX, character->m_SectorY);
		}

		if (character->m_NextActionTime <= curTime)
		{
			character->m_IsMove = false;
			character->m_NextActionTime = curTime + ((rand() % 9) + 2) * 1000;  // 2초 ~ 10초 대기
			g_ClientManager.m_GameClient->Request_CS_MoveStop(character->m_GameSessionId, character->m_X, character->m_Y);
		}
	}
	else
	{
		if (character->m_NextActionTime <= curTime)
		{
			character->m_IsMove = true;
			
			BYTE newDirection;
			do
			{
				newDirection = rand() % 8;
			} while (character->m_Direction == newDirection);
			character->m_Direction = newDirection;
			character->m_NextActionTime = curTime + ((rand() % 9) + 2) * 1000;  // 2초 ~ 10초 이동
			g_ClientManager.m_GameClient->Request_CS_MoveStart(character->m_GameSessionId, character->m_Direction);
		}
	}

	// 채팅
	if (character->m_IsChatReplyWait == false)
	{
		int randNum = rand() % 100;
		if (randNum >= g_RandConnect) return;

		int messageLine = rand() % g_ChatListSize;
		int messageLen = wcslen(g_ChatList[messageLine]);

		g_ClientManager.m_ChatClient->Request_CS_Message(character->m_ChatSessionId, character->m_AccountNo, messageLen * 2, g_ChatList[messageLine]);
	}
}

