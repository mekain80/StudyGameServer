#include <iostream>
#include <windows.h>
#include <wchar.h>
#include "profiling.h"

WCHAR currTimestamp[32];

void MakeTimestamp()
{
	SYSTEMTIME st;
	GetLocalTime(&st);

	// YYYYMMDD_HHMMSS 형태
	swprintf_s(currTimestamp, 32, L"%04d%02d%02d_%02d%02d%02d",
		st.wYear, st.wMonth, st.wDay,
		st.wHour, st.wMinute, st.wSecond);
}

int add1(int a, int b) {
	ProfileBegin(L"add1");
	Sleep(1);

	int ret = a + b;
	ProfileEnd(L"add1");
	return ret;
}

int add10000(int a, int b) {
	ProfileBegin(L"add10000");

	int total = 0;
	for (size_t i = 0; i < 10000; i++)
	{
		total += (a + b);
	}
	ProfileEnd(L"add10000");

	return total;
}

struct A {
	int a;
	int b;
	char c;	
};

void structCopy() {
	ProfileBegin(L"structCopy");
	A a{ 1, 2, 'x' };
	A b = a;
	ProfileEnd(L"structCopy");
}

#pragma function(memcpy)             // ← 인트린식 해제 (이 파일/구간에만)
__declspec(noinline)                 // 인라이닝 방지 (어셈 확인 용이)
void memcpyFunc() {
	ProfileBegin(L"memcpyFunc");
	A a{ 1, 2, 'x' };
	A b;
	memcpy(&b, &a, sizeof(A));
	ProfileEnd(L"memcpyFunc");
}

int main() {
	ProfileReset();
	//for (size_t i = 0; i < 10; i++) {
	//	std::cout << i << std::endl;
	//	add1(i, i);
	//}
	//for (size_t i = 0; i < 1000; i++) add10000(i, i);
	//for (size_t i = 0; i < 10000; i++) structCopy();
	//for (size_t i = 0; i < 10000; i++) memcpyFunc();
	structCopy();
	memcpyFunc();
	char c = 'c';
	int a = 1;
	int b = 2;
	if (a < b) {
		std::cout << a;
	}
	else if (a <= b) {
		std::cout << a;
	}

	MakeTimestamp();

	WCHAR fileName[MAX_PATH];
	swprintf_s(fileName, L"%s%ls.txt", PROFILE_FILE_NAME, currTimestamp);
	ProfileDataOutText(fileName);

	// 확인 출력
	char utf8[256];
	int len = WideCharToMultiByte(CP_UTF8, 0, fileName, -1, utf8, sizeof(utf8), NULL, NULL);
	if (len > 0) {
		printf("Saved: %s\n", utf8);
	}
}

