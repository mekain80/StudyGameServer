#pragma once

#include <Windows.h>

#include "Session.h"

extern LARGE_INTEGER gFreq;
extern LARGE_INTEGER gFrameStartTick;
extern LARGE_INTEGER gFrameEndTick;

void Update() noexcept;
bool MoveCheck(BYTE direction, int x, int y) noexcept;

BYTE NormalizeViewDir(BYTE direction) noexcept;
void GetMoveDelta(BYTE direction, int& dx, int& dy) noexcept;
bool IsValidMoveDirection(BYTE direction) noexcept;
bool IsValidViewDirection(BYTE direction) noexcept;
WORD GetExpectedBodySize(BYTE packetType) noexcept;

bool IsHitAttack1(const Session* attacker, const Session* target, int centerX, int centerY) noexcept;
bool IsHitDirectionalAttack(const Session* attacker, const Session* target, int centerX, int centerY, int rangeX, int rangeY) noexcept;
