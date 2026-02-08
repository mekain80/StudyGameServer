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

using namespace std;

// ------------------------------------------------------------
// Constants / Timing
// ------------------------------------------------------------
LARGE_INTEGER g_Freq{};
const double  kDefaultTimeSec = 0.050;
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
RingBuffer g_MsgRingBuffer(50000);

// Sync
CRITICAL_SECTION g_CS;

// TPS Counters
int g_ADD_TPS = 0;
int g_DEL_TPS = 0;
int g_SORT_TPS = 0;
int g_FIND_TPS = 0;
int g_PRINT_TPS = 0;

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

void PrintState(RingBuffer& rb, const char* title)
{
	cout << "==== " << title << " ====\n";
	cout << "BufferSize : " << rb.GetBufferSize() << "\n";
	cout << "UseSize    : " << rb.GetUseSize() << "\n";
	cout << "FreeSize   : " << rb.GetFreeSize() << "\n";

	int useSize = rb.GetUseSize();
	if (useSize > 0)
	{
		char temp[1024] = { 0 };
		int peekSize = (useSize < (int)sizeof(temp) - 1) ? useSize : (int)sizeof(temp) - 1;
		int ret = rb.Peek(temp, peekSize);

		cout << "Peek(" << ret << ") : ";
		for (int i = 0; i < ret; ++i)
			cout << temp[i];
		cout << "\n";
	}
	else
	{
		cout << "Peek       : (empty)\n";
	}

	cout << "DirectEnqueueSize : " << rb.DirectEnqueueSize() << "\n";
	cout << "DirectDequeueSize : " << rb.DirectDequeueSize() << "\n";
	cout << endl;
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
			string str = MakeRandomStringA(5);
			msgHead.shPayloadLen = (short)str.size();
			g_MsgRingBuffer.Enqueue((const char*)&msgHead, sizeof(msgHead));
			g_MsgRingBuffer.Enqueue(str.data(), (int)str.size());
			break;
		}
		case dfJOB_DEL:
		{
			g_MsgRingBuffer.Enqueue((const char*)&msgHead, sizeof(msgHead));
			break;
		}
		case dfJOB_SORT:
		{
			g_MsgRingBuffer.Enqueue((const char*)&msgHead, sizeof(msgHead));
			break;
		}
		case dfJOB_FIND:
		{
			string str = MakeRandomStringA(5);
			msgHead.shPayloadLen = (short)str.size();
			g_MsgRingBuffer.Enqueue((const char*)&msgHead, sizeof(msgHead));
			g_MsgRingBuffer.Enqueue(str.data(), (int)str.size());
			break;
		}
		case dfJOB_PRINT:
		{
			g_MsgRingBuffer.Enqueue((const char*)&msgHead, sizeof(msgHead));
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
			g_MsgRingBuffer.Enqueue((const char*)&quitMsgHead, sizeof(quitMsgHead));
			Unlock();

			SetEvent(g_WorkerEvent);
			SetEvent(g_ExitEvent);

			HANDLE handleArr[4]{};
			memcpy(handleArr, g_WorkerThreadHandles, sizeof(g_WorkerThreadHandles));
			handleArr[3] = g_MonitoringThreadHandle;

			WaitForMultipleObjects(4, handleArr, TRUE, INFINITE);
			break;
		}

		QueryPerformanceCounter(&end);

		double seconds = (double)(end.QuadPart - start.QuadPart) / (double)g_Freq.QuadPart;
		double remain = kDefaultTimeSec - seconds;
		if (remain > 0)
			Sleep((DWORD)(remain * 1000.0));
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
			if (h != (int)sizeof(head))
			{
				Unlock();
				break;
			}

			if (head.shPayloadLen > 0)
			{
				payload.resize((size_t)head.shPayloadLen);

				char* dst = &payload[0]; // C++11/14 data() const 문제 회피
				const int p = g_MsgRingBuffer.Dequeue(dst, (int)payload.size());
				if (p != (int)payload.size())
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
				g_List.push_back(payload);
				++g_ADD_TPS;
				Unlock();
				break;
			}
			case dfJOB_DEL:
			{
				Lock();
				if (!g_List.empty())
					g_List.pop_front();
				++g_DEL_TPS;
				Unlock();
				break;
			}
			case dfJOB_SORT:
			{
				Lock();
				g_List.sort();
				++g_SORT_TPS;
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
				++g_FIND_TPS;
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
				++g_PRINT_TPS;
				Unlock();

				for (const auto& s : snapshot)
					std::cout << s << ' ';
				std::cout << "\n";
				break;
			}
			case dfJOB_QUIT:
				Lock();
				st_MSG_HEAD quitMsgHead;
				quitMsgHead.shType = dfJOB_QUIT;
				quitMsgHead.shPayloadLen = 0;
				g_MsgRingBuffer.Enqueue((const char*)&quitMsgHead, sizeof(quitMsgHead));
				SetEvent(g_WorkerEvent);
				Unlock();
				return 0;
			default:
				break;
			}
		} while (g_MsgRingBuffer.GetUseSize() > sizeof(st_MSG_HEAD));
	}
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
	g_WorkerThreadHandles[1] = (HANDLE)_beginthreadex(nullptr, 0, WorkerThread, nullptr, 0, nullptr);
	g_WorkerThreadHandles[2] = (HANDLE)_beginthreadex(nullptr, 0, WorkerThread, nullptr, 0, nullptr);

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
