#include "CLogger.h"

#include <ctime>
#include <cwchar>
#include <share.h>
#include <strsafe.h>

CLogger* g_Logger = nullptr;

namespace
{
	WCHAR g_MessageBuf[10000] = {};
}

CLogger* CLogger::GetInstance() noexcept
{
	static CLogger logger;
	return &logger;
}

CLogger::CLogger() noexcept
	: m_lock()
	, m_LogCount(0)
	, m_LogLevel(LOG_LEVEL::DEBUG)
	, m_directoryName(L"LogFile")
{
	InitializeSRWLock(&m_lock);
}

void CLogger::WriteLog(const WCHAR* type, LOG_LEVEL logLevel, const WCHAR* fmt, ...)
{
	if (m_LogLevel > logLevel || type == nullptr || fmt == nullptr)
	{
		return;
	}

	tm tmTime;
	time_t nowTime = time(nullptr);
	localtime_s(&tmTime, &nowTime);

	CreateDirectoryW(m_directoryName, nullptr);

	WCHAR fileName[256] = {};
	StringCchPrintfW(
		fileName,
		_countof(fileName),
		L".\\%s\\%d%02d_%s.txt",
		m_directoryName,
		tmTime.tm_year + 1900,
		tmTime.tm_mon + 1,
		type);

	const INT64 logCount = InterlockedIncrement64(&m_LogCount);

	WCHAR logLevelStr[16] = {};
	switch (logLevel)
	{
	case LOG_LEVEL::DEBUG:
		StringCchCopyW(logLevelStr, _countof(logLevelStr), L"DEBUG");
		break;
	case LOG_LEVEL::SYSTEM:
		StringCchCopyW(logLevelStr, _countof(logLevelStr), L"SYSTEM");
		break;
	case LOG_LEVEL::ERR:
		StringCchCopyW(logLevelStr, _countof(logLevelStr), L"ERROR");
		break;
	default:
		StringCchCopyW(logLevelStr, _countof(logLevelStr), L"UNKNOWN");
		break;
	}

	WCHAR messageBuf[1024] = {};

	va_list va;
	va_start(va, fmt);
	const HRESULT messageResult = StringCchVPrintfW(messageBuf, _countof(messageBuf), fmt, va);
	va_end(va);

	if (messageResult != S_OK)
	{
		StringCchCopyW(messageBuf, _countof(messageBuf), L"log buffer too small");
	}

	WCHAR totalBuf[1400] = {};
	const HRESULT totalResult = StringCchPrintfW(
		totalBuf,
		_countof(totalBuf),
		L"[%s] [%d-%02d-%02d %02d:%02d:%02d / %s / %09lld] %s\n",
		type,
		tmTime.tm_year + 1900,
		tmTime.tm_mon + 1,
		tmTime.tm_mday,
		tmTime.tm_hour,
		tmTime.tm_min,
		tmTime.tm_sec,
		logLevelStr,
		logCount,
		messageBuf);

	if (totalResult != S_OK)
	{
		wprintf(L"[LOGGER] total format failed\n");
		return;
	}

	Lock();

	FILE* pFile = _wfsopen(fileName, L"a, ccs=UTF-16LE", _SH_DENYWR);
	if (pFile == nullptr)
	{
		wprintf(L"[LOGGER] file open fail, errorCode=%lu\n", GetLastError());
		UnLock();
		return;
	}

	fwrite(totalBuf, sizeof(WCHAR), wcslen(totalBuf), pFile);
	fclose(pFile);

	UnLock();
}

void CLogger::WriteLogHex(const WCHAR* type, LOG_LEVEL logLevel, const WCHAR* log, BYTE* pByte, int byteLen)
{
	if (type == nullptr || log == nullptr || pByte == nullptr || byteLen <= 0)
	{
		return;
	}

	int offset = 0;
	offset += swprintf_s(g_MessageBuf + offset, _countof(g_MessageBuf) - offset, L"%s: ", log);

	for (int i = 0; i < byteLen && offset < static_cast<int>(_countof(g_MessageBuf)); ++i)
	{
		offset += swprintf_s(g_MessageBuf + offset, _countof(g_MessageBuf) - offset, L"%02X ", pByte[i]);
	}

	WriteLog(type, logLevel, L"%s", g_MessageBuf);
}

void CLogger::WriteLogConsole(LOG_LEVEL logLevel, const WCHAR* fmt, ...)
{
	if (m_LogLevel > logLevel || fmt == nullptr)
	{
		return;
	}

	WCHAR logBuffer[1024] = {};

	va_list va;
	va_start(va, fmt);
	const HRESULT result = StringCchVPrintfW(logBuffer, _countof(logBuffer), fmt, va);
	va_end(va);

	if (result != S_OK)
	{
		wprintf(L"[LOGGER] console format failed\n");
		return;
	}

	wprintf(L"%s\n", logBuffer);
}
