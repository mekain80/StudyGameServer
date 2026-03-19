#include "stdafx.h"

#include <cassert>

#include "Character.h"
#include "Sector.h"

SectorCharacterList gSector[dfSECTOR_MAX_Y][dfSECTOR_MAX_X];

bool IsSameSector(const SectorPos& lhs, const SectorPos& rhs) noexcept
{
	return lhs.x == rhs.x && lhs.y == rhs.y;
}

namespace
{
	constexpr std::size_t kInvalidSectorIndex = static_cast<std::size_t>(-1);

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

	std::size_t FindCharacterIndex(const SectorCharacterList& characters, const Character* target) noexcept
	{
		for (std::size_t index = 0; index < characters.size(); ++index)
		{
			if (characters[index] == target)
			{
				return index;
			}
		}

		return kInvalidSectorIndex;
	}

	std::size_t GetCharacterIndexInSector(const SectorCharacterList& characters, const Character* target) noexcept
	{
		if (target == nullptr)
		{
			return kInvalidSectorIndex;
		}

		if (target->sectorIndex < characters.size() && characters[target->sectorIndex] == target)
		{
			return target->sectorIndex;
		}

		return FindCharacterIndex(characters, target);
	}

	void RemoveCharacterAt(SectorCharacterList& characters, std::size_t removeIndex) noexcept
	{
		assert(removeIndex < characters.size());

		const std::size_t lastIndex = characters.size() - 1;
		Character* movedCharacter = characters[lastIndex];
		characters[removeIndex] = movedCharacter;
		if (movedCharacter != nullptr)
		{
			movedCharacter->sectorIndex = removeIndex;
		}

		characters.pop_back();
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
	if (IsSameSector(ch->sector, secPos))
	{
		const std::size_t currentIndex = GetCharacterIndexInSector(sectorList, ch);
		if (currentIndex != kInvalidSectorIndex)
		{
			ch->sectorIndex = currentIndex;
			return;
		}
	}

	ch->sector = secPos;
	ch->sectorIndex = sectorList.size();
	sectorList.push_back(ch);
}

void RemoveSector(Character* ch) noexcept
{
	assert(ch != nullptr);

	const SectorPos secPos = ch->sector;
	if (!IsValidSectorPos(secPos))
	{
		ch->sectorIndex = kInvalidSectorIndex;
		return;
	}

	auto& curList = gSector[secPos.y][secPos.x];
	const std::size_t removeIndex = GetCharacterIndexInSector(curList, ch);
	if (removeIndex == kInvalidSectorIndex)
	{
		ch->sector = SectorPos{};
		ch->sectorIndex = kInvalidSectorIndex;
		return;
	}

	RemoveCharacterAt(curList, removeIndex);
	ch->sector = SectorPos{};
	ch->sectorIndex = kInvalidSectorIndex;
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
		const std::size_t removeIndex = GetCharacterIndexInSector(oldSectorList, ch);
		if (removeIndex != kInvalidSectorIndex)
		{
			RemoveCharacterAt(oldSectorList, removeIndex);
		}
	}

	auto& newSectorList = gSector[newSec.y][newSec.x];
	ch->sector = newSec;
	ch->sectorIndex = newSectorList.size();
	newSectorList.push_back(ch);

	return true;
}
