#pragma once
#include <windows.h>
#include <string.h>
#include <stdio.h>
#include <limits>

const static int PROFILE_MAX_CNT = 100;
const static int PROFILE_NAME_LEN = 65;
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
extern LARGE_INTEGER gFreq;

void ProfileBegin(const WCHAR* mName);
void ProfileEnd(const WCHAR* mName);

// Profiling 된 데이터를 Text 파일로 출력
// @param szFileName (char *)출력될 파일 이름.
void ProfileDataOutText(const WCHAR* szFileName);

// 프로파일링 된 데이터를 모두 초기화
void ProfileReset(void);