#pragma once
#pragma warning(disable : 26110) // MSVC 정적 분석 경고 C26110을 끄는

#include "Common.h"

enum class LOG_LEVEL
{
	DEBUG,
	SYSTEM,
	ERR
};

class CLogger
{
public:
	static CLogger* GetInstance() noexcept;

	void WriteLog(const WCHAR* type, LOG_LEVEL logLevel, const WCHAR* fmt, ...);
	void WriteLogHex(const WCHAR* type, LOG_LEVEL logLevel, const WCHAR* log, BYTE* pByte, int byteLen);
	void WriteLogConsole(LOG_LEVEL logLevel, const WCHAR* fmt, ...);

	inline void SetDirectory(const WCHAR* directoryName) noexcept
	{
		if (directoryName == nullptr || directoryName[0] == L'\0')
		{
			m_directoryName = L"LogFile";
			return;
		}

		m_directoryName = directoryName;
	}

	inline void SetLogLevel(LOG_LEVEL logLevel) noexcept
	{
		m_LogLevel = logLevel;
	}

	CLogger(const CLogger&) = delete;
	CLogger& operator=(const CLogger&) = delete;
	CLogger(CLogger&&) = delete;
	CLogger& operator=(CLogger&&) = delete;

private:
	CLogger() noexcept;
	~CLogger() noexcept = default;

	void Lock() noexcept
	{
		AcquireSRWLockExclusive(&m_lock);
	}

	void UnLock() noexcept
	{
		ReleaseSRWLockExclusive(&m_lock);
	}

private:
	SRWLOCK m_lock;
	INT64 m_LogCount;
	LOG_LEVEL m_LogLevel;
	const WCHAR* m_directoryName;
};

extern CLogger* g_Logger;
