#define _CRT_SECURE_NO_WARNINGS // 구형 C 함수 사용 시 경고 끄기
#define _WINSOCK_DEPRECATED_NO_WARNINGS // 구형 소켓 API 사용 시 경고 끄기

#include <winsock2.h> // 윈속2 메인 헤더
#include <windows.h>
#include <ws2tcpip.h> // 윈속2 확장 헤더

#include <tchar.h> // _T(), ...
#include <stdio.h> // printf(), ...
#include <stdlib.h> // exit(), ...
#include <string.h> // strncpy(), ...
#include <process.h> // _beginthreadex
#include <stdint.h>
#include <unordered_map>

#include "Profiler.h"
#include "RingBuffer.h"

#pragma comment(lib, "ws2_32") // ws2_32.lib 링크

int sendImmediateComplete, sendPending;

// WSAGetLastError() 기준으로 오류 메시지를 표시하고 프로세스를 종료한다.
void err_quit(const char* msg)
{
	LPVOID lpMsgBuf;
	FormatMessageA(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, WSAGetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(char*)&lpMsgBuf, 0, NULL);
	MessageBoxA(NULL, (const char*)lpMsgBuf, msg, MB_ICONERROR);
	LocalFree(lpMsgBuf);
	exit(1);
}

// WSAGetLastError() 기준으로 오류 메시지를 표시한다.
void err_display(const char* msg)
{
	LPVOID lpMsgBuf;
	FormatMessageA(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, WSAGetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(char*)&lpMsgBuf, 0, NULL);
	printf("[%s] %s\n", msg, (char*)lpMsgBuf);
	LocalFree(lpMsgBuf);
}

// 전달받은 Win32/WinSock 오류 코드 메시지를 표시한다.
void err_display(int errcode)
{
	LPVOID lpMsgBuf;
	FormatMessageA(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, errcode,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(char*)&lpMsgBuf, 0, NULL);
	printf("[오류] %s\n", (char*)lpMsgBuf);
	LocalFree(lpMsgBuf);
}

#define SERVERPORT 6000//9000
#define BUFSIZE    512

enum IOType
{
	IO_RECV = 1,
	IO_SEND = 2
};

struct OverlappedEx
{
	OVERLAPPED ov;
	IOType type;
};

struct Session
{
	uint64_t id;
	SOCKET sock;

	OverlappedEx recvOv;
	OverlappedEx sendOv;
	WSABUF recvBuf;
	WSABUF sendBuf;
	char recvStorage[BUFSIZE];

	RingBuffer sendQ;
	CRITICAL_SECTION sendLock;
	LONG sending;
	LONG ioCount;
};

static HANDLE g_hcp = NULL;
static SOCKET g_listenSock = INVALID_SOCKET;
static CRITICAL_SECTION g_sessionLock;
static std::unordered_map<uint64_t, Session*> g_sessions;
static volatile LONG64 g_sessionId = 0;

// IOCP 완료 패킷을 처리하는 작업자 스레드 루프.
unsigned __stdcall WorkerThread(void* arg);

// accept()를 전담하는 스레드 루프.
unsigned __stdcall AcceptThread(void* arg);

// 전역 세션 맵에 세션을 등록한다.
static void AddSession(Session* s)
{
	EnterCriticalSection(&g_sessionLock);
	g_sessions.emplace(s->id, s);
	LeaveCriticalSection(&g_sessionLock);
}

// 전역 세션 맵에서 세션을 제거한다.
static void RemoveSession(Session* s)
{
	EnterCriticalSection(&g_sessionLock);
	auto it = g_sessions.find(s->id);
	if (it != g_sessions.end())
	{
		g_sessions.erase(it);
	}
	LeaveCriticalSection(&g_sessionLock);
}

// 세션을 세션 맵에서 제거하고 관련 리소스를 해제한다.
static void DestroySession(Session* s)
{
	RemoveSession(s);
	if (s->sock != INVALID_SOCKET)
	{
		closesocket(s->sock);
		s->sock = INVALID_SOCKET;
	}
	DeleteCriticalSection(&s->sendLock);
	delete s;
}

// 종료 절차를 시작한다. 현재 outstanding I/O가 없으면 즉시 세션을 파괴한다.
static void BeginClose(Session* s)
{
	if (InterlockedCompareExchange(&s->ioCount, 0, 0) == 0)
	{
		DestroySession(s);
	}
}

// I/O 완료 시 outstanding 카운트를 감소시키고, 0이 되면 세션을 파괴한다.
static void CompleteIO(Session* s)
{
	LONG remain = InterlockedDecrement(&s->ioCount);
	if (remain == 0)
	{
		DestroySession(s);
	}
}

// 새 세션 객체를 생성하고 IOCP용 상태를 초기화한다.
static Session* CreateSession(SOCKET sock)
{
	Session* s = new Session;
	s->id = (uint64_t)InterlockedIncrement64(&g_sessionId);
	s->sock = sock;

	ZeroMemory(&s->recvOv.ov, sizeof(OVERLAPPED));
	ZeroMemory(&s->sendOv.ov, sizeof(OVERLAPPED));
	s->recvOv.type = IO_RECV;
	s->sendOv.type = IO_SEND;

	s->recvBuf.buf = s->recvStorage;
	s->recvBuf.len = BUFSIZE;
	s->sendBuf.buf = nullptr;
	s->sendBuf.len = 0;

	InitializeCriticalSection(&s->sendLock);
	s->sending = 0;
	s->ioCount = 0;

	return s;
}

// 비동기 수신(WSARecv)을 1회 게시한다.
static bool RecvPost(Session* s)
{
	DWORD flags = 0;
	DWORD recvbytes = 0;

	ZeroMemory(&s->recvOv.ov, sizeof(OVERLAPPED));
	s->recvOv.type = IO_RECV;

	InterlockedIncrement(&s->ioCount);
	int retval = WSARecv(s->sock, &s->recvBuf, 1, &recvbytes, &flags, &s->recvOv.ov, NULL);
	if (retval == SOCKET_ERROR) {
		int err = WSAGetLastError();
		if (err != WSA_IO_PENDING) {
			InterlockedDecrement(&s->ioCount);
			BeginClose(s);
			return false;
		}
	}

	return true;
}

// sendQ의 래핑 구간까지 WSABUF[2]로 묶어 비동기 송신(WSASend)으로 게시한다.
static bool SendPost(Session* s)
{
	EnterCriticalSection(&s->sendLock);
	int used = s->sendQ.GetUseSize();
	if (used <= 0) {
		InterlockedExchange(&s->sending, 0);
		LeaveCriticalSection(&s->sendLock);
		return false;
	}

	int direct = s->sendQ.GetDirectDequeueSize();
	int wrap = used - direct;
	char* front = s->sendQ.GetFront();
	char* rear = s->sendQ.GetRear();
	LeaveCriticalSection(&s->sendLock);
	WSABUF bufs[2];
	bufs[0].buf = front;
	bufs[0].len = direct;
	// wrap > 0이면 rear가 (버퍼 시작 + wrap)를 가리키므로, 시작 포인터는 (rear - wrap).
	bufs[1].buf = (wrap > 0) ? (rear - wrap) : nullptr;
	bufs[1].len = (wrap > 0) ? wrap : 0;

	ZeroMemory(&s->sendOv.ov, sizeof(OVERLAPPED));
	s->sendOv.type = IO_SEND;

	DWORD sendbytes = 0;
	InterlockedIncrement(&s->ioCount);
	DWORD bufCount = (bufs[1].len > 0) ? 2 : 1;
	int retval = WSASend(s->sock, bufs, bufCount, &sendbytes, 0, &s->sendOv.ov, NULL);
	if (retval == SOCKET_ERROR) {
		int err = WSAGetLastError();
		if (err != WSA_IO_PENDING) {
			InterlockedDecrement(&s->ioCount);
			BeginClose(s);
			return false;
		}
	}

	return true;
}

// 전송 데이터를 sendQ에 적재하고, 유휴 상태라면 송신 파이프라인을 시작한다.
static bool EnqueueSend(Session* s, const char* data, int len)
{
	if (len <= 0) return true;

	EnterCriticalSection(&s->sendLock);
	bool ok = s->sendQ.Enqueue(data, len);
	LeaveCriticalSection(&s->sendLock);

	if (!ok) return false;

	if (InterlockedCompareExchange(&s->sending, 1, 0) == 0)
	{
		SendPost(s);
	}

	return true;
}

int main(int argc, char* argv[])
{
	int retval;

	// 윈속 초기화
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return 1;

	InitializeCriticalSection(&g_sessionLock);

	// 입출력 완료 포트 생성
	g_hcp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 1);
	if (g_hcp == NULL) return 1;

	// CPU 개수 확인
	SYSTEM_INFO si;
	GetSystemInfo(&si);

	// (CPU 개수 * 2)개의 작업자 스레드 생성
	HANDLE hThread;
	for (int i = 0; i < (int)si.dwNumberOfProcessors * 2; i++) {
		hThread = (HANDLE)_beginthreadex(NULL, 0, WorkerThread, g_hcp, 0, NULL);
		if (hThread == NULL) return 1;
		CloseHandle(hThread);
	}

	// 리슨 소켓 생성 (OVERLAPPED)
	g_listenSock = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (g_listenSock == INVALID_SOCKET) err_quit("socket()");

	// bind()
	struct sockaddr_in serveraddr;
	memset(&serveraddr, 0, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons(SERVERPORT);
	retval = bind(g_listenSock, (struct sockaddr*)&serveraddr, sizeof(serveraddr));
	if (retval == SOCKET_ERROR) err_quit("bind()");

	// listen()
	retval = listen(g_listenSock, SOMAXCONN);
	if (retval == SOCKET_ERROR) err_quit("listen()");

	// Accept 전용 스레드 시작 (프로세스 생존 유지 역할)
	HANDLE hAcceptThread = (HANDLE)_beginthreadex(NULL, 0, AcceptThread, (void*)g_listenSock, 0, NULL);
	if (hAcceptThread == NULL) return 1;

	// accept 스레드가 종료되면 프로세스 종료
	WaitForSingleObject(hAcceptThread, INFINITE);
	CloseHandle(hAcceptThread);

	DeleteCriticalSection(&g_sessionLock);

	// 윈속 종료
	WSACleanup();
	return 0;
}

// 완료된 RECV/SEND를 처리하고 다음 I/O를 이어 붙이는 IOCP 워커.
unsigned __stdcall WorkerThread(void* arg)
{
	HANDLE hcp = (HANDLE)arg;

	while (1) {
		DWORD cbTransferred = 0;
		ULONG_PTR completionKey = 0;
		LPOVERLAPPED ov = nullptr;

		BOOL ok = GetQueuedCompletionStatus(hcp, &cbTransferred, &completionKey, &ov, INFINITE);
		// ov == nullptr 이면 "어떤 세션의 I/O 완료"를 꺼낸 게 아님
		// (예: 타임아웃/큐 핸들 오류/워커 종료 신호). 특정 연결 종료 대상이 없다.
		if (ov == nullptr) {
			continue;
		}

		Session* s = (Session*)completionKey;
		OverlappedEx* ex = (OverlappedEx*)ov;

		// ov != nullptr 인 상태에서 실패/0바이트면 해당 세션 I/O 종료 상황
		// ok == false : I/O 오류 완료
		// cbTransferred == 0 : 상대방 정상 종료(FIN) 포함 종료 상황
		if (ok == false || cbTransferred == 0) {
			BeginClose(s);
			CompleteIO(s);
			continue;
		}

		if (ex->type == IO_RECV) {
			// 받은 데이터 출력
			char addr[INET_ADDRSTRLEN] = { 0 };
			struct sockaddr_in clientaddr;
			int addrlen = sizeof(clientaddr);
			if (getpeername(s->sock, (struct sockaddr*)&clientaddr, &addrlen) == 0) {
				inet_ntop(AF_INET, &clientaddr.sin_addr, addr, sizeof(addr));
			}

			int printLen = (cbTransferred > BUFSIZE) ? BUFSIZE : (int)cbTransferred;
			char msg[BUFSIZE + 1];
			memcpy(msg, s->recvStorage, printLen);
			msg[printLen] = '\0';
			printf("[TCP/%s:%d] %s\n", addr, ntohs(clientaddr.sin_port), msg);

			// RECV 완료 데이터는 Echo를 위해 SendQ에 넣고 전송 시작/이어붙임, EnqueSend는 sending이 0일 때 SendPost 호출
			if (!EnqueueSend(s, s->recvStorage, (int)cbTransferred)) {
				BeginClose(s);
				CompleteIO(s);
				continue;
			}

			// 파이프라인 유지를 위해 다음 RECV를 즉시 다시 게시
			RecvPost(s);
		}
		else if (ex->type == IO_SEND) {
			// SEND 완료된 바이트만큼 SendQ front를 당겨 실제로 소비 처리
			EnterCriticalSection(&s->sendLock);
			s->sendQ.MoveFront((int)cbTransferred);
			int remain = s->sendQ.GetUseSize();
			LeaveCriticalSection(&s->sendLock);

			if (remain > 0) {
				// 아직 보낼 데이터가 남았으면 다음 SEND 연속 게시
				SendPost(s);
			}
			else {
				// 큐가 비었으면 sending 플래그를 내려 신규 전송 게시를 허용
				InterlockedExchange(&s->sending, 0);
			}
		}

		CompleteIO(s);
	}

	return 0;
}

// 클라이언트 연결을 수락하고 세션/IOCP 바인딩 후 최초 Recv를 게시한다.
unsigned __stdcall AcceptThread(void* arg)
{
	SOCKET listenSock = (SOCKET)arg;

	while (1) {
		struct sockaddr_in clientaddr;
		int addrlen = sizeof(clientaddr);
		SOCKET client_sock = accept(listenSock, (struct sockaddr*)&clientaddr, &addrlen);
		if (client_sock == INVALID_SOCKET) {
			int err = WSAGetLastError();
			if (err == WSAENOTSOCK || err == WSAEINVAL) {
				break; // 리슨 소켓 닫힘
			}
			err_display("accept()");
			continue;
		}

		// 접속한 클라이언트 정보 출력
		char addr[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &clientaddr.sin_addr, addr, sizeof(addr));
		printf("\n[TCP 서버] 클라이언트 접속: IP 주소=%s, 포트 번호=%d\n",
			addr, ntohs(clientaddr.sin_port));

		// 세션 생성
		Session* s = CreateSession(client_sock);

		// 소켓과 입출력 완료 포트 연결
		if (CreateIoCompletionPort((HANDLE)client_sock, g_hcp, (ULONG_PTR)s, 0) == NULL) {
			BeginClose(s);
			continue;
		}

		AddSession(s);

		// 송신 소켓 버퍼 0 설정
		//int snd = 0;
		//setsockopt(s->sock, SOL_SOCKET, SO_SNDBUF, (char*)&snd, sizeof(snd));

		// 비동기 입출력 시작
		RecvPost(s);
	}

	return 0;
}
