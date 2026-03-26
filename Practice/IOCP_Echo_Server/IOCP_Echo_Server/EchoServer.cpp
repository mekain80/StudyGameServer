#include "EchoServer.h"

#include "CLogger.h"
#include "SerializedBuffer.h"

bool EchoServer::OnConnectionRequest(const WCHAR* ip, USHORT port)
{
	if (g_Logger != nullptr)
	{
		g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"[CONNECT] ip=%s port=%hu", ip, port);
	}
	return true;
}

void EchoServer::OnAccept(UINT64 sessionID)
{
	if (g_Logger != nullptr)
	{
		g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"[ACCEPT] sessionID=%llu", sessionID);
	}
}

void EchoServer::OnClientLeave(UINT64 sessionID)
{
	if (g_Logger != nullptr)
	{
		g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"[LEAVE] sessionID=%llu", sessionID);
	}
}

void EchoServer::OnRecv(UINT64 sessionID, SerializedBuffer* message)
{
	if (message == nullptr)
	{
		return;
	}

	if (message->GetDataSize() < static_cast<int>(sizeof(__int64)))
	{
		OnError(ERROR_INVALID_DATA, "Received payload is smaller than __int64");
		return;
	}

	__int64 value = 0;
	*message >> value;

	if (g_Logger != nullptr)
	{
		g_Logger->WriteLogConsole(LOG_LEVEL::DEBUG, L"[RECV] sessionID=%llu value=%lld", sessionID, value);
	}

	SerializedBuffer response;
	response << value;
	SendPacket(sessionID, &response);
}

void EchoServer::OnError(int errorCode, const char* errMsg)
{
	if (errMsg == nullptr)
	{
		errMsg = "Unknown error";
	}

	if (g_Logger != nullptr)
	{
		g_Logger->WriteLog(L"ERROR", LOG_LEVEL::ERR, L"code=%d message=%hs", errorCode, errMsg);
		g_Logger->WriteLogConsole(LOG_LEVEL::ERR, L"[ERROR] code=%d message=%hs", errorCode, errMsg);
	}
}
