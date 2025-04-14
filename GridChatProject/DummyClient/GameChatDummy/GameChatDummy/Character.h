#pragma once
#include <Windows.h>
#include 	<wchar.h>

enum en_CHARACTER_STATUS : byte
{
	LOGIN_CONNECT = 0,
	LOGIN_LOGIN,
	GAME_CONNECT,
	CHAT_CONNECT,
	GAME_CHAT_LOGIN,
	PLAY,
};

// 여기는 여러 클라이언트와 클라이언트매니저에서 접근하는 영역입니다.
// 쓰기 작업 시에는 무조건 아토믹 연산자 사용해야 합니다.
struct Character
{
	Character();
	void Clear();

	// account 연동 변수
	__int64 m_AccountNo;
	wchar_t m_SessionKey[64];

	// 네트워크 라이브러리 연동 변수
	__int64 m_LoginSessionId;
	__int64 m_GameSessionId;
	__int64 m_ChatSessionId;

	// Update 변수
	byte m_Status;
	DWORD m_NextActionTime;
	bool m_IsDisconnected;

	// Login 변수
	bool m_IsRequestedLogin;
	char m_GameSessionKey[64];
	char m_ChatSessionKey[64];

	// GameChat 공통 변수
	SHORT m_ConnectCount;
	bool m_IsCharacterCreated;
	wchar_t m_ID[20];
	wchar_t m_Nickname[20];

	// Game 변수
	bool m_IsGameLogin;
	bool m_IsGameReplyWait;
	short m_X;
	short m_Y;
	char m_Direction;
	short m_SectorX;
	short m_SectorY;
	bool m_IsMove;

	// Chat 변수
	bool m_IsChatLogin;
	bool m_IsChatReplyWait;
};

bool UpdateSect(Character* character);