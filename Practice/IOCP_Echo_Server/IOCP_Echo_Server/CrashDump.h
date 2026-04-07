#pragma once

#include "Common.h"

class CrashDump
{
public:
	CrashDump() noexcept;

	static void Crash() noexcept;

private:
	static LONG WINAPI MyExceptionFilter(PEXCEPTION_POINTERS pExceptionPointer) noexcept;
	static void SetHandlerDump() noexcept;
	static void DisableCrtReport() noexcept;

	static void __cdecl MyInvalidParameterHandler(
		const wchar_t* expression,
		const wchar_t* function,
		const wchar_t* file,
		unsigned int line,
		uintptr_t pReserved) noexcept;
	static int __cdecl CustomReportHook(int reportType, char* message, int* returnValue) noexcept;
	static void __cdecl MyPurecallHandler();

private:
	static LONG m_lDumpCount;
	static SRWLOCK m_srwLock;
};
