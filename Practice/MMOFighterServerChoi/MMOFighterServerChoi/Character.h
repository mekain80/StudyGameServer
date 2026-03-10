#pragma once

#include "Session.h"

struct Character
{
    Session* session = nullptr;
    DWORD sessionID = 0;

    BYTE action = 0;
    BYTE direction = 0;
    short x = 0;
    short y = 0;
    int HP = 0;

    // TODO Sector
};

extern std::unordered_map<DWORD, Character*> gCharacterMap;

Character* FindCharacter(DWORD sessionID) noexcept;
