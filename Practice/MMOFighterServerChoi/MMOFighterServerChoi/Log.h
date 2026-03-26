#pragma once

#include <windows.h>
#include <stdio.h>

#define LOG_LEVEL_DEBUG 0
#define LOG_LEVEL_SYSTEM 1
#define LOG_LEVEL_ERROR 2

void LogFileInit();
void LogWrite(int logLevel, const WCHAR* fmt, ...);
void LogWriteType(const WCHAR* type, int logLevel, const WCHAR* fmt, ...);
void LogWriteConsole(int logLevel, const WCHAR* fmt, ...);

extern int gLogLevel;

#define _LOG(LogLevel, fmt, ...)                    \
do{                                                 \
    LogWrite(LogLevel, fmt, ##__VA_ARGS__);         \
}while(0)

#define _LOG_TYPE(Type, LogLevel, fmt, ...)         \
do{                                                 \
    LogWriteType(Type, LogLevel, fmt, ##__VA_ARGS__); \
}while(0)

#define _LOG_CONSOLE(LogLevel, fmt, ...)            \
do{                                                 \
    LogWriteConsole(LogLevel, fmt, ##__VA_ARGS__);  \
}while(0)
