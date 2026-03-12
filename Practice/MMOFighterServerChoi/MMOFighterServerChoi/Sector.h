#pragma once

#include <list>

#include "GameInfo.h"
#include "SectorPos.h"

struct Character;

constexpr int dfSECTOR_SIZE_X = 150;
constexpr int dfSECTOR_SIZE_Y = 150;

constexpr int dfSECTOR_MAX_X = ((dfRANGE_MOVE_RIGHT - dfRANGE_MOVE_LEFT) / dfSECTOR_SIZE_X) + 1;
constexpr int dfSECTOR_MAX_Y = ((dfRANGE_MOVE_BOTTOM - dfRANGE_MOVE_TOP) / dfSECTOR_SIZE_Y) + 1;

extern std::list<Character*> gSector[dfSECTOR_MAX_Y][dfSECTOR_MAX_X];

bool IsSameSector(const SectorPos& lhs, const SectorPos& rhs) noexcept;

// 특정 섹터 좌표 기준 주변 영향권 섹터를 구한다.
// @param sectorX 기준 섹터의 X 좌표
// @param sectorY 기준 섹터의 Y 좌표
// @param outSectorAround 기준 섹터를 포함한 주변 섹터 목록 출력 버퍼
void GetSectorAround(int sectorX, int sectorY, SectorAround* outSectorAround) noexcept;

void GetSectorAroundBySector(const SectorPos* secPos, SectorAround* outSectorAround) noexcept;

// 캐릭터 이동 시 이전 영향권에서 빠진 섹터와 새로 포함된 섹터를 구한다.
// @param character 현재 위치가 반영된 캐릭터
// @param outRemoveSector 이전 영향권에서 제외된 섹터 목록 출력 버퍼
// @param outAddSector 새 영향권에 추가된 섹터 목록 출력 버퍼
void GetUpdateSectorAround(Character* character, SectorAround* outRemoveSector, SectorAround* outAddSector) noexcept;


// 월드 좌표를 섹터 좌표로 변환한다.
// @param x 월드 X 좌표
// @param y 월드 Y 좌표
// @return sectorPos 계산된 섹터 좌표
SectorPos CalcSector(int x, int y) noexcept;

// 캐릭터를 현재 좌표에 해당하는 섹터에 등록한다.
// @param ch 등록할 캐릭터
void InsertSector(Character* ch) noexcept;

// 캐릭터를 현재 기록된 섹터에서 제거한다.
// @param ch 제거할 캐릭터
void RemoveSector(Character* ch) noexcept;

// 캐릭터가 다른 섹터로 이동했는지 확인하고 필요하면 섹터를 갱신한다.
// @param ch 섹터 갱신 대상 캐릭터
// @param oldPos 이동 전 섹터 좌표를 저장할 출력 버퍼, 필요 없으면 nullptr
// @return movedSector 섹터 변경 발생 여부
bool UpdateSector(Character* ch, SectorPos* oldPos = nullptr) noexcept;
