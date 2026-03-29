#include "LanServer.h"

#include "Logger.h"
#include "DefineServer.h"
#include "SerializedBuffer.h"
#include "Session.h"

LanServer* gServer = nullptr;
static MemoryPool<Session> gSessionPool(true, SESSION_POOL_SIZE);

unsigned __stdcall WorkerThreadProc(void* param)
{
	LanServer* server = static_cast<LanServer*>(param);
	return static_cast<unsigned int>(server->WorkerThread());
}

unsigned __stdcall AcceptorThreadProc(void* param)
{
	LanServer* server = static_cast<LanServer*>(param);
	return static_cast<unsigned int>(server->AccepterThread());
}

LanServer::LanServer()
	: mSessionCount(0)
	, mMaxSessionCount(0)
	, mCurrentSessionID(0)
	, mListenSocket(INVALID_SOCKET)
	, mMaxWorkerThreadCount(0)
	, mWorkerRun(TRUE)
	, mAcceptorRun(TRUE)
	, mAcceptorThread(nullptr)
	, mIOCPHandle(nullptr)
{
	InitializeCriticalSection(&mSessionMapLock);
}

LanServer::~LanServer() noexcept
{
	DeleteCriticalSection(&mSessionMapLock);
}

void LanServer::PrintSystemMessage(const WCHAR* message) const
{
	if (message == nullptr)
	{
		return;
	}

	if (g_Logger != nullptr)
	{
		g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"%s", message);
		return;
	}

	wprintf(L"%s\n", message);
}

bool LanServer::Start(const char* openIP, USHORT port, USHORT createWorkerThreadCount, USHORT maxWorkerThreadCount, INT maxSessionCount)
{
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		if (g_Logger != nullptr)
		{
			g_Logger->WriteLog(L"ERROR", LOG_LEVEL::ERR, L"WSAStartup failed : %d", WSAGetLastError());
		}
		OnError(WSAGetLastError(), "WSAStartup failed");
		return false;
	}

	mMaxSessionCount = maxSessionCount;
	mMaxWorkerThreadCount = maxWorkerThreadCount;

	mListenSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_OVERLAPPED);
	if (mListenSocket == INVALID_SOCKET)
	{
		if (g_Logger != nullptr)
		{
			g_Logger->WriteLog(L"ERROR", LOG_LEVEL::ERR, L"WSASocket failed : %d", WSAGetLastError());
		}
		OnError(WSAGetLastError(), "WSASocket failed");
		return false;
	}

	SOCKADDR_IN serverAddr;
	ZeroMemory(&serverAddr, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	InetPtonA(AF_INET, openIP, &serverAddr.sin_addr);
	serverAddr.sin_port = htons(port);

	if (bind(mListenSocket, reinterpret_cast<SOCKADDR*>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR)
	{
		if (g_Logger != nullptr)
		{
			g_Logger->WriteLog(L"ERROR", LOG_LEVEL::ERR, L"bind failed : %d", WSAGetLastError());
		}
		OnError(WSAGetLastError(), "bind failed");
		return false;
	}

	DWORD sendBufferSize = 0;
	if (setsockopt(mListenSocket, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<char*>(&sendBufferSize), sizeof(sendBufferSize)) == SOCKET_ERROR)
	{
		if (g_Logger != nullptr)
		{
			g_Logger->WriteLog(L"ERROR", LOG_LEVEL::ERR, L"setsockopt(SO_SNDBUF) failed : %d", WSAGetLastError());
		}
		OnError(WSAGetLastError(), "setsockopt(SO_SNDBUF) failed");
		return false;
	}

	LINGER lingerOption;
	lingerOption.l_onoff = 1;
	lingerOption.l_linger = 0;
	if (setsockopt(mListenSocket, SOL_SOCKET, SO_LINGER, reinterpret_cast<char*>(&lingerOption), sizeof(lingerOption)) == SOCKET_ERROR)
	{
		if (g_Logger != nullptr)
		{
			g_Logger->WriteLog(L"ERROR", LOG_LEVEL::ERR, L"setsockopt(SO_LINGER) failed : %d", WSAGetLastError());
		}
		OnError(WSAGetLastError(), "setsockopt(SO_LINGER) failed");
		return false;
	}

	if (listen(mListenSocket, SOMAXCONN_HINT(65535)) == SOCKET_ERROR)
	{
		if (g_Logger != nullptr)
		{
			g_Logger->WriteLog(L"ERROR", LOG_LEVEL::ERR, L"listen failed : %d", WSAGetLastError());
		}
		OnError(WSAGetLastError(), "listen failed");
		return false;
	}

	mIOCPHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, mMaxWorkerThreadCount);
	if (mIOCPHandle == nullptr)
	{
		if (g_Logger != nullptr)
		{
			g_Logger->WriteLog(L"ERROR", LOG_LEVEL::ERR, L"CreateIoCompletionPort failed : %d", GetLastError());
		}
		OnError(GetLastError(), "CreateIoCompletionPort failed");
		return false;
	}

	mAcceptorThread = reinterpret_cast<HANDLE>(_beginthreadex(nullptr, 0, AcceptorThreadProc, this, 0, nullptr));
	if (mAcceptorThread == nullptr)
	{
		if (g_Logger != nullptr)
		{
			g_Logger->WriteLog(L"ERROR", LOG_LEVEL::ERR, L"AccepterThread start failed : %d", GetLastError());
		}
		OnError(GetLastError(), "Failed to start accept thread");
		return false;
	}

	PrintSystemMessage(L"[SYSTEM] AccepterThread running..");

	for (USHORT i = 0; i < createWorkerThreadCount; ++i)
	{
		HANDLE workerThread = reinterpret_cast<HANDLE>(_beginthreadex(nullptr, 0, WorkerThreadProc, this, 0, nullptr));
		if (workerThread == nullptr)
		{
			if (g_Logger != nullptr)
			{
				g_Logger->WriteLog(L"ERROR", LOG_LEVEL::ERR, L"WorkerThread[%d] start failed : %d", i + 1, GetLastError());
			}
			OnError(GetLastError(), "Failed to start worker thread");
			return false;
		}

		mWorkerThreads.push_back(workerThread);
		if (g_Logger != nullptr)
		{
			g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"[SYSTEM] WorkerThread[%d] running..", i + 1);
		}
	}

	PrintSystemMessage(L"[SYSTEM] IOCP echo server started");
	return true;
}

bool LanServer::SendPacket(UINT64 sessionID, SerializedBuffer* buffer)
{
	if (buffer == nullptr)
	{
		return false;
	}

	EnterCriticalSection(&mSessionMapLock);
	const auto iter = mSessions.find(sessionID);
	if (iter == mSessions.end())
	{
		LeaveCriticalSection(&mSessionMapLock);
		return false;
	}

	Session* session = iter->second;
	EnterCriticalSection(&session->mLock);
	LeaveCriticalSection(&mSessionMapLock);

	bool result = false;
	if (session->mConnected == TRUE)
	{
		if (buffer->GetDataSize() <= static_cast<int>(USHRT_MAX))
		{
			const USHORT header = static_cast<USHORT>(buffer->GetDataSize());
			if (buffer->EnqueueHeader(reinterpret_cast<const char*>(&header), sizeof(header)) == true)
			{
				result = session->SendPacket(buffer);
			}
		}
	}

	LeaveCriticalSection(&session->mLock);

	if (result == false)
	{
		Disconnect(session);
	}

	return result;
}

bool LanServer::Disconnect(Session* session)
{
	if (session == nullptr)
	{
		return false;
	}

	SOCKET closeSocket = INVALID_SOCKET;

	EnterCriticalSection(&session->mLock);
	if (session->mConnected == TRUE)
	{
		session->mConnected = FALSE;
		closeSocket = session->mSocket;
		session->mSocket = INVALID_SOCKET;
	}
	LeaveCriticalSection(&session->mLock);

	if (closeSocket != INVALID_SOCKET)
	{
		shutdown(closeSocket, SD_BOTH);
		closesocket(closeSocket);
		return true;
	}

	return false;
}

bool LanServer::ReleaseSession(Session* session)
{
	if (session == nullptr)
	{
		return false;
	}

	if (InterlockedCompareExchange(&session->mReleaseFlag, TRUE, FALSE) != FALSE)
	{
		return false;
	}

	EnterCriticalSection(&mSessionMapLock);
	auto iter = mSessions.find(session->mSessionID);
	if (iter != mSessions.end() && iter->second == session)
	{
		mSessions.erase(iter);
	}

	EnterCriticalSection(&session->mLock);
	LeaveCriticalSection(&mSessionMapLock);

	const UINT64 sessionID = session->mSessionID;
	SOCKET closeSocket = session->mSocket;
	session->mConnected = FALSE;
	session->mSocket = INVALID_SOCKET;

	LeaveCriticalSection(&session->mLock);

	if (closeSocket != INVALID_SOCKET)
	{
		shutdown(closeSocket, SD_BOTH);
		closesocket(closeSocket);
	}

	InterlockedDecrement(&mSessionCount);
	OnClientLeave(sessionID);

	session->Reset();
	return gSessionPool.Free(session);
}

int LanServer::WorkerThread()
{
	while (mWorkerRun == TRUE)
	{
		DWORD transferredSize = 0;
		ULONG_PTR completionKey = 0;
		LPOVERLAPPED overlapped = nullptr;

		const BOOL result = GetQueuedCompletionStatus(
			mIOCPHandle,
			&transferredSize,
			&completionKey,
			&overlapped,
			INFINITE);

		if (overlapped == nullptr)
		{
			if (result == FALSE)
			{
				if (g_Logger != nullptr)
				{
					g_Logger->WriteLog(L"ERROR", LOG_LEVEL::ERR, L"GetQueuedCompletionStatus failed : %d", GetLastError());
				}
				OnError(GetLastError(), "GetQueuedCompletionStatus failed");
			}
			continue;
		}

		Session* session = reinterpret_cast<Session*>(completionKey);
		OverlappedEx* overlappedEx = reinterpret_cast<OverlappedEx*>(overlapped);

		bool shouldDisconnect = false;
		if (result == FALSE || transferredSize == 0)
		{
			shouldDisconnect = true;
		}
		else if (overlappedEx->mOperation == IOOperation::RECV)
		{
			if (session->RecvCompleted(static_cast<int>(transferredSize)) == false ||
				session->PostRecv() == false)
			{
				shouldDisconnect = true;
			}
		}
		else if (overlappedEx->mOperation == IOOperation::SEND)
		{
			EnterCriticalSection(&session->mLock);
			session->SendCompleted(static_cast<int>(transferredSize));
			const bool sendOk = session->PostSend();
			LeaveCriticalSection(&session->mLock);

			if (sendOk == false)
			{
				shouldDisconnect = true;
			}
		}

		if (shouldDisconnect)
		{
			Disconnect(session);
		}

		if (InterlockedDecrement(&session->mIoCount) == 0 && session->mConnected == FALSE)
		{
			ReleaseSession(session);
		}
	}

	return 0;
}

int LanServer::AccepterThread()
{
	while (mAcceptorRun == TRUE)
	{
		SOCKADDR_IN clientAddr;
		int addrLen = sizeof(clientAddr);

		SOCKET clientSocket = accept(mListenSocket, reinterpret_cast<SOCKADDR*>(&clientAddr), &addrLen);
		if (clientSocket == INVALID_SOCKET)
		{
			if (g_Logger != nullptr)
			{
				g_Logger->WriteLog(L"ERROR", LOG_LEVEL::ERR, L"accept failed : %d", WSAGetLastError());
			}
			OnError(WSAGetLastError(), "accept failed");
			continue;
		}

		if (mSessionCount >= mMaxSessionCount)
		{
			closesocket(clientSocket);
			continue;
		}

		WCHAR clientIp[INET_ADDRSTRLEN] = {};
		InetNtopW(AF_INET, &clientAddr.sin_addr, clientIp, INET_ADDRSTRLEN);

		if (OnConnectionRequest(clientIp, ntohs(clientAddr.sin_port)) == false)
		{
			closesocket(clientSocket);
			continue;
		}

		DWORD sendBufferSize = 0;
		setsockopt(clientSocket, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<char*>(&sendBufferSize), sizeof(sendBufferSize));

		Session* session = gSessionPool.Alloc();
		if (session == nullptr)
		{
			closesocket(clientSocket);
			if (g_Logger != nullptr)
			{
				g_Logger->WriteLog(L"ERROR", LOG_LEVEL::ERR, L"Session pool allocation failed");
			}
			OnError(ERROR_OUTOFMEMORY, "Session pool allocation failed");
			continue;
		}

		const UINT64 sessionID = static_cast<UINT64>(InterlockedIncrement64(&mCurrentSessionID));
		session->Initialize(clientSocket, sessionID);

		if (CreateIoCompletionPort(reinterpret_cast<HANDLE>(clientSocket), mIOCPHandle, reinterpret_cast<ULONG_PTR>(session), 0) == nullptr)
		{
			closesocket(clientSocket);
			session->Reset();
			gSessionPool.Free(session);
			if (g_Logger != nullptr)
			{
				g_Logger->WriteLog(L"ERROR", LOG_LEVEL::ERR, L"CreateIoCompletionPort(bind) failed : %d", GetLastError());
			}
			OnError(GetLastError(), "CreateIoCompletionPort(bind) failed");
			continue;
		}

		EnterCriticalSection(&mSessionMapLock);
		mSessions.emplace(sessionID, session);
		LeaveCriticalSection(&mSessionMapLock);

		InterlockedIncrement(&mSessionCount);
		OnAccept(sessionID);

		if (session->PostRecv() == false)
		{
			Disconnect(session);
			if (session->mIoCount == 0)
			{
				ReleaseSession(session);
			}
		}
	}

	return 0;
}
