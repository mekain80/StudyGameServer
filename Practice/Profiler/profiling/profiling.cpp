#include "profiling.h"

profile gProfileArray[PROFILE_MAX_CNT];
LARGE_INTEGER gFreq;

profile* findProfile(const wchar_t* mName)
{
    for (int i = 0; i < PROFILE_MAX_CNT; ++i) {
        if (gProfileArray[i].mFlag && wcscmp(gProfileArray[i].mName, mName) == 0)
            return &gProfileArray[i];
    }
    return nullptr;
}

void ProfileBegin(const WCHAR* mName)
{
    if (gFreq.QuadPart == 0) {
        QueryPerformanceFrequency(&gFreq);
    }

    profile* prof = findProfile(mName);
    if (!prof) {
        for (int i = 0; i < PROFILE_MAX_CNT; ++i) {
            if (!gProfileArray[i].mFlag) {
                prof = &gProfileArray[i];
                wcsncpy_s(prof->mName, PROFILE_NAME_LEN, mName, _TRUNCATE);
                prof->mFlag = true;
                break;
            }
        }
    }
    if (!prof) return;

    QueryPerformanceCounter(&prof->mStartTime);
}


void ProfileEnd(const WCHAR* mName)
{
    profile* prof = findProfile(mName);
    if (!prof) {
        return;
    }

	LARGE_INTEGER end;
	QueryPerformanceCounter(&end);
    // 1) 나누지 말고 '틱' 그대로 사용
    __int64 deltaTicks = end.QuadPart - prof->mStartTime.QuadPart;

    // 2) 누적
    prof->mTotalTime += deltaTicks;
    ++prof->mCall;

    // 3) Min, Max 방금 측정한 deltaTicks로 min/max를 직접 갱신
    prof->mMin[0] = min(prof->mMin[0], deltaTicks);
    prof->mMax[0] = max(prof->mMax[0], deltaTicks);
}


void ProfileDataOutText(const WCHAR* szFileName)
{
    FILE* fp = nullptr;
    if (_wfopen_s(&fp, szFileName, L"wt, ccs=UTF-8") != 0 || !fp)
        return;

    fwprintf(fp, L"-------------------------------------------------------------------------------\r\n");
    fwprintf(fp, L"           Name  |     Average  |        Min   |        Max   |      Call |\r\n");
    fwprintf(fp, L"-------------------------------------------------------------------------------\r\n");

    for (int i = 0; i < PROFILE_MAX_CNT; ++i) {
        if (!gProfileArray[i].mFlag) continue;
        const profile& p = gProfileArray[i];

        // 틱 → us 변환 헬퍼
        auto ticksToUs = [](double ticks, double freq)->double {
            return (ticks * 1'000'000.0) / freq;
            };

        double avg_us = 0.0;
        if (p.mCall > 0) {
            double avgTicks = static_cast<double>(p.mTotalTime) / static_cast<double>(p.mCall);
            avg_us = ticksToUs(avgTicks, static_cast<double>(gFreq.QuadPart));
        }

        __int64 minTicks = p.mMin[0];
        __int64 maxTicks = p.mMax[0];
        // (PROFILE_DELETE_CNT > 1 이면 여기서 추가 슬롯도 비교)
        for (int j = 1; j < PROFILE_DELETE_CNT; ++j) {
            if (p.mMin[j] < minTicks) minTicks = p.mMin[j];
            if (p.mMax[j] > maxTicks) maxTicks = p.mMax[j];
        }

        double min_us = ticksToUs(static_cast<double>(minTicks), static_cast<double>(gFreq.QuadPart));
        double max_us = ticksToUs(static_cast<double>(maxTicks), static_cast<double>(gFreq.QuadPart));

        fwprintf(fp, L"%15ls | %10.4fus | %10.4fus | %10.4fus | %10lld\r\n",
            p.mName, avg_us, min_us, max_us, p.mCall);
    }

    fwprintf(fp, L"-------------------------------------------------------------------------------\r\n");
    fclose(fp);
}


void ProfileReset(void)
{
    for (int i = 0; i < PROFILE_MAX_CNT; ++i) {
        gProfileArray[i].mFlag = 0;
        gProfileArray[i].mName[0] = L'\0';
        gProfileArray[i].mTotalTime = 0;
        gProfileArray[i].mCall = 0;
        for (int j = 0; j < PROFILE_DELETE_CNT; ++j) {
            gProfileArray[i].mMin[j] = LLONG_MAX;
            gProfileArray[i].mMax[j] = LLONG_MIN;
        }
    }
}