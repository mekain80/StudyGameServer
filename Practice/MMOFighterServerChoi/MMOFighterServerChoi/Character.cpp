#include "stdafx.h"

#include "Character.h"

std::unordered_map<DWORD, Character*> gCharacterMap;

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
