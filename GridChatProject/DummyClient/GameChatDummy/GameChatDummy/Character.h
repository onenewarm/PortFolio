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

// ����� ���� Ŭ���̾�Ʈ�� Ŭ���̾�Ʈ�Ŵ������� �����ϴ� �����Դϴ�.
// ���� �۾� �ÿ��� ������ ����� ������ ����ؾ� �մϴ�.
struct Character
{
	Character();
	void Clear();

	// account ���� ����
	__int64 m_AccountNo;
	wchar_t m_SessionKey[64];

	// ��Ʈ��ũ ���̺귯�� ���� ����
	__int64 m_LoginSessionId;
	__int64 m_GameSessionId;
	__int64 m_ChatSessionId;

	// Update ����
	byte m_Status;
	DWORD m_NextActionTime;
	bool m_IsDisconnected;

	// Login ����
	bool m_IsRequestedLogin;
	char m_GameSessionKey[64];
	char m_ChatSessionKey[64];

	// GameChat ���� ����
	SHORT m_ConnectCount;
	bool m_IsCharacterCreated;
	wchar_t m_ID[20];
	wchar_t m_Nickname[20];

	// Game ����
	bool m_IsGameLogin;
	bool m_IsGameReplyWait;
	short m_X;
	short m_Y;
	char m_Direction;
	short m_SectorX;
	short m_SectorY;
	bool m_IsMove;

	// Chat ����
	bool m_IsChatLogin;
	bool m_IsChatReplyWait;
};

bool UpdateSect(Character* character);