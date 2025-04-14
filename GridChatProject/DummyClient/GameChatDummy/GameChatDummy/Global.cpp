#include "Global.h"

int g_RandConnect;
int g_RandDisconnect;
int g_RandContents;
int g_DelayLogin;

int g_CharacterSize;
int g_StartCharacterNo;
Character* g_Characters;


//제어 플래그
bool g_IsPause = false;
bool g_IsStopConnect = false;
bool g_IsQuit = false;


////////////////////////////////
// **모니터링 항목**
//////////////////////////////
LONG g_Monitor_DisconnectCount;
LONG g_Monitor_Action_Delay_Min = LONG_MAX;
LONG g_Monitor_Action_Delay_Max = 0;
__int64 g_Monitor_Action_Delay_Total = 0LL;
__int64 g_Monitor_Action_Count = 0LL;


// 채팅리스트
wchar_t** g_ChatList;
int g_ChatListSize;