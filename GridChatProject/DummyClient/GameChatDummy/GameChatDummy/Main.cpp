#include <iostream>
#include "ClientManager.h"
#include <Utils/ConfigLoader.hpp>
#include <conio.h>
#include <strsafe.h>
#include <Utils/PerformanceMonitor.h>
#include <process.h>

using namespace onenewarm;

#pragma comment(lib, "Winmm.lib")

PerformanceMonitor s_PerfMonitor;
PerformanceMonitor::MONITOR_INFO s_Hard_Monitor;

bool SelectCharacterText(int accountSize)
{
	printf("0 : SESSIONKEY0.txt\n");
	printf("1 : SESSIONKEY1.txt\n");
	printf("2 : SESSIONKEY2.txt\n");

	int number;
	cin >> number;
	
	if (number == 0)
	{
		g_StartCharacterNo = 0;
	}
	else if (number == 1)
	{
		g_StartCharacterNo = 9999;
	}
	else if (number == 2)
	{
		g_StartCharacterNo = 19999;
	}


	if (number < 0 && number > 2) return false;
	
	wchar_t fileName[64];

	system("cls");


	swprintf_s(fileName, _countof(fileName), L"SESSIONKEY%d.txt", number);

	FILE* fp;
	if (int errNum = _wfopen_s(&fp, fileName, L"rt, ccs=UTF-16LE") != 0)
	{
		_LOG_CONSOLE(LOG_LEVEL_ERROR, L"file open error : %d\n", errNum);
		return false;
	}

	int fileSize;
	fseek(fp, 0, SEEK_END);
	fileSize = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	fileSize /= sizeof(wchar_t);

	wchar_t* fileData = (wchar_t*)malloc(sizeof(wchar_t) * fileSize);
	fread_s(fileData, fileSize * sizeof(wchar_t), sizeof(wchar_t), fileSize, fp);

	wstring accountInfo[2];
	int filePos = 1;
	int turn = 0;
	int curCharacterSize = 0;
	while (filePos <= fileSize && curCharacterSize < accountSize)
	{
		while (filePos <= fileSize && (fileData[filePos] == L'\0' || fileData[filePos] == L'\n' || fileData[filePos] == L'\t' || fileData[filePos] == L'\r' || fileData[filePos] == L' '))
		{
			++filePos;
		}

		accountInfo[turn].clear();
		while (filePos <= fileSize && (fileData[filePos] != L'\0' && fileData[filePos] != L'\n' && fileData[filePos] != L'\t' && fileData[filePos] != L'\r' && fileData[filePos] != L' '))
		{
			accountInfo[turn] += (fileData[filePos]);
			++filePos;
		}

		if (turn == 1)
		{
			g_Characters[curCharacterSize].m_AccountNo = stoi(accountInfo[0]);
			wmemcpy_s(g_Characters[curCharacterSize].m_SessionKey, 64, &accountInfo[1][0], accountInfo[1].size());
			curCharacterSize++;
		}

		turn ^= 1;
	}

	free(fileData);
	fclose(fp);
	return true;
}

void LoadChatList()
{
	FILE* fp;
	if (int errNum = _wfopen_s(&fp, L"ChatList.txt", L"rt, ccs=UTF-16LE") != 0)
	{
		_LOG_CONSOLE(LOG_LEVEL_ERROR, L"file open error : %d\n", errNum);
		return;
	}

	int fileSize;
	fseek(fp, 0, SEEK_END);
	fileSize = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	fileSize /= sizeof(wchar_t);

	wchar_t* fileData = (wchar_t*)malloc(sizeof(wchar_t) * fileSize);
	fread_s(fileData, fileSize * sizeof(wchar_t), sizeof(wchar_t), fileSize, fp);
	g_ChatList = (wchar_t**)malloc(sizeof(wchar_t*) * fileSize);

	int lineSize = 0;

	wstring s;
	for (int cnt = 1; cnt < fileSize; ++cnt)
	{
		if (fileData[cnt] == L'\n')
		{
			g_ChatList[lineSize] = (wchar_t*)malloc(MAX_CHAT_SIZE * sizeof(wchar_t));
			wmemcpy_s(g_ChatList[lineSize], MAX_CHAT_SIZE, &s[0], s.size());
			g_ChatList[lineSize][s.size()] = L'\0';
			lineSize++;
			s.clear();
		}
		else s += fileData[cnt];
	}

	
	g_ChatListSize = lineSize;

	free(fileData);
	fclose(fp);
}


void GlobalSet(ConfigLoader& configLoader)
{
	int accountSize;
	configLoader.GetValue(&accountSize, L"DUMMY_CLIENT", L"ACCOUNT_SIZE");
	g_CharacterSize = accountSize;
	g_Characters = new Character[accountSize];

	if (SelectCharacterText(accountSize) == false) DebugBreak();

	configLoader.GetValue(&g_RandConnect, L"DUMMY_CLIENT", L"RAND_CONNECT");
	configLoader.GetValue(&g_RandDisconnect, L"DUMMY_CLIENT", L"RAND_DISCONNECT");
	configLoader.GetValue(&g_RandContents, L"DUMMY_CLIENT", L"RAND_CONTENTS");
	configLoader.GetValue(&g_DelayLogin, L"DUMMY_CLIENT", L"DELAY_LOGIN");

	LoadChatList();
}

unsigned WINAPI MonitorAndControl(void* argv)
{
	// 시작시간 구하기
	struct tm newtime;
	__time64_t long_time;
	_time64(&long_time);
	_localtime64_s(&newtime, &long_time);



	// 버퍼 할당
	WCHAR* mLog = (WCHAR*)malloc(sizeof(WCHAR) * 4096);
	if (mLog == NULL) {
		CCrashDump::Crash();
		return 0;
	}


	LoginClient* loginClient = g_ClientManager.m_LoginClient;
	GameClient* gameClient = g_ClientManager.m_GameClient;
	ChatClient* chatClient = g_ClientManager.m_ChatClient;

	while(g_IsQuit == false)
	{
		if (_kbhit())
		{
			char key = _getch();
			
			if (key > 'Z') key -= 32;

			switch (key)
			{
			case 'S' :
				g_IsPause ^= 1;
				break;
			case 'A' :
				g_IsStopConnect ^= 1;
				break;
			case 'Q' :
				g_IsQuit = true;
				break;
			default :
				break;
			}
		}

		s_PerfMonitor.CollectMonitorData(&s_Hard_Monitor);

		StringCchPrintf(mLog, 4096, L"StartTime : %d.%d.%d %d:%d:%d\n", newtime.tm_year + 1900, newtime.tm_mon + 1, newtime.tm_mday, newtime.tm_hour, newtime.tm_min, newtime.tm_sec);
		StringCchPrintf(mLog, 4096, L"%s-------------------------------------------\n", mLog);

		StringCchPrintf(mLog, 4096, L"%s Now MODE : ", mLog);
		if (g_IsPause == true)
		{
			StringCchPrintf(mLog, 4096, L"%sSTOP", mLog);
		}
		else
		{
			StringCchPrintf(mLog, 4096, L"%sPLAY", mLog);
		}

		if (g_IsStopConnect == true)
		{
			StringCchPrintf(mLog, 4096, L"%s / STOP CONNECT", mLog);
		}

		StringCchPrintf(mLog, 4096, L"%s\n\n", mLog);
		StringCchPrintf(mLog, 4096, L"%s S : STOP / PLAY  |  A : CONNECT STOP  | Q : QUIT\n", mLog);
		StringCchPrintf(mLog, 4096, L"%s-------------------------------------------\n", mLog);
		StringCchPrintf(mLog, 4096, L"%s     Client Total : %d\n", mLog, g_CharacterSize);
		StringCchPrintf(mLog, 4096, L"%s-------------------------------------------\n", mLog);
		StringCchPrintf(mLog, 4096, L"%s       Disconnect : %d\n", mLog, InterlockedExchange(&g_Monitor_DisconnectCount,0));
		StringCchPrintf(mLog, 4096, L"%s  IN Login Server : %d\n", mLog, loginClient->m_CurSessionCnt);
		StringCchPrintf(mLog, 4096, L"%s     ConnectTotal : %lld\n", mLog, loginClient->m_Monitor_ConnectTotal);
		StringCchPrintf(mLog, 4096, L"%s      ConnectFail : %d\n", mLog, loginClient->m_Monitor_ConnetFail);
		StringCchPrintf(mLog, 4096, L"%s Login DownClient : %d\n", mLog, loginClient->m_Monitor_DownClient);
		StringCchPrintf(mLog, 4096, L"%s-------------------------------------------\n", mLog);
		StringCchPrintf(mLog, 4096, L"%s   IN Game Server : %d\n", mLog, gameClient->m_CurSessionCnt);
		StringCchPrintf(mLog, 4096, L"%s     ConnectTotal : %lld\n", mLog, gameClient->m_Monitor_ConnectTotal);
		StringCchPrintf(mLog, 4096, L"%s  Connect / Login : %d / %d\n", mLog, gameClient->m_CurSessionCnt - gameClient->m_Monitor_LoginSessionCnt, gameClient->m_Monitor_LoginSessionCnt);
		StringCchPrintf(mLog, 4096, L"%s      ConnectFail : %d\n", mLog, gameClient->m_Monitor_ConnetFail);
		StringCchPrintf(mLog, 4096, L"%s  Game DownClient : %d\n", mLog, gameClient->m_Monitor_DownClient);
		StringCchPrintf(mLog, 4096, L"%s-------------------------------------------\n", mLog);
		StringCchPrintf(mLog, 4096, L"%s        ReplyWait : %d\n", mLog, gameClient->m_Monitor_ReplyWait);
		StringCchPrintf(mLog, 4096, L"%s-------------------------------------------\n", mLog);
		StringCchPrintf(mLog, 4096, L"%s\n", mLog); // TODO : 1초 이상 응답을 받지 못한 패킷이 있다면 여기에 띄운다. (type / sessionId)
		StringCchPrintf(mLog, 4096, L"%s-------------------------------------------\n", mLog);
		StringCchPrintf(mLog, 4096, L"%s   IN Chat Server : %d\n", mLog, chatClient->m_CurSessionCnt);
		StringCchPrintf(mLog, 4096, L"%s     ConnectTotal : %lld\n", mLog, chatClient->m_Monitor_ConnectTotal);
		StringCchPrintf(mLog, 4096, L"%s  Connect / Login : %d / %d\n", mLog, chatClient->m_CurSessionCnt - chatClient->m_Monitor_LoginSessionCnt, chatClient->m_Monitor_LoginSessionCnt);
		StringCchPrintf(mLog, 4096, L"%s      ConnectFail : %d\n", mLog, chatClient->m_Monitor_ConnetFail);
		StringCchPrintf(mLog, 4096, L"%s  Chat DownClient : %d\n", mLog, chatClient->m_Monitor_DownClient);
		StringCchPrintf(mLog, 4096, L"%s-------------------------------------------\n", mLog);
		StringCchPrintf(mLog, 4096, L"%s        ReplyWait : %d\n", mLog, chatClient->m_Monitor_ReplyWait);
		StringCchPrintf(mLog, 4096, L"%s-------------------------------------------\n", mLog);
		StringCchPrintf(mLog, 4096, L"%s\n", mLog); // TODO : 1초 이상 응답을 받지 못한 패킷이 있다면 여기에 띄운다. (type / sessionId)
		StringCchPrintf(mLog, 4096, L"%s-------------------------------------------\n", mLog);
		StringCchPrintf(mLog, 4096, L"%s       PacketPool : Alloc %d / Use %d\n", mLog, CPacket::GetChunkSize(), CPacket::GetUseSize());
		StringCchPrintf(mLog, 4096, L"%s-------------------------------------------\n", mLog);
		StringCchPrintf(mLog, 4096, L"%sAction Delay Min : %d ms \n", mLog, g_Monitor_Action_Delay_Min);
		StringCchPrintf(mLog, 4096, L"%sAction Delay Max : %d ms \n", mLog, g_Monitor_Action_Delay_Max);
		int actionDealyAvr;
		if (g_Monitor_Action_Delay_Total == 0 || g_Monitor_Action_Count == 0) actionDealyAvr = 0;
		else actionDealyAvr = g_Monitor_Action_Delay_Total / g_Monitor_Action_Count;
		StringCchPrintf(mLog, 4096, L"%sAction Delay Avr : %d ms \n", mLog, actionDealyAvr);
		StringCchPrintf(mLog, 4096, L"%sCPU usage [T:%0.1f%% U:%0.1f%% K:%0.1f%%] [Dummy: T:%0.1f%% U:%0.1f%% K:%0.1f%%]\n", mLog, s_Hard_Monitor.m_ProcessorTotalUsage, s_Hard_Monitor.m_ProcessorUserUsage, s_Hard_Monitor.m_ProcessorKernelUsage,
			s_Hard_Monitor.m_ProcessTotalUsage, s_Hard_Monitor.m_ProcessUserUsage, s_Hard_Monitor.m_ProcessKernelUsage);

		::wprintf(L"%s", mLog);

		Sleep(1000);
	}


	// TODO : 끝날 때, LoginClient, GameClient, ChatClient의 Stop 호출하기
	g_ClientManager.m_LoginClient->Stop();
	g_ClientManager.m_GameClient->Stop();
	g_ClientManager.m_ChatClient->Stop();

	return 0;
}


int main()
{
	timeBeginPeriod(1);

	::Sleep(500);
	system("cls");

	ConfigLoader configLoader;
	configLoader.LoadFile(L"GameChatDummy_Config.ini");
	GlobalSet(configLoader);
	
	unsigned int threadId;

	_beginthreadex(NULL, 0, MonitorAndControl, NULL, 0, &threadId);

	g_ClientManager.Start();

	Sleep(INFINITE);

	return 0;
}