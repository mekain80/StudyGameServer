#include "stdafx.h"

#include <cassert>
#include <unordered_map>
#include "Session.h"
#include "MemoryPool.h"

namespace
{
	constexpr unsigned int kSessionPoolInitCount = 15000;
	constexpr std::size_t kInvalidActiveSessionIndex = static_cast<std::size_t>(-1);
	constexpr std::size_t kInvalidWritableSessionIndex = static_cast<std::size_t>(-1);
	MemoryPool<Session> gSessionPool(false, kSessionPoolInitCount);
}

std::unordered_map<SOCKET, Session*> gSessionMap;
std::unordered_map<DWORD, Session*> gSessionIdMap;
ActiveSessionList gActiveSessions;
WritableSessionList gWritableSessions;

namespace
{
	struct SessionMapReserveInitializer
	{
		SessionMapReserveInitializer() noexcept
		{
			gSessionMap.reserve(kSessionPoolInitCount);
			gSessionIdMap.reserve(kSessionPoolInitCount);
		}
	};

	SessionMapReserveInitializer gSessionMapReserveInitializer;
}

Session* AllocSession() noexcept
{
	Session* session = gSessionPool.Alloc();
	if (session == nullptr)
	{
		return nullptr;
	}

	session->Reset();
	return session;
}

void FreeSession(Session* session) noexcept
{
	if (session == nullptr)
	{
		return;
	}

	session->Reset();
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
	if (removeIndex == kInvalidActiveSessionIndex)
	{
		session->activeSessionIndex = kInvalidActiveSessionIndex;
		return;
	}

	assert(removeIndex < gActiveSessions.size() && gActiveSessions[removeIndex] == session);
	if (removeIndex >= gActiveSessions.size() || gActiveSessions[removeIndex] != session)
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

void AddWritableSession(Session* session) noexcept
{
	if (session == nullptr)
	{
		return;
	}

	if (session->wantWrite &&
		session->writableIndex < gWritableSessions.size() &&
		gWritableSessions[session->writableIndex] == session)
	{
		return;
	}

	if (gWritableSessions.empty())
	{
		gWritableSessions.reserve(kSessionPoolInitCount);
	}

	session->wantWrite = true;
	session->writableIndex = gWritableSessions.size();
	gWritableSessions.push_back(session);
}

void RemoveWritableSession(Session* session) noexcept
{
	if (session == nullptr)
	{
		return;
	}

	std::size_t removeIndex = session->writableIndex;
	if (removeIndex == kInvalidWritableSessionIndex)
	{
		session->wantWrite = false;
		session->writableIndex = kInvalidWritableSessionIndex;
		return;
	}

	assert(removeIndex < gWritableSessions.size() && gWritableSessions[removeIndex] == session);
	if (removeIndex >= gWritableSessions.size() || gWritableSessions[removeIndex] != session)
	{
		session->wantWrite = false;
		session->writableIndex = kInvalidWritableSessionIndex;
		return;
	}

	const std::size_t lastIndex = gWritableSessions.size() - 1;
	Session* movedSession = gWritableSessions[lastIndex];
	gWritableSessions[removeIndex] = movedSession;
	if (movedSession != nullptr)
	{
		movedSession->writableIndex = removeIndex;
	}

	gWritableSessions.pop_back();
	session->wantWrite = false;
	session->writableIndex = kInvalidWritableSessionIndex;
}
