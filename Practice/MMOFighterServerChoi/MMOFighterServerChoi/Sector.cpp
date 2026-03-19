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
	constexpr int kPrecomputedMoveDeltaRange = 1;
	constexpr int kPrecomputedMoveDeltaSize = (kPrecomputedMoveDeltaRange * 2) + 1;

	struct SectorAroundBounds
	{
		short minX;
		short maxX;
		short minY;
		short maxY;
	};

	struct SectorPrecomputedInfo
	{
		SectorAround around;
		SectorAroundBounds bounds;
	};

	struct SectorAroundDeltaInfo
	{
		SectorAround removeAround;
		SectorAround addAround;
	};

	SectorPrecomputedInfo gSectorPrecomputed[dfSECTOR_MAX_Y][dfSECTOR_MAX_X];
	SectorAroundDeltaInfo gSectorMoveDelta[dfSECTOR_MAX_Y][dfSECTOR_MAX_X][kPrecomputedMoveDeltaSize][kPrecomputedMoveDeltaSize];
	bool gSectorCacheInitialized = false;

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

	SectorAround BuildSectorAround(const SectorPos& sectorPos) noexcept
	{
		SectorAround sectorAround{};
		for (short y = static_cast<short>(sectorPos.y - 1); y <= sectorPos.y + 1; ++y)
		{
			if (y < 0 || y >= dfSECTOR_MAX_Y)
			{
				continue;
			}

			for (short x = static_cast<short>(sectorPos.x - 1); x <= sectorPos.x + 1; ++x)
			{
				if (x < 0 || x >= dfSECTOR_MAX_X)
				{
					continue;
				}

				sectorAround.around[sectorAround.count] = { x, y };
				++sectorAround.count;
			}
		}

		return sectorAround;
	}

	SectorAroundDeltaInfo BuildSectorAroundDelta(const SectorPos& oldSector, const SectorPos& newSector) noexcept
	{
		SectorAroundDeltaInfo deltaInfo{};
		if (IsSameSector(oldSector, newSector))
		{
			return deltaInfo;
		}

		const SectorPrecomputedInfo& oldInfo = gSectorPrecomputed[oldSector.y][oldSector.x];
		const SectorPrecomputedInfo& newInfo = gSectorPrecomputed[newSector.y][newSector.x];
		for (int index = 0; index < oldInfo.around.count; ++index)
		{
			const SectorPos sectorPos = oldInfo.around.around[index];
			if (!IsSectorInsideBounds(sectorPos, newInfo.bounds))
			{
				deltaInfo.removeAround.around[deltaInfo.removeAround.count] = sectorPos;
				++deltaInfo.removeAround.count;
			}
		}

		for (int index = 0; index < newInfo.around.count; ++index)
		{
			const SectorPos sectorPos = newInfo.around.around[index];
			if (!IsSectorInsideBounds(sectorPos, oldInfo.bounds))
			{
				deltaInfo.addAround.around[deltaInfo.addAround.count] = sectorPos;
				++deltaInfo.addAround.count;
			}
		}

		return deltaInfo;
	}

	void InitializeSectorPrecomputedData() noexcept
	{
		for (short y = 0; y < dfSECTOR_MAX_Y; ++y)
		{
			for (short x = 0; x < dfSECTOR_MAX_X; ++x)
			{
				const SectorPos sectorPos{ x, y };
				SectorPrecomputedInfo& sectorInfo = gSectorPrecomputed[y][x];
				sectorInfo.bounds = GetSectorAroundBounds(sectorPos);
				sectorInfo.around = BuildSectorAround(sectorPos);
			}
		}

		for (short y = 0; y < dfSECTOR_MAX_Y; ++y)
		{
			for (short x = 0; x < dfSECTOR_MAX_X; ++x)
			{
				const SectorPos oldSector{ x, y };
				for (int dy = -kPrecomputedMoveDeltaRange; dy <= kPrecomputedMoveDeltaRange; ++dy)
				{
					for (int dx = -kPrecomputedMoveDeltaRange; dx <= kPrecomputedMoveDeltaRange; ++dx)
					{
						const short newX = static_cast<short>(x + dx);
						const short newY = static_cast<short>(y + dy);
						SectorAroundDeltaInfo& deltaInfo =
							gSectorMoveDelta[y][x][dy + kPrecomputedMoveDeltaRange][dx + kPrecomputedMoveDeltaRange];
						if (newX < 0 || newX >= dfSECTOR_MAX_X || newY < 0 || newY >= dfSECTOR_MAX_Y)
						{
							deltaInfo = {};
							continue;
						}

						deltaInfo = BuildSectorAroundDelta(oldSector, SectorPos{ newX, newY });
					}
				}
			}
		}
	}
}

void InitializeSectorCache() noexcept
{
	if (gSectorCacheInitialized)
	{
		return;
	}

	InitializeSectorPrecomputedData();
	gSectorCacheInitialized = true;
}

void GetSectorAround(int sectorX, int sectorY, SectorAround* outSectorAround) noexcept
{
	assert(outSectorAround != nullptr);
	if (!gSectorCacheInitialized)
	{
		InitializeSectorCache();
	}
	const SectorPos sectorPos{ static_cast<short>(sectorX), static_cast<short>(sectorY) };
	if (!IsValidSectorPos(sectorPos))
	{
		outSectorAround->count = 0;
		return;
	}

	*outSectorAround = gSectorPrecomputed[sectorPos.y][sectorPos.x].around;
}

void GetSectorAroundBySector(const SectorPos* secPos, SectorAround* outSectorAround) noexcept
{
	assert(secPos != nullptr);
	assert(outSectorAround != nullptr);
	if (!gSectorCacheInitialized)
	{
		InitializeSectorCache();
	}

	if (!IsValidSectorPos(*secPos))
	{
		outSectorAround->count = 0;
		return;
	}

	*outSectorAround = gSectorPrecomputed[secPos->y][secPos->x].around;
}

void GetUpdateSectorAround(
	const SectorPos& oldSector,
	const SectorPos& newSector,
	SectorAround* outRemoveSector,
	SectorAround* outAddSector) noexcept
{
	assert(outRemoveSector != nullptr);
	assert(outAddSector != nullptr);
	if (!gSectorCacheInitialized)
	{
		InitializeSectorCache();
	}

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
		*outAddSector = gSectorPrecomputed[newSector.y][newSector.x].around;
		return;
	}

	const int dx = newSector.x - oldSector.x;
	const int dy = newSector.y - oldSector.y;
	if (dx >= -kPrecomputedMoveDeltaRange && dx <= kPrecomputedMoveDeltaRange &&
		dy >= -kPrecomputedMoveDeltaRange && dy <= kPrecomputedMoveDeltaRange)
	{
		const SectorAroundDeltaInfo& deltaInfo =
			gSectorMoveDelta[oldSector.y][oldSector.x][dy + kPrecomputedMoveDeltaRange][dx + kPrecomputedMoveDeltaRange];
		*outRemoveSector = deltaInfo.removeAround;
		*outAddSector = deltaInfo.addAround;
		return;
	}

	const SectorPrecomputedInfo& oldInfo = gSectorPrecomputed[oldSector.y][oldSector.x];
	const SectorPrecomputedInfo& newInfo = gSectorPrecomputed[newSector.y][newSector.x];

	for (int index = 0; index < oldInfo.around.count; ++index)
	{
		const SectorPos sectorPos = oldInfo.around.around[index];
		if (!IsSectorInsideBounds(sectorPos, newInfo.bounds))
		{
			outRemoveSector->around[outRemoveSector->count] = sectorPos;
			++outRemoveSector->count;
		}
	}

	for (int index = 0; index < newInfo.around.count; ++index)
	{
		const SectorPos sectorPos = newInfo.around.around[index];
		if (!IsSectorInsideBounds(sectorPos, oldInfo.bounds))
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
