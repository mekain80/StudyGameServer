#include "stdafx.h"

#include "Character.h"
#include "MemoryPool.h"

namespace
{
	constexpr unsigned int kCharacterPoolInitCount = 15000;
	MemoryPool<Character> gCharacterPool(true, kCharacterPoolInitCount);
}

std::unordered_map<DWORD, Character*> gCharacterMap;

Character* AllocCharacter() noexcept
{
	return gCharacterPool.Alloc();
}

void FreeCharacter(Character* character) noexcept
{
	if (character == nullptr)
	{
		return;
	}

	gCharacterPool.Free(character);
}

Character* FindCharacter(DWORD sessionID) noexcept
{
	auto iter = gCharacterMap.find(sessionID);
	if (iter == gCharacterMap.end())
	{
		return nullptr;
	}
	else
	{
		return iter->second;
	}
}

bool IsCharacterActive(const Character* character) noexcept
{
	if (character == nullptr)
	{
		return false;
	}

	Session* session = FindSession(character->sessionID);
	if (session == nullptr)
	{
		return false;
	}

	return session->disconnectFlag == false;
}
