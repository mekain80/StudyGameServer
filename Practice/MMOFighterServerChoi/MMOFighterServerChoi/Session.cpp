#include "stdafx.h"

#include <unordered_map>
#include "Session.h"

std::unordered_map<SOCKET, Session*> gSessionMap;
std::unordered_map<DWORD, Session*> gSessionIdMap;

Session* FindSession(DWORD sessionID) noexcept
{
	auto iter = gSessionIdMap.find(sessionID);
	if (iter == gSessionIdMap.end())
	{
		return nullptr;
	}

	return iter->second;
}
