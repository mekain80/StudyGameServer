#include "stdafx.h"

#include <unordered_map>
#include "Session.h"
#include "MemoryPool.h"

namespace
{
	constexpr unsigned int kSessionPoolInitCount = 15000;
	constexpr std::size_t kInvalidActiveSessionIndex = static_cast<std::size_t>(-1);
	MemoryPool<Session> gSessionPool(true, kSessionPoolInitCount);
}

std::unordered_map<SOCKET, Session*> gSessionMap;
std::unordered_map<DWORD, Session*> gSessionIdMap;
ActiveSessionList gActiveSessions;

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

void AddActiveSession(Session* session) noexcept
{
	if (session == nullptr)
	{
		return;
	}

	if (session->activeSessionIndex < gActiveSessions.size() &&
		gActiveSessions[session->activeSessionIndex] == session)
	{
		return;
	}

	if (gActiveSessions.empty())
	{
		gActiveSessions.reserve(kSessionPoolInitCount);
	}

	session->activeSessionIndex = gActiveSessions.size();
	gActiveSessions.push_back(session);
}

void RemoveActiveSession(Session* session) noexcept
{
	if (session == nullptr)
	{
		return;
	}

	std::size_t removeIndex = session->activeSessionIndex;
	if (removeIndex >= gActiveSessions.size() || gActiveSessions[removeIndex] != session)
	{
		removeIndex = kInvalidActiveSessionIndex;
		for (std::size_t index = 0; index < gActiveSessions.size(); ++index)
		{
			if (gActiveSessions[index] == session)
			{
				removeIndex = index;
				break;
			}
		}
	}

	if (removeIndex == kInvalidActiveSessionIndex)
	{
		session->activeSessionIndex = kInvalidActiveSessionIndex;
		return;
	}

	const std::size_t lastIndex = gActiveSessions.size() - 1;
	Session* movedSession = gActiveSessions[lastIndex];
	gActiveSessions[removeIndex] = movedSession;
	if (movedSession != nullptr)
	{
		movedSession->activeSessionIndex = removeIndex;
	}

	gActiveSessions.pop_back();
	session->activeSessionIndex = kInvalidActiveSessionIndex;
}
