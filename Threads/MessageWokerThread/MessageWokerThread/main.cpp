#include "RingBuffer.h"

#include <windows.h>
#include <process.h>

#include <iostream>
#include <cstring>
#include <cassert>
#include <list>
#include <string>
#include <vector>
#include <random>
#include <winnt.h>

using namespace std;
#undef max;

// ------------------------------------------------------------
// Constants / Timing
// ------------------------------------------------------------
LARGE_INTEGER g_Freq{};
double  kDefaultTimeSec = 0.0011; // 0.050 / 5;
const double  kMinTimeSec = 0.001;
const double  kDecreaseTimeSec = 0.0000001;
const int     kStrMaxLen = 50;   // (현재 미사용)

// ------------------------------------------------------------
// Message Definitions
// ------------------------------------------------------------
struct st_MSG_HEAD
{
	short shType;
	short shPayloadLen;
};

#define dfJOB_ADD   0
#define dfJOB_DEL   1
#define dfJOB_SORT  2
#define dfJOB_FIND  3
#define dfJOB_PRINT 4
#define dfJOB_QUIT  5

// ------------------------------------------------------------
// Globals (Shared State)
// ------------------------------------------------------------

// Events
HANDLE g_WorkerEvent = nullptr;
HANDLE g_ExitEvent = nullptr;

// Content
list<string> g_List;

// Message Queue
RingBuffer g_MsgRingBuffer(400000);

// Sync
CRITICAL_SECTION g_CS;

// TPS Counters
long g_ADD_TPS = 0;
long g_DEL_TPS = 0;
long g_SORT_TPS = 0;
long g_FIND_TPS = 0;
long g_PRINT_TPS = 0;

// Threads
HANDLE g_MainThreadHandle = nullptr;
HANDLE g_WorkerThreadHandles[3]{};
HANDLE g_MonitoringThreadHandle = nullptr;

// ------------------------------------------------------------
// Helpers
// ------------------------------------------------------------
inline void Lock()
{
	EnterCriticalSection(&g_CS);
}

inline void Unlock()
{
	LeaveCriticalSection(&g_CS);
}


std::wstring MakeRandomWString(size_t len)
{
	static constexpr wchar_t kCharset[] =
		L"0123456789"
		L"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		L"abcdefghijklmnopqrstuvwxyz";

	thread_local std::mt19937 rng{ std::random_device{}() };
	std::uniform_int_distribution<size_t> dist(0, (sizeof(kCharset) / sizeof(wchar_t)) - 2);

	std::wstring s;
	s.reserve(len);

	for (size_t i = 0; i < len; ++i)
		s.push_back(kCharset[dist(rng)]);

	return s;
}

std::string MakeRandomStringA(size_t len)
{
	static constexpr char kCharset[] =
		"0123456789"
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"abcdefghijklmnopqrstuvwxyz";

	thread_local std::mt19937 rng{ std::random_device{}() };
	std::uniform_int_distribution<size_t> dist(0, (sizeof(kCharset) / sizeof(char)) - 2);

	std::string s;
	s.reserve(len);

	for (size_t i = 0; i < len; ++i)
		s.push_back(kCharset[dist(rng)]);

	return s;
}

// ------------------------------------------------------------
// Thread Entry Points (Forward Decls)
// ------------------------------------------------------------
unsigned int WINAPI MainThread(void* lpParam);
unsigned int WINAPI WorkerThread(void* lpParam);
unsigned int WINAPI MoniteringThread(void* lpParam);

// ------------------------------------------------------------
// Main Thread
// ------------------------------------------------------------
unsigned int WINAPI MainThread(void* /*lpParam*/)
{
	while (true)
	{
		LARGE_INTEGER start{}, end{};
		QueryPerformanceCounter(&start);

		st_MSG_HEAD msgHead{};
		msgHead.shType = rand() % dfJOB_QUIT;
		msgHead.shPayloadLen = 0;

		Lock();
		switch (msgHead.shType)
		{
		case dfJOB_ADD:
		{
			string str = "ABCD"; // MakeRandomStringA(5);
			msgHead.shPayloadLen = (short)str.size();
			int size = g_MsgRingBuffer.Enqueue(reinterpret_cast<char*>(&msgHead), sizeof(msgHead));
			if (size == 0)
			{
				cout << "BUFFER MAX!!!!!!!!!!!!!!!!";
				return 10;
			}
			g_MsgRingBuffer.Enqueue(const_cast<char*>(str.data()), (int)str.size());
			break;
		}
		case dfJOB_DEL:
		{
			int size = g_MsgRingBuffer.Enqueue(reinterpret_cast<char*>(&msgHead), sizeof(msgHead));
			if (size == 0)
			{
				cout << "BUFFER MAX!!!!!!!!!!!!!!!!";
				return 10;
			}
			break;
		}
		case dfJOB_SORT:
		{
			int size = g_MsgRingBuffer.Enqueue(reinterpret_cast<char*>(&msgHead), sizeof(msgHead));
			if (size == 0)
			{
				cout << "BUFFER MAX!!!!!!!!!!!!!!!!";
				return 10;
			}
			break;
		}
		case dfJOB_FIND:
		{
			string str = MakeRandomStringA(4);
			msgHead.shPayloadLen = (short)str.size();
			int size = g_MsgRingBuffer.Enqueue(reinterpret_cast<char*>(&msgHead), sizeof(msgHead));
			if (size == 0)
			{
				cout << "BUFFER MAX!!!!!!!!!!!!!!!!";
				return 10;
			}
			g_MsgRingBuffer.Enqueue(const_cast<char*>(str.data()), (int)str.size());
			break;
		}
		case dfJOB_PRINT:
		{
			int size = g_MsgRingBuffer.Enqueue(reinterpret_cast<char*>(&msgHead), sizeof(msgHead));
			if (size == 0)
			{
				cout << "BUFFER MAX!!!!!!!!!!!!!!!!";
				return 10;
			}
			break;
		}
		default:
			break;
		}
		Unlock();

		SetEvent(g_WorkerEvent);

		// 'Q' 눌림, 종료
		if (GetAsyncKeyState('Q') & 1)
		{
			cout << "Quit!" << endl;

			st_MSG_HEAD quitMsgHead{};
			quitMsgHead.shType = dfJOB_QUIT;
			quitMsgHead.shPayloadLen = 0;

			Lock();
			//if (g_MsgRingBuffer.GetFreeSize() < sizeof(st_MSG_HEAD) * 3)
			//{
			//	g_MsgRingBuffer.Resize(60000);
			//}
			g_MsgRingBuffer.Enqueue(reinterpret_cast<char*>(&quitMsgHead), sizeof(quitMsgHead));
			g_MsgRingBuffer.Enqueue(reinterpret_cast<char*>(&quitMsgHead), sizeof(quitMsgHead));
			g_MsgRingBuffer.Enqueue(reinterpret_cast<char*>(&quitMsgHead), sizeof(quitMsgHead));
			Unlock();

			SetEvent(g_WorkerEvent);
			SetEvent(g_ExitEvent);

			HANDLE handleArr[4]{};
			memcpy(handleArr, g_WorkerThreadHandles, sizeof(g_WorkerThreadHandles));
			handleArr[3] = g_MonitoringThreadHandle;

			DWORD r = WaitForMultipleObjects(4, handleArr, TRUE, INFINITE);
			if (r == WAIT_FAILED) {
				DWORD e = GetLastError();
				printf("WaitForMultipleObjects FAILED, err=%lu\n", e);
			}
			break;
		}

		QueryPerformanceCounter(&end);

		double seconds = (double)(end.QuadPart - start.QuadPart) / (double)g_Freq.QuadPart;
		double remain = kDefaultTimeSec - seconds;
		if (remain > 0)
		{
			Sleep((DWORD)(remain * 1000.0));

			// 프레임이 빨라지도록 설정
			kDefaultTimeSec = kDefaultTimeSec - kDecreaseTimeSec;
			kDefaultTimeSec = max(kDefaultTimeSec, kMinTimeSec);
		}
	}

	return 0;
}

// ------------------------------------------------------------
// Worker Thread
// ------------------------------------------------------------
unsigned int WINAPI WorkerThread(void* /*lpParam*/)
{
	const DWORD tid = GetCurrentThreadId();
	std::cout << "worker start: " << tid << "\n";

	while (1)
	{
		DWORD wr = WaitForSingleObject(g_WorkerEvent, INFINITE);

		do
		{
			st_MSG_HEAD head;
			std::string payload;

			// (1) "메시지 1개" 완전체 확보 후 dequeue
			Lock();

			const int use = g_MsgRingBuffer.GetUseSize();
			if (use < (int)sizeof(st_MSG_HEAD))
			{
				Unlock();
				break;
			}

			g_MsgRingBuffer.Peek((char*)&head, sizeof(head));

			const int need = (int)sizeof(st_MSG_HEAD) + (int)head.shPayloadLen;
			if (use < need)
			{
				Unlock();
				break;
			}

			const int h = g_MsgRingBuffer.Dequeue((char*)&head, sizeof(head));
			if (h == 0)
			{
				Unlock();
				break;
			}


			char* dst;
			int p;
			if (head.shPayloadLen > 0)
			{
				payload.resize((size_t)head.shPayloadLen);

				dst = &payload[0]; // C++11/14 data() const 문제 회피
				p = g_MsgRingBuffer.Dequeue(dst, (int)payload.size());
				if (p == 0)
				{
					Unlock();
					break;
				}
			}

			Unlock();

			// (2) 처리
			switch (head.shType)
			{
			case dfJOB_ADD:
			{
				Lock();
				if (payload != "ABCD")
				{
					int a = 1;
				}
				g_List.push_back(payload);
				InterlockedIncrement((long*)&g_ADD_TPS);
				Unlock();
				break;
			}
			case dfJOB_DEL:
			{
				Lock();
				if (!g_List.empty())
					g_List.pop_front();
				InterlockedIncrement((long*)&g_DEL_TPS);
				Unlock();
				break;
			}
			case dfJOB_SORT:
			{
				Lock();
				g_List.sort();
				InterlockedIncrement((long*)&g_SORT_TPS);
				Unlock();
				break;
			}
			case dfJOB_FIND:
			{
				bool found = false;

				Lock();
				for (const auto& s : g_List)
				{
					if (s == payload)
					{
						found = true;
						break;
					}
				}
				InterlockedIncrement((long*)&g_FIND_TPS);
				Unlock();

				if (found)
					std::cout << "FIND OK: " << payload << "\n";
				break;
			}
			case dfJOB_PRINT:
			{
				vector<string> snapshot;

				Lock();
				snapshot.assign(g_List.begin(), g_List.end());
				InterlockedIncrement((long*)&g_PRINT_TPS);
				Unlock();

				for (const auto& s : snapshot)
				{
					std::cout << s << ' ';
					if (s != "ABCD")
					{
						int a = 1;
					}
				}
				std::cout << "\n";
				break;
			}
			case dfJOB_QUIT:
				Lock();
				SetEvent(g_WorkerEvent);
				Unlock();
				return 0;
			default:
				break;
			}
		} while (g_MsgRingBuffer.GetUseSize() > sizeof(st_MSG_HEAD));
	}

	return 0;
}

// ------------------------------------------------------------
// Monitoring Thread
// ------------------------------------------------------------
unsigned int WINAPI MoniteringThread(void* /*lpParam*/)
{
	while (true)
	{
		DWORD exitResult = WaitForSingleObject(g_ExitEvent, 1000);
		if (exitResult == WAIT_OBJECT_0)
			return 0;

		Lock();
		cout << "==================" << endl;
		cout << "GET USE SIZE : " << g_MsgRingBuffer.GetUseSize() << endl;
		cout << "TPS : " << g_ADD_TPS + g_DEL_TPS + g_SORT_TPS + g_FIND_TPS + g_PRINT_TPS << endl;
		cout << "dfJOB_ADD TPS : " << g_ADD_TPS << endl;
		cout << "dfJOB_DEL TPS : " << g_DEL_TPS << endl;
		cout << "dfJOB_SORT TPS : " << g_SORT_TPS << endl;
		cout << "dfJOB_FIND TPS : " << g_FIND_TPS << endl;
		cout << "dfJOB_PRINT TPS : " << g_PRINT_TPS << endl;
		cout << "kDefaultTimeSec : " << kDefaultTimeSec << endl;
		cout << "==================" << endl;

		g_ADD_TPS = 0;
		g_DEL_TPS = 0;
		g_SORT_TPS = 0;
		g_FIND_TPS = 0;
		g_PRINT_TPS = 0;
		Unlock();
	}

	return 0;
}

// ------------------------------------------------------------
// Program Entry
// ------------------------------------------------------------
int main()
{
	QueryPerformanceFrequency(&g_Freq);

	g_WorkerEvent = CreateEvent(NULL, false, false, NULL);
	g_ExitEvent = CreateEvent(NULL, true, false, NULL);

	InitializeCriticalSection(&g_CS);

	g_MainThreadHandle = (HANDLE)_beginthreadex(nullptr, 0, MainThread, nullptr, 0, nullptr);

	g_WorkerThreadHandles[0] = (HANDLE)_beginthreadex(nullptr, 0, WorkerThread, nullptr, 0, nullptr);
	//g_WorkerThreadHandles[1] = (HANDLE)_beginthreadex(nullptr, 0, WorkerThread, nullptr, 0, nullptr);
	//g_WorkerThreadHandles[2] = (HANDLE)_beginthreadex(nullptr, 0, WorkerThread, nullptr, 0, nullptr);

	g_MonitoringThreadHandle = (HANDLE)_beginthreadex(nullptr, 0, MoniteringThread, nullptr, 0, nullptr);

	WaitForSingleObject(g_MainThreadHandle, INFINITE);

	// Handles cleanup
	CloseHandle(g_MainThreadHandle);
	CloseHandle(g_WorkerThreadHandles[0]);
	CloseHandle(g_WorkerThreadHandles[1]);
	CloseHandle(g_WorkerThreadHandles[2]);
	CloseHandle(g_MonitoringThreadHandle);

	CloseHandle(g_WorkerEvent);
	CloseHandle(g_ExitEvent);
	// CloseHandle(g_TODO);  // (로직 변경 없이 리팩토링만: 필요 시 사용자 쪽에서 정리)

	DeleteCriticalSection(&g_CS);


	return 0;
}
