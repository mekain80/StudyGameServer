#include "stdafx.h"
#include "Profiler.h"

profile gProfileArray[PROFILE_MAX_CNT];
LARGE_INTEGER gProfileFreq{};

namespace
{
	void CopyProfileName(const char* source, WCHAR* dest, size_t destSize) noexcept
	{
		if (dest == nullptr || destSize == 0)
		{
			return;
		}

		dest[0] = L'\0';
		if (source == nullptr)
		{
			return;
		}

		size_t index = 0;
		for (; index + 1 < destSize && source[index] != '\0'; ++index)
		{
			dest[index] = static_cast<unsigned char>(source[index]);
		}

		dest[index] = L'\0';
	}

	profile* FindProfile(const WCHAR* name) noexcept
	{
		if (name == nullptr)
		{
			return nullptr;
		}

		for (int index = 0; index < PROFILE_MAX_CNT; ++index)
		{
			if (gProfileArray[index].mFlag && wcscmp(gProfileArray[index].mName, name) == 0)
			{
				return &gProfileArray[index];
			}
		}

		return nullptr;
	}

	profile* AcquireProfile(const WCHAR* name) noexcept
	{
		if (name == nullptr)
		{
			return nullptr;
		}

		if (gProfileFreq.QuadPart == 0)
		{
			QueryPerformanceFrequency(&gProfileFreq);
		}

		profile* prof = FindProfile(name);
		if (prof != nullptr)
		{
			return prof;
		}

		for (int index = 0; index < PROFILE_MAX_CNT; ++index)
		{
			if (!gProfileArray[index].mFlag)
			{
				prof = &gProfileArray[index];
				wcsncpy_s(prof->mName, PROFILE_NAME_LEN, name, _TRUNCATE);
				prof->mFlag = true;
				return prof;
			}
		}

		return nullptr;
	}
}

profile* ProfileRegister(const char* name) noexcept
{
	WCHAR wideName[PROFILE_NAME_LEN];
	CopyProfileName(name, wideName, _countof(wideName));
	return AcquireProfile(wideName);
}

void ProfileBegin(profile* prof) noexcept
{
	if (prof == nullptr)
	{
		return;
	}

	QueryPerformanceCounter(&prof->mStartTime);
}

void ProfileEnd(profile* prof) noexcept
{
	if (prof == nullptr)
	{
		return;
	}

	LARGE_INTEGER endTick{};
	QueryPerformanceCounter(&endTick);

	const __int64 deltaTicks = endTick.QuadPart - prof->mStartTime.QuadPart;
	prof->mTotalTime += deltaTicks;
	++prof->mCall;
	prof->mMin[0] = min(prof->mMin[0], deltaTicks);
	prof->mMax[0] = max(prof->mMax[0], deltaTicks);
}

void ProfileBegin(const WCHAR* mName)
{
	ProfileBegin(AcquireProfile(mName));
}

void ProfileEnd(const WCHAR* mName)
{
	ProfileEnd(FindProfile(mName));
}

void ProfileDataOutText(const WCHAR* szFileName)
{
	FILE* fp = nullptr;
	if (_wfopen_s(&fp, szFileName, L"wt, ccs=UTF-8") != 0 || fp == nullptr)
	{
		return;
	}

	fwprintf(fp, L"-------------------------------------------------------------------------------\r\n");
	fwprintf(fp, L"           Name  |     Average  |        Min   |        Max   |      Call |\r\n");
	fwprintf(fp, L"-------------------------------------------------------------------------------\r\n");

	for (int index = 0; index < PROFILE_MAX_CNT; ++index)
	{
		if (!gProfileArray[index].mFlag)
		{
			continue;
		}

		const profile& p = gProfileArray[index];
		auto ticksToUs = [](double ticks, double freq) -> double
		{
			return (ticks * 1000000.0) / freq;
		};

		double avgUs = 0.0;
		if (p.mCall > 0)
		{
			const double avgTicks = static_cast<double>(p.mTotalTime) / static_cast<double>(p.mCall);
			avgUs = ticksToUs(avgTicks, static_cast<double>(gProfileFreq.QuadPart));
		}

		__int64 minTicks = p.mMin[0];
		__int64 maxTicks = p.mMax[0];
		for (int subIndex = 1; subIndex < PROFILE_DELETE_CNT; ++subIndex)
		{
			if (p.mMin[subIndex] < minTicks)
			{
				minTicks = p.mMin[subIndex];
			}

			if (p.mMax[subIndex] > maxTicks)
			{
				maxTicks = p.mMax[subIndex];
			}
		}

		const double minUs = ticksToUs(static_cast<double>(minTicks), static_cast<double>(gProfileFreq.QuadPart));
		const double maxUs = ticksToUs(static_cast<double>(maxTicks), static_cast<double>(gProfileFreq.QuadPart));
		fwprintf(fp, L"%15ls | %10.4fus | %10.4fus | %10.4fus | %10lld\r\n",
			p.mName,
			avgUs,
			minUs,
			maxUs,
			p.mCall);
	}

	fwprintf(fp, L"-------------------------------------------------------------------------------\r\n");
	fclose(fp);
}

void ProfileReset(void)
{
	for (int index = 0; index < PROFILE_MAX_CNT; ++index)
	{
		gProfileArray[index].mFlag = 0;
		gProfileArray[index].mName[0] = L'\0';
		gProfileArray[index].mTotalTime = 0;
		gProfileArray[index].mCall = 0;
		for (int subIndex = 0; subIndex < PROFILE_DELETE_CNT; ++subIndex)
		{
			gProfileArray[index].mMin[subIndex] = LLONG_MAX;
			gProfileArray[index].mMax[subIndex] = LLONG_MIN;
		}
	}
}
