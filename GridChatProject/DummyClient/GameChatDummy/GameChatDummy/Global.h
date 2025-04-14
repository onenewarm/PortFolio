#pragma once
#include <unordered_map>
#include "Character.h"

#define CHARACTER_IDX(accountNo) (((accountNo) - (g_StartCharacterNo)) - 1)
#define SECT_SIZE 160

//-----------------------------------------------------------------
// ȭ�� �̵� ����.
//-----------------------------------------------------------------
#define dfRANGE_MOVE_TOP	0
#define dfRANGE_MOVE_LEFT	0
#define dfRANGE_MOVE_RIGHT	6400
#define dfRANGE_MOVE_BOTTOM	6400


//---------------------------------------------------------------
// ���� ������.
//---------------------------------------------------------------
#define dfATTACK1_DAMAGE		1


//-----------------------------------------------------------------
// ĳ���� �̵� �ӵ�   // 25fps ���� �̵��ӵ�
//-----------------------------------------------------------------
#define dfSPEED_PLAYER_X	3	// 3   50fps
#define dfSPEED_PLAYER_Y	2	// 2   50fps

	//(���� ������ �� 8���� ���)
#define dfPACKET_MOVE_DIR_LL					0 // ������ ���� �ִ� ����
#define dfPACKET_MOVE_DIR_LU					1
#define dfPACKET_MOVE_DIR_UU					2 // ���� ���� �ִ� ����
#define dfPACKET_MOVE_DIR_RU					3
#define dfPACKET_MOVE_DIR_RR					4 // �������� ���� �ִ� ����
#define dfPACKET_MOVE_DIR_RD					5
#define dfPACKET_MOVE_DIR_DD					6 // �Ʒ��� ���� �ִ� ����
#define dfPACKET_MOVE_DIR_LD					7

#define MAX_CHAT_SIZE 100

extern int g_RandConnect;
extern int g_RandDisconnect;
extern int g_RandContents;
extern int g_DelayLogin;

extern int g_CharacterSize;
extern int g_StartCharacterNo;
extern Character* g_Characters;

//���� �÷���
extern bool g_IsPause;
extern bool g_IsStopConnect;
extern bool g_IsQuit;

////////////////////////////////
// **����͸� �׸�**
//////////////////////////////
extern LONG g_Monitor_DisconnectCount;
extern LONG g_Monitor_Action_Delay_Min;
extern LONG g_Monitor_Action_Delay_Max;
extern __int64 g_Monitor_Action_Delay_Total;
extern __int64 g_Monitor_Action_Count;


extern wchar_t** g_ChatList;
extern int g_ChatListSize;