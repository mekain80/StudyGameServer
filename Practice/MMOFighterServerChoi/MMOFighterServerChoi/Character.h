#pragma once

#include "Session.h"
#include "SectorPos.h"

struct Character
{
    DWORD sessionID = 0;

    BYTE action = 0;
    BYTE direction = 0;
    int x = 0;
    int y = 0;
    LONGLONG lastMoveTick = 0;          // 마지막으로 시간 계산한 시각
    double moveTimeRemainder = 0.0;     // 아직 20ms 한 칸이 안 돼서 남겨둔 시간
    int HP = 0;

    SectorPos sector;
};

extern std::unordered_map<DWORD, Character*> gCharacterMap;

Character* AllocCharacter() noexcept;
void FreeCharacter(Character* character) noexcept;
Character* FindCharacter(DWORD sessionID) noexcept;
bool IsCharacterActive(const Character* character) noexcept;
