#include "stdafx.h"

#include "Log.h"

void ErrorHandler(const wchar_t* msg) noexcept
{
    int err = WSAGetLastError();
    wprintf(L"ERROR: %s, WSAGetLastError : %d\n", msg, err);
    ::WSACleanup();
    abort();
}

void Logger(const wchar_t* msg) noexcept
{
    wprintf(L"%s\n", msg);
}
