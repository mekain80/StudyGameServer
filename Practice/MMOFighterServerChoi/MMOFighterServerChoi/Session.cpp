#include "stdafx.h"

#include <unordered_map>
#include "Session.h"
#include "MemoryPool.h"

namespace
{
	constexpr unsigned int kSessionPoolInitCount = 15000;
	MemoryPool<Session> gSessionPool(true, kSessionPoolInitCount);
}

std::unordered_map<SOCKET, Session*> gSessionMap;
std::unordered_map<DWORD, Session*> gSessionIdMap;

Session* AllocSession() noexcept
{
	return gSessionPool.Alloc();
}

void FreeSession(Session* session) noexcept
{
	if (session == nullptr)
	{
		return;
	}

	gSessionPool.Free(session);
}

Session* FindSession(DWORD sessionID) noexcept
{
	auto iter = gSessionIdMap.find(sessionID);
	if (iter == gSessionIdMap.end())
	{
		return nullptr;
	}

	return iter->second;
}
