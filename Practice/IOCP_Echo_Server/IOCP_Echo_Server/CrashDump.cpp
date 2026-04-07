#include "CrashDump.h"
#include "Logger.h"

#pragma comment(lib, "Dbghelp.lib")

#include <DbgHelp.h>
#include <crtdbg.h>
#include <cstdlib>
#include <strsafe.h>

LONG CrashDump::m_lDumpCount = 0;
SRWLOCK CrashDump::m_srwLock = SRWLOCK_INIT;

CrashDump::CrashDump() noexcept
{
	_set_invalid_parameter_handler(MyInvalidParameterHandler);
	_set_purecall_handler(MyPurecallHandler);
	_CrtSetReportHook(CustomReportHook);
	_set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);

	DisableCrtReport();
	SetErrorMode(GetErrorMode() | SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
	SetHandlerDump();
}

void CrashDump::Crash() noexcept
{
	volatile int* crashPtr = nullptr;
	*crashPtr = 0;
}

LONG WINAPI CrashDump::MyExceptionFilter(PEXCEPTION_POINTERS pExceptionPointer) noexcept
{
	const LONG dumpCount = InterlockedIncrement(&m_lDumpCount);

	SYSTEMTIME nowTime{};
	GetLocalTime(&nowTime);

	CreateDirectoryW(L"Dump", nullptr);

	WCHAR fileName[MAX_PATH] = {};
	HRESULT fileNameResult = StringCchPrintfW(
		fileName,
		_countof(fileName),
		L".\\Dump\\Dump_%04u%02u%02u_%02u%02u%02u_%04ld.dmp",
		nowTime.wYear,
		nowTime.wMonth,
		nowTime.wDay,
		nowTime.wHour,
		nowTime.wMinute,
		nowTime.wSecond,
		dumpCount);

	if (fileNameResult != S_OK)
	{
		StringCchCopyW(fileName, _countof(fileName), L".\\Dump\\Dump_Fallback.dmp");
	}

	AcquireSRWLockExclusive(&m_srwLock);

	if (g_Logger != nullptr)
	{
		g_Logger->WriteLogConsole(LOG_LEVEL::ERR, L"\n\n\n Crash Error!!!");
		g_Logger->WriteLogConsole(LOG_LEVEL::ERR, L"Saving dump file...");
	}

	HANDLE dumpFile = CreateFileW(
		fileName,
		GENERIC_WRITE,
		FILE_SHARE_READ,
		nullptr,
		CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL,
		nullptr);

	if (dumpFile != INVALID_HANDLE_VALUE)
	{
		MINIDUMP_EXCEPTION_INFORMATION dumpExceptionInfo{};
		dumpExceptionInfo.ThreadId = GetCurrentThreadId();
		dumpExceptionInfo.ExceptionPointers = pExceptionPointer;
		dumpExceptionInfo.ClientPointers = FALSE;

		const MINIDUMP_TYPE dumpType = static_cast<MINIDUMP_TYPE>(
			MiniDumpWithFullMemory |
			MiniDumpWithHandleData |
			MiniDumpWithThreadInfo |
			MiniDumpWithUnloadedModules |
			MiniDumpIgnoreInaccessibleMemory);

		MiniDumpWriteDump(
			GetCurrentProcess(),
			GetCurrentProcessId(),
			dumpFile,
			dumpType,
			pExceptionPointer != nullptr ? &dumpExceptionInfo : nullptr,
			nullptr,
			nullptr);

		CloseHandle(dumpFile);

		if (g_Logger != nullptr)
		{
			g_Logger->WriteLogConsole(LOG_LEVEL::ERR, L"CrashDump saved completed");
		}
	}
	else if (g_Logger != nullptr)
	{
		g_Logger->WriteLogConsole(LOG_LEVEL::ERR, L"CrashDump failed");
	}

	ReleaseSRWLockExclusive(&m_srwLock);
	return EXCEPTION_EXECUTE_HANDLER;
}

void CrashDump::SetHandlerDump() noexcept
{
	SetUnhandledExceptionFilter(MyExceptionFilter);
}

void CrashDump::DisableCrtReport() noexcept
{
	_CrtSetReportMode(_CRT_WARN, 0);
	_CrtSetReportMode(_CRT_ASSERT, 0);
	_CrtSetReportMode(_CRT_ERROR, 0);
}

void __cdecl CrashDump::MyInvalidParameterHandler(
	const wchar_t* expression,
	const wchar_t* function,
	const wchar_t* file,
	unsigned int line,
	uintptr_t pReserved) noexcept
{
	UNREFERENCED_PARAMETER(expression);
	UNREFERENCED_PARAMETER(function);
	UNREFERENCED_PARAMETER(file);
	UNREFERENCED_PARAMETER(line);
	UNREFERENCED_PARAMETER(pReserved);

	Crash();
}

int __cdecl CrashDump::CustomReportHook(int reportType, char* message, int* returnValue) noexcept
{
	UNREFERENCED_PARAMETER(reportType);
	UNREFERENCED_PARAMETER(message);
	UNREFERENCED_PARAMETER(returnValue);

	Crash();
	return TRUE;
}

void __cdecl CrashDump::MyPurecallHandler()
{
	Crash();
}
