#pragma once
#include <windows.h>
#include <string.h>
#include <stdio.h>
#include <limits>

const static int PROFILE_MAX_CNT = 512;
const static int PROFILE_NAME_LEN = 256;
const static int PROFILE_DELETE_CNT = 2;
const static WCHAR PROFILE_FILE_NAME[] = L"Profiler_";

struct profile
{
	long			mFlag;								// 프로파일의 사용 여부. (배열시에만)
	WCHAR			mName[PROFILE_NAME_LEN];			// 프로파일 샘플 이름.

	LARGE_INTEGER	mStartTime;							// 프로파일 샘플 실행 시간.

	__int64			mTotalTime;							// 전체 사용시간 카운터 Time.	(출력시 호출회수로 나누어 평균 구함)
	__int64			mMin[PROFILE_DELETE_CNT];			// 최소 사용시간 카운터 Time.	(초단위로 계산하여 저장 / [0] 가장최소 [1] 다음 최소 [2])
	__int64			mMax[PROFILE_DELETE_CNT];			// 최대 사용시간 카운터 Time.	(초단위로 계산하여 저장 / [0] 가장최대 [1] 다음 최대 [2])

	__int64			mCall;								// 누적 호출 횟수.
};

extern profile gProfileArray[PROFILE_MAX_CNT];
extern LARGE_INTEGER gProfileFreq;

profile* ProfileRegister(const char* name) noexcept;
void ProfileBegin(profile* prof) noexcept;
void ProfileEnd(profile* prof) noexcept;

// 하나의 함수 Profiling 시작
// Parameters: Profiling이름.
void ProfileBegin(const WCHAR* mName);
void ProfileEnd(const WCHAR* mName);

// Profiling 된 데이터를 Text 파일로 출력
//
// Parameters: (char *)출력될 파일 이름.
void ProfileDataOutText(const WCHAR* szFileName);

// 프로파일링 된 데이터를 모두 초기화
void ProfileReset(void);

class ProfileScope
{
public:
	explicit ProfileScope(profile* prof) noexcept
		: mProfile(prof)
	{
		ProfileBegin(mProfile);
	}

	~ProfileScope() noexcept
	{
		ProfileEnd(mProfile);
	}

private:
	profile* mProfile;
};

#define PROFILE_SCOPE() \
	static profile* const __profileEntry##__LINE__ = ProfileRegister(__FUNCSIG__); \
	ProfileScope __profileScope##__LINE__(__profileEntry##__LINE__)
