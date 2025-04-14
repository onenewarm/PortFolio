#include "Logger.h"
#include <iostream>
#include "CCrashDump.h"

using namespace std;

#define FILENAMESIZE 256

using namespace onenewarm;

int onenewarm::g_LogLevel;
WCHAR onenewarm::g_LogBuffer[1024];
CRITICAL_SECTION onenewarm::g_LogLock;

class LoggerManager
{
public:
	LoggerManager()
	{
		InitializeCriticalSection(&g_LogLock);
	}
	~LoggerManager()
	{
		DeleteCriticalSection(&g_LogLock);
	}
};



namespace onenewarm
{
	void CreateFileName(WCHAR* dest);
}

void onenewarm::CreateFileName(WCHAR* dest)
{
	struct tm newtime;
	__time64_t long_time;

	_time64(&long_time);

	_localtime64_s(&newtime, &long_time);

	WCHAR fileName[FILENAMESIZE];

	wsprintfW(fileName, L".\\%s\\LOG_%d%d%d_%d%d%d.txt", L"LOG", (newtime.tm_year + 1900),
		newtime.tm_mon + 1, newtime.tm_mday, newtime.tm_hour, newtime.tm_min, newtime.tm_sec);

	memcpy(dest, fileName, sizeof(WCHAR) * FILENAMESIZE);

	return;
}

void onenewarm::Log(const char* filePath, unsigned int fileLine, WCHAR * logStr, int LogLevel)
{
	static LoggerManager s_LoggerManager;

	EnterCriticalSection(&g_LogLock);
	WCHAR WfilePath[1024];

	size_t filePathSize;

	errno_t toWideRet = mbstowcs_s(&filePathSize, WfilePath, _countof(WfilePath), filePath, strlen(filePath));

	if (toWideRet != 0) CCrashDump::Crash();

	FILE* file;
	int openRet;

	WCHAR fileName[FILENAMESIZE];

	onenewarm::CreateFileName(fileName);

	openRet = _wfopen_s(&file, fileName, L"wt");

	if (file == NULL) {
		CCrashDump::Crash();
		return;
	}

	int filePathLen = (int)wcslen(WfilePath);

	WCHAR LogBuffer[1024 * 2];
	LogBuffer[0] = L'\0';

	WCHAR* pLogBuffer = LogBuffer;

	memcpy(pLogBuffer, L"FILE NAME : ",sizeof(WCHAR) * filePathLen);
	pLogBuffer += 12;
	memcpy(pLogBuffer, WfilePath, sizeof(WCHAR) * wcslen(WfilePath));
	pLogBuffer += wcslen(WfilePath);
	*pLogBuffer = L'\n';
	++pLogBuffer;
	memcpy(pLogBuffer, L"---------------------------------", sizeof(WCHAR) * 33);
	pLogBuffer += 33;
	*pLogBuffer = L'\n';
	++pLogBuffer;

	memcpy(pLogBuffer, L"LINE : ", sizeof(WCHAR) * 7);
	pLogBuffer += 7;

	int dataSize = wsprintfW(pLogBuffer, L"%d", fileLine);
	pLogBuffer += dataSize;

	*pLogBuffer = L'\n';
	++pLogBuffer;
	memcpy(pLogBuffer, L"---------------------------------", sizeof(WCHAR) * 33);
	pLogBuffer += 33;
	*pLogBuffer = L'\n';
	++pLogBuffer;

	memcpy(pLogBuffer, L"LOG : ", sizeof(WCHAR) * 6);
	pLogBuffer += 6;
	*(pLogBuffer++) = L'\n';
	memcpy(pLogBuffer, logStr, sizeof(WCHAR) * wcslen(logStr));
	pLogBuffer += wcslen(logStr);
	*pLogBuffer = L'\0';

	fwrite(LogBuffer, 2, wcslen(LogBuffer), file);

	fclose(file);
	LeaveCriticalSection(&g_LogLock);
}
