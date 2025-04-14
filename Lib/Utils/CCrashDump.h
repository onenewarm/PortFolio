#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <crtdbg.h>
#include <Windows.h>
#include <dbghelp.h>

#pragma comment(lib, "DbgHelp.Lib")

namespace onenewarm
{
	class CCrashDump
	{
	public:
		static void Crash(void);

		static LONG WINAPI MyExceptionFilter(__in PEXCEPTION_POINTERS pExceptionPointer);

		static void SetHandlerDump();
		static void myInvalidParameterHandler(const wchar_t* expression, const wchar_t* function, const wchar_t* file, unsigned int line, uintptr_t pReserved);
		static int _custom_Report_hook(int ireposttype, char* message, int* returnvalue);
		static void myPurecallHandler(void);
		CCrashDump();
		CCrashDump(CCrashDump& rhs) = delete;
		CCrashDump(CCrashDump&& rhs) = delete;
		CCrashDump& operator=(CCrashDump& rhs) = delete;
		CCrashDump& operator=(CCrashDump&& rhs) = delete;

	private:
		long m_CrashDumpCnt;
	};
}