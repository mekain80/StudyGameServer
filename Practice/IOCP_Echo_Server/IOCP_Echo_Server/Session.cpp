#include "Session.h"

#include "LanServer.h"
#include "Logger.h"

Session::Session()
	: mSocket(INVALID_SOCKET)
	, mSessionID(0)
	, mRecvBuffer(SESSION_RECV_BUFFER_SIZE)
	, mSendBuffer(SESSION_SEND_BUFFER_SIZE)
	, mRecvOverlapped(IOOperation::RECV)
	, mSendOverlapped(IOOperation::SEND)
	, mIoCount(0)
	, mSendFlag(FALSE)
	, mConnected(FALSE)
	, mReleaseFlag(FALSE)
{
	InitializeCriticalSection(&mLock);
}

Session::~Session() noexcept
{
	DeleteCriticalSection(&mLock);
}

bool Session::Initialize(SOCKET socket, UINT64 sessionID) noexcept
{
	Reset();
	mSocket = socket;
	mSessionID = sessionID;
	mConnected = TRUE;
	return true;
}

void Session::Reset() noexcept
{
	mSocket = INVALID_SOCKET;
	mSessionID = 0;
	mRecvBuffer.ClearBuffer();
	mSendBuffer.ClearBuffer();
	mRecvOverlapped.Reset(IOOperation::RECV);
	mSendOverlapped.Reset(IOOperation::SEND);
	mIoCount = 0;
	mSendFlag = FALSE;
	mConnected = FALSE;
	mReleaseFlag = FALSE;
}

bool Session::RecvCompleted(int size)
{
	if (size <= 0)
	{
		return false;
	}

	if (mRecvBuffer.MoveRear(static_cast<size_t>(size)) == false)
	{
		return false;
	}

	while (mRecvBuffer.GetUseSize() >= PACKET_HEADER_SIZE)
	{
		USHORT payloadSize = 0;
		if (mRecvBuffer.Peek(reinterpret_cast<char*>(&payloadSize), PACKET_HEADER_SIZE) == false)
		{
			return false;
		}

		if (mRecvBuffer.GetUseSize() < static_cast<size_t>(PACKET_HEADER_SIZE + payloadSize))
		{
			break;
		}

		if (mRecvBuffer.MoveFront(PACKET_HEADER_SIZE) == false)
		{
			return false;
		}

		SerializedBuffer* message = SerializedBuffer::Alloc();
		if (message == nullptr)
		{
			if (g_Logger != nullptr)
			{
				g_Logger->WriteLog(L"ERROR", LOG_LEVEL::ERR, L"SerializedBuffer pool allocation failed");
			}
			gServer->OnError(ERROR_OUTOFMEMORY, "SerializedBuffer pool allocation failed");
			return false;
		}

		if (message->GetFreeSize() < payloadSize)
		{
			SerializedBuffer::Free(message);
			if (g_Logger != nullptr)
			{
				g_Logger->WriteLog(L"ERROR", LOG_LEVEL::ERR, L"Payload is larger than SerializedBuffer capacity : %hu", payloadSize);
			}
			gServer->OnError(ERROR_BUFFER_OVERFLOW, "Payload is larger than SerializedBuffer capacity");
			return false;
		}

		if (mRecvBuffer.Dequeue(message->GetContentBufferPtr(), payloadSize) == false)
		{
			SerializedBuffer::Free(message);
			return false;
		}

		message->MoveWritePos(payloadSize);
		gServer->OnRecv(mSessionID, message);
		SerializedBuffer::Free(message);
	}

	return true;
}

bool Session::SendPacket(SerializedBuffer* message)
{
	if (message == nullptr)
	{
		return false;
	}

	if (mSendBuffer.Enqueue(message->GetBufferPtr(), static_cast<size_t>(message->GetFullSize())) == false)
	{
		return false;
	}

	return PostSend();
}

void Session::SendCompleted(int size)
{
	if (size > 0)
	{
		mSendBuffer.MoveFront(static_cast<size_t>(size));
	}

	InterlockedExchange(&mSendFlag, FALSE);
}

bool Session::PostRecv()
{
	if (mConnected == FALSE || mSocket == INVALID_SOCKET)
	{
		return false;
	}

	const size_t directSize = mRecvBuffer.GetDirectEnqueueSize();
	if (directSize == 0)
	{
		return false;
	}

	WSABUF wsaBuf;
	wsaBuf.buf = mRecvBuffer.GetRear();
	wsaBuf.len = static_cast<ULONG>(directSize);

	mRecvOverlapped.Reset(IOOperation::RECV);
	InterlockedIncrement(&mIoCount);

	DWORD flag = 0;
	DWORD recvBytes = 0;
	const int retVal = WSARecv(mSocket, &wsaBuf, 1, &recvBytes, &flag, &mRecvOverlapped.mOverlapped, nullptr);
	if (retVal == SOCKET_ERROR)
	{
		const int errVal = WSAGetLastError();
		if (errVal != WSA_IO_PENDING)
		{
			if (g_Logger != nullptr)
			{
				g_Logger->WriteLog(L"ERROR", LOG_LEVEL::ERR, L"WSARecv() error : %d", errVal);
			}
			InterlockedDecrement(&mIoCount);
			return false;
		}
	}

	return true;
}

bool Session::PostSend()
{
	if (mConnected == FALSE || mSocket == INVALID_SOCKET)
	{
		return false;
	}

	if (mSendBuffer.GetUseSize() == 0)
	{
		return true;
	}

	if (InterlockedCompareExchange(&mSendFlag, TRUE, FALSE) != FALSE)
	{
		return true;
	}

	const size_t directSize = mSendBuffer.GetDirectDequeueSize();
	if (directSize == 0)
	{
		InterlockedExchange(&mSendFlag, FALSE);
		return false;
	}

	WSABUF wsaBuf;
	wsaBuf.buf = mSendBuffer.GetFront();
	wsaBuf.len = static_cast<ULONG>(directSize);

	mSendOverlapped.Reset(IOOperation::SEND);
	InterlockedIncrement(&mIoCount);

	DWORD sendBytes = 0;
	const int retVal = WSASend(mSocket, &wsaBuf, 1, &sendBytes, 0, &mSendOverlapped.mOverlapped, nullptr);
	if (retVal == SOCKET_ERROR)
	{
		const int errVal = WSAGetLastError();
		if (errVal != WSA_IO_PENDING)
		{
			if (g_Logger != nullptr)
			{
				g_Logger->WriteLog(L"ERROR", LOG_LEVEL::ERR, L"WSASend() error : %d", errVal);
			}
			InterlockedExchange(&mSendFlag, FALSE);
			InterlockedDecrement(&mIoCount);
			return false;
		}
	}

	return true;
}
