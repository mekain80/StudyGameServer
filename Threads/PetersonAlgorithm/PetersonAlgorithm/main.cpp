#include <iostream>
#include <stdio.h>
#include <tchar.h>
#include <process.h>
#include <Windows.h>

volatile bool flag[2];
volatile int turn;

volatile int ans;
const int million = 100'000'000 / 1000;
const bool bDebug = true;

volatile LONG inCS = 0;		// 임계 역역에 진입한 스레드 개수
volatile LONG owner = 0;	// 마지막 진입 스레드 ID 기록

unsigned int WINAPI Thread1(LPVOID lpParam)
{
	volatile bool otherFlagStack = false;
	volatile int turnStack = 0;
	for (size_t i = 0; i < million; i++)
	{
		flag[0] = true;
		turn = 1;
		while (1)
		{
			otherFlagStack = flag[1];
			turnStack = turn;
			if (otherFlagStack == false || turnStack != 1)
			{
				break;
			}			
		}

		// ===== 임계 영역 진입 계측(검출용) =====
		LONG now = InterlockedIncrement(&inCS);
		if (now >= 2)
		{
			owner = 0;
			if (bDebug)
				__debugbreak();
		}

		// 임계 영역
		++ans;

		// ===== 임계 영역 이탈 계측 =====
		InterlockedDecrement(&inCS);

		flag[0] = false;
	}

	return 0;
}

unsigned int WINAPI Thread2(LPVOID lpParam)
{
	volatile bool otherFlagStack = false;
	volatile int turnStack = 0;
	for (size_t i = 0; i < million; i++)
	{
		flag[1] = true;
		turn = 0;

		while (1)
		{
			otherFlagStack = flag[0];
			turnStack = turn;
			if (otherFlagStack == false || turnStack != 0)
			{
				break;
			}
		}

		// ===== 임계 영역 진입 계측(검출용) =====
		LONG now = InterlockedIncrement(&inCS);
		if (now >= 2)
		{
			owner = 1;
			if (bDebug)
				__debugbreak();
		}

		++ans;

		// ===== 임계 영역 이탈 계측 =====
		InterlockedDecrement(&inCS);

		flag[1] = false;
	}

	return 0;
}

int _tmain()
{
	HANDLE hThread[2] = {};

	hThread[0] = (HANDLE)_beginthreadex(nullptr, 0, Thread1, nullptr, 0, nullptr);
	if (!hThread[0])
	{
		_tprintf(_T("Create Thread1 is fail\n"));
		return 1;
	}

	hThread[1] = (HANDLE)_beginthreadex(nullptr, 0, Thread2, nullptr, 0, nullptr);
	if (!hThread[1]) {
		_tprintf(_T("Create Thread2 is fail\n"));
		return 1;
	}

	WaitForMultipleObjects(2, hThread, TRUE, INFINITE);

	std::cout << "\n" << ans;

	CloseHandle(hThread[0]);
	CloseHandle(hThread[1]);

	return 0;
}
