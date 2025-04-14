#pragma once


#ifndef __LOGGER__
#define __LOGGER__

#include <wchar.h>
#include <windows.h>

namespace onenewarm
{
	#define LOG_LEVEL_DEBUG 0
	#define LOG_LEVEL_WARNING 1
	#define LOG_LEVEL_ERROR 2
	#define LOG_LEVEL_SYSTEM 3

	void _cdecl Log(const char* filePath, unsigned int fileLine, WCHAR* logStr, int LogLevel);
	
	#define _LOG_CONSOLE(LogLevel, fmt, ...)						\
	 do {															\
		if(g_LogLevel <= LogLevel)									\
		{															\
			wsprintf(g_LogBuffer, fmt, ##__VA_ARGS__);				\
			wcout << g_LogBuffer << L'\n';							\
		}															\
	} while(0)														\
	
	
	
	#define _LOG(LogLevel, FilePath, FileLine, fmt, ...)			\
	 do {															\
		if(g_LogLevel <= LogLevel)									\
		{															\
			wsprintf(g_LogBuffer, fmt, ##__VA_ARGS__);				\
			Log(FilePath, FileLine, g_LogBuffer, LogLevel);			\
		}															\
	} while(0)														\
	
	
	extern int g_LogLevel;
	extern WCHAR g_LogBuffer[1024];
	extern CRITICAL_SECTION g_LogLock;
}
#endif