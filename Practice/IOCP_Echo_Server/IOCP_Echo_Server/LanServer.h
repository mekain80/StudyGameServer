#pragma once

#include "Common.h"

class Session;
class SerializedBuffer;

class LanServer
{
public:
	LanServer();
	virtual ~LanServer() noexcept;

	bool Start(const char* openIP, USHORT port, USHORT createWorkerThreadCount, USHORT maxWorkerThreadCount, INT maxSessionCount);
	int GetSessionCount() const noexcept { return static_cast<int>(mSessionCount); }

	bool SendPacket(UINT64 sessionID, SerializedBuffer* buffer);
	bool Disconnect(Session* session);
	bool ReleaseSession(Session* session);

	virtual bool OnConnectionRequest(const WCHAR* ip, USHORT port) = 0;
	virtual void OnAccept(UINT64 sessionID) = 0;
	virtual void OnClientLeave(UINT64 sessionID) = 0;
	virtual void OnRecv(UINT64 sessionID, SerializedBuffer* message) = 0;
	virtual void OnError(int errorCode, const char* errMsg) = 0;

	int WorkerThread();
	int AccepterThread();

protected:
	void PrintSystemMessage(const WCHAR* message) const;

private:
	LONG mSessionCount;
	INT mMaxSessionCount;
	LONG64 mCurrentSessionID;

	SOCKET mListenSocket;
	USHORT mMaxWorkerThreadCount;
	LONG mWorkerRun;
	LONG mAcceptorRun;

	std::vector<HANDLE> mWorkerThreads;
	HANDLE mAcceptorThread;
	HANDLE mIOCPHandle;

	CRITICAL_SECTION mSessionMapLock;
	std::unordered_map<UINT64, Session*> mSessions;
};

extern LanServer* gServer;

unsigned __stdcall WorkerThreadProc(void* param);
unsigned __stdcall AcceptorThreadProc(void* param);
