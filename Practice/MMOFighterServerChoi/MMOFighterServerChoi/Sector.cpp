#include "stdafx.h"

#include <algorithm>
#include <cassert>

#include "Character.h"
#include "Sector.h"

std::list<Character*> gSector[dfSECTOR_MAX_Y][dfSECTOR_MAX_X];


bool IsSameSector(const SectorPos& lhs, const SectorPos& rhs) noexcept
{
	return lhs.x == rhs.x && lhs.y == rhs.y;
}

namespace
{
	struct SectorAroundBounds
	{
		short minX;
		short maxX;
		short minY;
		short maxY;
	};

	bool IsValidSectorPos(const SectorPos& sectorPos) noexcept
	{
		return sectorPos.x >= 0
			&& sectorPos.x < dfSECTOR_MAX_X
			&& sectorPos.y >= 0
			&& sectorPos.y < dfSECTOR_MAX_Y;
	}

	bool ContainsCharacter(const std::list<Character*>& characters, const Character* target) noexcept
	{
		return std::find(characters.begin(), characters.end(), target) != characters.end();
	}

	SectorAroundBounds GetSectorAroundBounds(const SectorPos& sectorPos) noexcept
	{
		SectorAroundBounds bounds{};
		bounds.minX = (sectorPos.x > 0) ? static_cast<short>(sectorPos.x - 1) : 0;
		bounds.maxX = (sectorPos.x + 1 < dfSECTOR_MAX_X) ? static_cast<short>(sectorPos.x + 1) : static_cast<short>(dfSECTOR_MAX_X - 1);
		bounds.minY = (sectorPos.y > 0) ? static_cast<short>(sectorPos.y - 1) : 0;
		bounds.maxY = (sectorPos.y + 1 < dfSECTOR_MAX_Y) ? static_cast<short>(sectorPos.y + 1) : static_cast<short>(dfSECTOR_MAX_Y - 1);
		return bounds;
	}

	bool IsSectorInsideBounds(const SectorPos& sectorPos, const SectorAroundBounds& bounds) noexcept
	{
		return sectorPos.x >= bounds.minX
			&& sectorPos.x <= bounds.maxX
			&& sectorPos.y >= bounds.minY
			&& sectorPos.y <= bounds.maxY;
	}
}

void GetSectorAround(int sectorX, int sectorY, SectorAround* outSectorAround) noexcept
{
	assert(outSectorAround != nullptr);

	outSectorAround->count = 0;

	for (int y = sectorY - 1; y <= sectorY + 1; ++y)
	{
		if (y < 0 || y >= dfSECTOR_MAX_Y)
		{
			continue;
		}

		for (int x = sectorX - 1; x <= sectorX + 1; ++x)
		{
			if (x < 0 || x >= dfSECTOR_MAX_X)
			{
				continue;
			}

			outSectorAround->around[outSectorAround->count] =
			{
				static_cast<short>(x),
				static_cast<short>(y)
			};
			++outSectorAround->count;
		}
	}
}

void GetSectorAroundBySector(const SectorPos* secPos, SectorAround* outSectorAround) noexcept
{
	assert(secPos != nullptr);
	assert(outSectorAround != nullptr);

	if (!IsValidSectorPos(*secPos))
	{
		outSectorAround->count = 0;
		return;
	}

	GetSectorAround(secPos->x, secPos->y, outSectorAround);
}

void GetUpdateSectorAround(
	const SectorPos& oldSector,
	const SectorPos& newSector,
	SectorAround* outRemoveSector,
	SectorAround* outAddSector) noexcept
{
	assert(outRemoveSector != nullptr);
	assert(outAddSector != nullptr);

	if (IsSameSector(oldSector, newSector))
	{
		outRemoveSector->count = 0;
		outAddSector->count = 0;
		return;
	}

	outRemoveSector->count = 0;
	outAddSector->count = 0;

	if (!IsValidSectorPos(oldSector))
	{
		GetSectorAround(newSector.x, newSector.y, outAddSector);
		return;
	}

	SectorAround oldAround{};
	SectorAround newAround{};
	GetSectorAround(oldSector.x, oldSector.y, &oldAround);
	GetSectorAround(newSector.x, newSector.y, &newAround);
	const SectorAroundBounds oldBounds = GetSectorAroundBounds(oldSector);
	const SectorAroundBounds newBounds = GetSectorAroundBounds(newSector);

	for (int index = 0; index < oldAround.count; ++index)
	{
		const SectorPos sectorPos = oldAround.around[index];
		if (!IsSectorInsideBounds(sectorPos, newBounds))
		{
			outRemoveSector->around[outRemoveSector->count] = sectorPos;
			++outRemoveSector->count;
		}
	}

	for (int index = 0; index < newAround.count; ++index)
	{
		const SectorPos sectorPos = newAround.around[index];
		if (!IsSectorInsideBounds(sectorPos, oldBounds))
		{
			outAddSector->around[outAddSector->count] = sectorPos;
			++outAddSector->count;
		}
	}
}

SectorPos CalcSector(int x, int y) noexcept
{
	int localX = x - dfRANGE_MOVE_LEFT;
	int localY = y - dfRANGE_MOVE_TOP;

	int sx = localX / dfSECTOR_SIZE_X;
	int sy = localY / dfSECTOR_SIZE_Y;

	if (sx < 0)
	{
		sx = 0;
	}
	else if (sx >= dfSECTOR_MAX_X)
	{
		sx = dfSECTOR_MAX_X - 1;
	}

	if (sy < 0)
	{
		sy = 0;
	}
	else if (sy >= dfSECTOR_MAX_Y)
	{
		sy = dfSECTOR_MAX_Y - 1;
	}

	SectorPos sectorPos{ static_cast<short>(sx), static_cast<short>(sy) };
	return sectorPos;
}

void InsertSector(Character* ch) noexcept
{
	assert(ch != nullptr);

	const SectorPos secPos = CalcSector(ch->x, ch->y);
	auto& sectorList = gSector[secPos.y][secPos.x];

	if (!ContainsCharacter(sectorList, ch))
	{
		sectorList.push_back(ch);
	}

	ch->sector = secPos;
}

void RemoveSector(Character* ch) noexcept
{
	assert(ch != nullptr);

	const SectorPos secPos = ch->sector;
	if (!IsValidSectorPos(secPos))
	{
		return;
	}

	auto& curList = gSector[secPos.y][secPos.x];

	for (auto iter = curList.begin(); iter != curList.end(); ++iter)
	{
		if (*iter == ch)
		{
			curList.erase(iter);
			ch->sector = SectorPos{};
			return;
		}
	}
}

bool UpdateSector(Character* ch, const SectorPos& newSec, SectorPos* oldPos) noexcept
{
	assert(ch != nullptr);

	const SectorPos oldSec = ch->sector;
	if (IsSameSector(oldSec, newSec))
	{
		return false;
	}

	if (oldPos != nullptr)
	{
		*oldPos = oldSec;
	}

	if (IsValidSectorPos(oldSec))
	{
		auto& oldSectorList = gSector[oldSec.y][oldSec.x];
		for (auto iter = oldSectorList.begin(); iter != oldSectorList.end(); ++iter)
		{
			if (*iter == ch)
			{
				oldSectorList.erase(iter);
				break;
			}
		}
	}

	auto& newSectorList = gSector[newSec.y][newSec.x];
	newSectorList.push_back(ch);
	ch->sector = newSec;

	return true;
}
