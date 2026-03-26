#pragma once

#include "LanServer.h"

class EchoServer : public LanServer
{
public:
	bool OnConnectionRequest(const WCHAR* ip, USHORT port) override;
	void OnAccept(UINT64 sessionID) override;
	void OnClientLeave(UINT64 sessionID) override;
	void OnRecv(UINT64 sessionID, SerializedBuffer* message) override;
	void OnError(int errorCode, const char* errMsg) override;
};
