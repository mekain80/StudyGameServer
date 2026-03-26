#pragma once

#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#pragma comment(lib, "ws2_32.lib")

#include <WinSock2.h>
#include <WS2tcpip.h>
#include <Windows.h>

#include <process.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <unordered_map>
#include <vector>

#include "ServerConfig.h"
#include "..\..\RingBuffer\RingBuffer\RingBuffer.h"
#include "..\..\MemoryPool\MemoryPool\MemoryPool.h"
