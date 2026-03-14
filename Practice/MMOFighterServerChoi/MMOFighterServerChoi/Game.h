#pragma once

#include <Windows.h>

#include "Character.h"

extern bool gShutdown;
extern LARGE_INTEGER gFreq;
extern LARGE_INTEGER gFrameStartTick;
extern LARGE_INTEGER gFrameEndTick;

void Update() noexcept;
void UpdateCharacterSector(Character* character, Session* currentSession) noexcept;
bool MoveCheck(BYTE direction, int x, int y) noexcept;

BYTE NormalizeViewDir(BYTE direction, BYTE currentDirection) noexcept;
void GetMoveDelta(BYTE direction, int& dx, int& dy) noexcept;
bool IsValidMoveDirection(BYTE direction) noexcept;
bool IsValidViewDirection(BYTE direction) noexcept;
WORD GetExpectedBodySize(BYTE packetType) noexcept;

bool IsHitAttack1(const Character* attacker, const Character* target, int centerX, int centerY) noexcept;
bool IsHitDirectionalAttack(const Character* attacker, const Character* target, int centerX, int centerY, int rangeX, int rangeY) noexcept;
