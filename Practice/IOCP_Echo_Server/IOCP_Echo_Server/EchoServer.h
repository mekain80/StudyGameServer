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

	EchoServer(const EchoServer&) = delete;
	EchoServer& operator=(const EchoServer&) = delete;
	EchoServer(EchoServer&&) = delete;
	EchoServer& operator=(EchoServer&&) = delete;

	EchoServer() noexcept = default;
	~EchoServer() noexcept override = default;
};
