#pragma once

#include "Common.h"
#include "DefineServer.h"
#include "SerializedBuffer.h"

class Session
{
public:
	Session();
	~Session() noexcept;

	bool Initialize(SOCKET socket, UINT64 sessionID) noexcept;
	void Reset() noexcept;

	bool RecvCompleted(int size);
	bool SendPacket(SerializedBuffer* message);
	void SendCompleted(int size);

	bool PostRecv();
	bool PostSend();

private:
	friend class LanServer;

	SOCKET mSocket;
	UINT64 mSessionID;

	RingBuffer mRecvBuffer;
	RingBuffer mSendBuffer;

	OverlappedEx mRecvOverlapped;
	OverlappedEx mSendOverlapped;

	LONG mIoCount;
	LONG mSendFlag;
	LONG mConnected;
	LONG mReleaseFlag;

	CRITICAL_SECTION mLock;
};
