#pragma once

#include <Windows.h>

#include "Character.h"

extern bool gShutdown;
extern LARGE_INTEGER gFreq;
extern LARGE_INTEGER gFrameStartTick;
extern LARGE_INTEGER gFrameEndTick;

// 게임 로직 한 틱을 처리하고, 실제 갱신이 일어났으면 true를 반환한다.
bool Update() noexcept;
// 현재 고해상도 타이머 값을 이동 계산 기준 시각으로 반환한다.
LONGLONG GetCurrentMoveTick() noexcept;
// 마지막 이동 시각부터 currentTick까지 누적된 시간을 20ms 이동 단위로 환산해 처리한다.
bool AdvanceCharacterByTime(Character* character, LONGLONG currentTick) noexcept;
// 캐릭터 좌표를 새 기준점으로 맞추고 남은 이동 누적 시간도 함께 초기화한다.
void SetCharacterPosition(Character* character, int x, int y, LONGLONG currentTick) noexcept;
// 좌표 변화로 섹터가 바뀌었는지 검사하고, 시야 범위 패킷을 갱신한다.
void UpdateCharacterSector(Character* character, Session* currentSession) noexcept;
// 주어진 방향으로 한 번 더 이동해도 맵 범위를 벗어나지 않는지 확인한다.
bool MoveCheck(BYTE direction, int x, int y) noexcept;

// 이동 방향을 기준으로 캐릭터가 바라보는 좌/우 방향을 정규화한다.
BYTE NormalizeViewDir(BYTE direction, BYTE currentDirection) noexcept;
// 방향 값에 대응하는 1회 이동량을 dx, dy로 계산한다.
void GetMoveDelta(BYTE direction, int& dx, int& dy) noexcept;
// 패킷에서 받은 이동 방향 값이 유효한지 검사한다.
bool IsValidMoveDirection(BYTE direction) noexcept;
// 정지 및 공격 패킷에서 사용하는 시선 방향 값이 유효한지 검사한다.
bool IsValidViewDirection(BYTE direction) noexcept;

// 기본 공격 1의 판정 범위 안에 target이 들어왔는지 확인한다.
bool IsHitAttack1(const Character* attacker, const Character* target, int centerX, int centerY) noexcept;
// 방향성 공격의 직사각형 판정 범위 안에 target이 들어왔는지 확인한다.
bool IsHitDirectionalAttack(const Character* attacker, const Character* target, int centerX, int centerY, int rangeX, int rangeY) noexcept;
