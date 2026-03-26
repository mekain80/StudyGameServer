#include "stdafx.h"

#include <share.h>
#include <stdarg.h>
#include <time.h>

#include "Log.h"

int gLogLevel;

namespace
{
    CRITICAL_SECTION gLogLock;
    INIT_ONCE gLogInitOnce = INIT_ONCE_STATIC_INIT;

    BOOL CALLBACK InitializeLogInfrastructure(PINIT_ONCE, PVOID, PVOID*) noexcept
    {
        InitializeCriticalSection(&gLogLock);
        return TRUE;
    }

    void EnsureLogInfrastructure() noexcept
    {
        InitOnceExecuteOnce(&gLogInitOnce, InitializeLogInfrastructure, nullptr, nullptr);
        CreateDirectoryW(L"LogFile", nullptr);
    }

    const WCHAR* GetLogLevelText(int logLevel) noexcept
    {
        switch (logLevel)
        {
        case LOG_LEVEL_DEBUG:
            return L"DEBUG";
        case LOG_LEVEL_SYSTEM:
            return L"SYSTEM";
        case LOG_LEVEL_ERROR:
            return L"ERROR";
        default:
            return L"UNKNOWN";
        }
    }

    void GetLocalTimeInfo(tm& timeInfo) noexcept
    {
        time_t timer = time(nullptr);
        localtime_s(&timeInfo, &timer);
    }

    void SanitizeFileType(const WCHAR* source, WCHAR* destination, size_t destinationCount) noexcept
    {
        if (destination == nullptr || destinationCount == 0)
        {
            return;
        }

        destination[0] = L'\0';
        if (source == nullptr || source[0] == L'\0')
        {
            return;
        }

        size_t writeIndex = 0;
        for (size_t readIndex = 0; source[readIndex] != L'\0' && writeIndex + 1 < destinationCount; ++readIndex)
        {
            WCHAR ch = source[readIndex];
            switch (ch)
            {
            case L'\\':
            case L'/':
            case L':':
            case L'*':
            case L'?':
            case L'"':
            case L'<':
            case L'>':
            case L'|':
                ch = L'_';
                break;
            default:
                break;
            }

            destination[writeIndex++] = ch;
        }

        destination[writeIndex] = L'\0';
    }

    void BuildDefaultLogPath(const tm& timeInfo, WCHAR* path, size_t pathCount) noexcept
    {
        swprintf_s(
            path,
            pathCount,
            L"LogFile\\Log_%04d_%02d_%02d.txt",
            1900 + timeInfo.tm_year,
            timeInfo.tm_mon + 1,
            timeInfo.tm_mday);
    }

    void BuildTypedLogPath(const tm& timeInfo, const WCHAR* type, WCHAR* path, size_t pathCount) noexcept
    {
        WCHAR safeType[128]{};
        SanitizeFileType(type, safeType, _countof(safeType));

        swprintf_s(
            path,
            pathCount,
            L"LogFile\\%04d%02d_%s.txt",
            1900 + timeInfo.tm_year,
            timeInfo.tm_mon + 1,
            safeType);
    }

    void WriteLogInternal(
        const WCHAR* type,
        int logLevel,
        bool writeFile,
        bool writeConsole,
        const WCHAR* fmt,
        va_list args) noexcept
    {
        if (fmt == nullptr || logLevel < gLogLevel)
        {
            return;
        }

        EnsureLogInfrastructure();

        tm timeInfo{};
        GetLocalTimeInfo(timeInfo);

        WCHAR messageBuffer[4096]{};
        _vsnwprintf_s(messageBuffer, _countof(messageBuffer), _TRUNCATE, fmt, args);

        WCHAR lineBuffer[4608]{};
        swprintf_s(
            lineBuffer,
            _countof(lineBuffer),
            L"[%04d-%02d-%02d %02d:%02d:%02d] [%s] %s\n",
            1900 + timeInfo.tm_year,
            timeInfo.tm_mon + 1,
            timeInfo.tm_mday,
            timeInfo.tm_hour,
            timeInfo.tm_min,
            timeInfo.tm_sec,
            GetLogLevelText(logLevel),
            messageBuffer);

        EnterCriticalSection(&gLogLock);

        if (writeConsole)
        {
            wprintf(L"%s", lineBuffer);
            fflush(stdout);
        }

        if (writeFile)
        {
            WCHAR filePath[MAX_PATH]{};
            if (type != nullptr && type[0] != L'\0')
            {
                BuildTypedLogPath(timeInfo, type, filePath, _countof(filePath));
            }
            else
            {
                BuildDefaultLogPath(timeInfo, filePath, _countof(filePath));
            }

            FILE* file = _wfsopen(filePath, L"a, ccs=UTF-8", _SH_DENYWR);
            if (file != nullptr)
            {
                fputws(lineBuffer, file);
                fclose(file);
            }
        }

        LeaveCriticalSection(&gLogLock);
    }
}

void LogFileInit()
{
    EnsureLogInfrastructure();
    gLogLevel = LOG_LEVEL_SYSTEM;
}

void LogWrite(int logLevel, const WCHAR* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    WriteLogInternal(nullptr, logLevel, true, true, fmt, args);
    va_end(args);
}

void LogWriteType(const WCHAR* type, int logLevel, const WCHAR* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    WriteLogInternal(type, logLevel, true, true, fmt, args);
    va_end(args);
}

void LogWriteConsole(int logLevel, const WCHAR* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    WriteLogInternal(nullptr, logLevel, false, true, fmt, args);
    va_end(args);
}
