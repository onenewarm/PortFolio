#include "Character.h"
#include "Global.h"

Character::Character() : m_AccountNo(0), m_LoginSessionId(0), m_GameSessionId(0), m_ChatSessionId(0), m_Status(en_CHARACTER_STATUS::LOGIN_CONNECT), m_NextActionTime(0), m_IsDisconnected(false), m_IsRequestedLogin(false), m_ConnectCount(0), m_IsCharacterCreated(false), m_IsGameLogin(false), m_IsGameReplyWait(false), m_X(-1), m_Y(-1), m_Direction(-1), m_SectorX(-1), m_SectorY(-1), m_IsMove(false), m_IsChatLogin(false), m_IsChatReplyWait(false)
{
	m_SessionKey[0] = L'\0';
	m_GameSessionKey[0] = '\0';
	m_ChatSessionKey[0] = '\0';
	m_ID[0] = L'\0';
	m_Nickname[0] = L'\0';
}

void Character::Clear()
{
	m_LoginSessionId = 0;
	m_GameSessionId = 0;
	m_ChatSessionId = 0;
	m_Status = en_CHARACTER_STATUS::LOGIN_CONNECT;
	m_NextActionTime = 0;
	m_IsRequestedLogin = false;
	m_GameSessionKey[0] = '\0';
	m_ChatSessionKey[0] = '\0';
	m_ConnectCount = 0;
	m_IsCharacterCreated = false;
	m_ID[0] = L'\0';
	m_Nickname[0] = L'\0';
	m_IsGameLogin = false;
	m_IsGameReplyWait = false;
	m_X = -1;
	m_Y = -1;
	m_Direction = -1;
	m_SectorX = -1;
	m_SectorY = -1;
	m_IsMove = false;
	m_IsChatLogin = false;
	m_IsChatReplyWait = false;
	InterlockedExchange8((char*) & m_IsDisconnected, 0);
}

bool UpdateSect(Character* character)
{
	short prevX = character->m_SectorX;
	short prevY = character->m_SectorY;

	short newSectX = (character->m_X / SECT_SIZE);
	short newSectY = (character->m_Y / SECT_SIZE);

	if (prevX != newSectX || prevY != newSectY)
	{
		character->m_SectorX = newSectX;
		character->m_SectorY = newSectY;
		return true;
	}

	return false;
}
