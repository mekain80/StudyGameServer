#include "stdafx.h"

#include <WS2tcpip.h>

#include "Network.h"
#include "Game.h"
#include "GameInfo.h"
#include "Log.h"
#include "Monitor.h"
#include "PacketControl.h"
#include "Protocol.h"
#include "Proxy.h"
#include "Stub.h"
#include "Character.h"
#include "Sector.h"

bool gShutdown = false;
DWORD gAllocID = 1;
SOCKET gListenSocket = INVALID_SOCKET;

namespace
{
	std::vector<Session*> gNetIoSessionBatch;
	std::vector<Session*> gNetIoWritableBatch;

	void LogRecvPacketHex(Session* session, const WCHAR* reason) noexcept
	{
		if (session == nullptr)
		{
			return;
		}

		const std::size_t usedSize = session->recvQ.GetUseSize();
		const std::size_t dumpSize = (usedSize < 64) ? usedSize : 64;
		if (dumpSize == 0)
		{
			_LOG(
				LOG_LEVEL_ERROR,
				L"PacketDump # SessionID:%u / Reason:%s / UsedSize:%zu / Data:(empty)",
				session->sessionID,
				(reason != nullptr) ? reason : L"(none)",
				usedSize);
			return;
		}

		char buffer[64]{};
		if (!session->recvQ.Peek(buffer, dumpSize))
		{
			_LOG(
				LOG_LEVEL_ERROR,
				L"PacketDump # SessionID:%u / Reason:%s / UsedSize:%zu / Data:(peek fail)",
				session->sessionID,
				(reason != nullptr) ? reason : L"(none)",
				usedSize);
			return;
		}

		WCHAR hexBuffer[256]{};
		std::size_t offset = 0;
		for (std::size_t index = 0; index < dumpSize && offset + 4 < _countof(hexBuffer); ++index)
		{
			const unsigned char value = static_cast<unsigned char>(buffer[index]);
			const int written = swprintf_s(hexBuffer + offset, _countof(hexBuffer) - offset, L"%02X ", value);
			if (written <= 0)
			{
				break;
			}

			offset += static_cast<std::size_t>(written);
		}

		_LOG(
			LOG_LEVEL_ERROR,
			L"PacketDump # SessionID:%u / Reason:%s / UsedSize:%zu / DumpSize:%zu / Data:%s",
			session->sessionID,
			(reason != nullptr) ? reason : L"(none)",
			usedSize,
			dumpSize,
			hexBuffer);
	}

	bool ConfigureAcceptedSocket(SOCKET clientSocket) noexcept
	{
		u_long on = 1;
		if (ioctlsocket(clientSocket, FIONBIO, &on) == SOCKET_ERROR)
		{
			closesocket(clientSocket);
			_LOG(LOG_LEVEL_ERROR, L"clientSocket ioctlsocket fail");
			return false;
		}

		BOOL noDelay = TRUE;
		if (setsockopt(
			clientSocket,
			IPPROTO_TCP,
			TCP_NODELAY,
			reinterpret_cast<const char*>(&noDelay),
			sizeof(noDelay)) == SOCKET_ERROR)
		{
			closesocket(clientSocket);
			_LOG(LOG_LEVEL_ERROR, L"clientSocket setsockopt TCP_NODELAY fail");
			return false;
		}

		return true;
	}

	Session* CreateSession(SOCKET clientSocket, const SOCKADDR_IN& clientAddr)
	{
		Session* session = AllocSession();
		if (session == nullptr)
		{
			return nullptr;
		}

		session->socket = clientSocket;
		session->addr = clientAddr;
		session->lastRecvTime = GetTickCount64();

		InetNtopW(AF_INET, const_cast<IN_ADDR*>(&clientAddr.sin_addr), session->ipStr, _countof(session->ipStr));
		session->port = ntohs(clientAddr.sin_port);
		session->sessionID = gAllocID++;

		gSessionMap[clientSocket] = session;
		gSessionIdMap[session->sessionID] = session;
		AddActiveSession(session);
		return session;
	}

	Character* SpawnCharacter(Session* session)
	{
		Character* character = AllocCharacter();
		if (character == nullptr)
		{
			return nullptr;
		}

		const LONGLONG currentTick = GetCurrentMoveTick();
		character->sessionID = session->sessionID;
		character->session = session;
		character->y = dfRANGE_MOVE_TOP + rand() % (dfRANGE_MOVE_BOTTOM - dfRANGE_MOVE_TOP + 1);
		character->x = dfRANGE_MOVE_LEFT + rand() % (dfRANGE_MOVE_RIGHT - dfRANGE_MOVE_LEFT + 1);
		character->direction = dfPACKET_MOVE_DIR_LL;
		character->action = dfACTION_STOP;
		character->lastMoveTick = currentTick;
		character->moveTimeRemainder = 0.0;
		character->HP = MAX_HP;

		gCharacterMap[session->sessionID] = character;
		InsertSector(character);
		return character;
	}

	const SerializedBuffer* BuildCreateOtherCharacterPacket(const Character* character) noexcept
	{
		if (character == nullptr)
		{
			return nullptr;
		}

		static SerializedBuffer createPacket(dfPACKET_HEADER_SIZE + dfPACKET_SC_CREATE_OTHER_CHARACTER_SIZE);
		MakePacket_CreateOtherCharacter(
			&createPacket,
			character->direction,
			character->sessionID,
			character->x,
			character->y,
			character->HP);
		return &createPacket;
	}

	const SerializedBuffer* BuildMoveStartPacket(const Character* character) noexcept
	{
		if (character == nullptr || character->action == dfACTION_STOP)
		{
			return nullptr;
		}

		static SerializedBuffer movePacket(dfPACKET_HEADER_SIZE + dfPACKET_SC_MOVE_START_SIZE);
		MakePacket_MoveStart(
			&movePacket,
			character->sessionID,
			character->action,
			character->x,
			character->y);
		return &movePacket;
	}

	void SendMyCharacterState(Session* session, const Character* character) noexcept
	{
		static SerializedBuffer createPacket(dfPACKET_HEADER_SIZE + dfPACKET_SC_CREATE_MY_CHARACTER_SIZE);
		MakePacket_CreateMyCharacter(
			&createPacket,
			character->direction,
			character->sessionID,
			character->x,
			character->y,
			character->HP);
		SendUnicast(session, &createPacket);
	}

	void SendNearbyCharactersToNewSession(Session* session, const Character* character) noexcept
	{
		static SerializedBuffer batchPacket(SerializedBuffer::eBUFFER_DEFAULT);
		batchPacket.Clear();

		SectorAround nearbySectors{};
		GetSectorAroundBySector(&character->sector, &nearbySectors);
		for (int sectorIndex = 0; sectorIndex < nearbySectors.count; ++sectorIndex)
		{
			const SectorPos sectorPos = nearbySectors.around[sectorIndex];
			SectorCharacterList& sectorCharacters = gSector[sectorPos.y][sectorPos.x];
			for (Character* otherCharacter : sectorCharacters)
			{
				if (otherCharacter == character || FindActiveSession(otherCharacter) == nullptr)
				{
					continue;
				}

				AppendPacketBatch(session, &batchPacket, BuildCreateOtherCharacterPacket(otherCharacter));
				AppendPacketBatch(session, &batchPacket, BuildMoveStartPacket(otherCharacter));
			}
		}

		FlushPacketBatch(session, &batchPacket);
	}

	void BroadcastNewCharacter(Session* session, const Character* character) noexcept
	{
		const SerializedBuffer* createPacket = BuildCreateOtherCharacterPacket(character);
		const SerializedBuffer* movePacket = BuildMoveStartPacket(character);
		static SerializedBuffer targetSessionBatch(SerializedBuffer::eBUFFER_DEFAULT);

		SectorAround nearbySectors{};
		GetSectorAroundBySector(&character->sector, &nearbySectors);
		for (int sectorIndex = 0; sectorIndex < nearbySectors.count; ++sectorIndex)
		{
			const SectorPos sectorPos = nearbySectors.around[sectorIndex];
			SectorCharacterList& sectorCharacters = gSector[sectorPos.y][sectorPos.x];
			for (Character* otherCharacter : sectorCharacters)
			{
				if (otherCharacter == nullptr || otherCharacter == character)
				{
					continue;
				}

				Session* targetSession = FindActiveSession(otherCharacter);
				if (targetSession == nullptr || targetSession == session)
				{
					continue;
				}

				targetSessionBatch.Clear();
				AppendPacketBatch(targetSession, &targetSessionBatch, createPacket);
				AppendPacketBatch(targetSession, &targetSessionBatch, movePacket);
				FlushPacketBatch(targetSession, &targetSessionBatch);
			}
		}
	}
}

void NetStartUp() noexcept
{
	auto failStartUp = [](const WCHAR* const message) noexcept
		{
			_LOG(LOG_LEVEL_ERROR, message);

			if (gListenSocket != INVALID_SOCKET)
			{
				closesocket(gListenSocket);
				gListenSocket = INVALID_SOCKET;
			}

			WSACleanup();
			gShutdown = true;
		};

	QueryPerformanceFrequency(&gFreq);
	QueryPerformanceCounter(&gFrameStartTick);

	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		failStartUp(L"WSAStartup fail");
		return;
	}

	gListenSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (gListenSocket == INVALID_SOCKET)
	{
		failStartUp(L"socket fail");
		return;
	}

	u_long on = 1;
	if (ioctlsocket(gListenSocket, FIONBIO, &on) == SOCKET_ERROR)
	{
		failStartUp(L"ioctlsocket fail");
		return;
	}

	SOCKADDR_IN serverAddr{};
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(SERVER_PORT);
	serverAddr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);

	if (bind(gListenSocket, reinterpret_cast<SOCKADDR*>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR)
	{
		failStartUp(L"bind fail");
		return;
	}

	_LOG(LOG_LEVEL_SYSTEM, L"BIND OK # Port:%d", SERVER_PORT);

	if (listen(gListenSocket, SOMAXCONN_HINT(20000)) == SOCKET_ERROR)
	{
		failStartUp(L"listen() fail");
		return;
	}
}

void NetEnd() noexcept
{
	closesocket(gListenSocket);
	WSACleanup();
}

void NetIOProcess() noexcept
{
	size_t maxBatchSize = static_cast<size_t>(FD_SETSIZE - 1);
	size_t readOffset = 0;
	size_t writeOffset = 0;
	do
	{
		FD_SET readSet;
		FD_SET writeSet;
		FD_ZERO(&readSet);
		FD_ZERO(&writeSet);

		FD_SET(gListenSocket, &readSet);

		gNetIoSessionBatch.clear();
		if (gNetIoSessionBatch.capacity() < maxBatchSize)
		{
			gNetIoSessionBatch.reserve(maxBatchSize);
		}

		gNetIoWritableBatch.clear();
		if (gNetIoWritableBatch.capacity() < maxBatchSize)
		{
			gNetIoWritableBatch.reserve(maxBatchSize);
		}

		const size_t activeSessionCount = gActiveSessions.size();
		const size_t readBatchEnd = min(readOffset + maxBatchSize, activeSessionCount);
		for (; readOffset < readBatchEnd; ++readOffset)
		{
			Session* session = gActiveSessions[readOffset];
			if (session == nullptr || session->disconnectFlag)
			{
				continue;
			}

			if (session->socket == INVALID_SOCKET)
			{
				continue;
			}

			gNetIoSessionBatch.push_back(session);
			FD_SET(session->socket, &readSet);
		}

		const size_t writableSessionCount = gWritableSessions.size();
		const size_t writeBatchEnd = min(writeOffset + maxBatchSize, writableSessionCount);
		for (; writeOffset < writeBatchEnd; ++writeOffset)
		{
			Session* session = gWritableSessions[writeOffset];
			if (session == nullptr || session->disconnectFlag)
			{
				continue;
			}

			if (session->socket == INVALID_SOCKET)
			{
				continue;
			}

			gNetIoWritableBatch.push_back(session);
			FD_SET(session->socket, &writeSet);
		}

		timeval time{};
		int selectResult = select(0, &readSet, &writeSet, nullptr, &time);
		if (selectResult == SOCKET_ERROR)
		{
			_LOG(LOG_LEVEL_ERROR, L"Select fail");
			continue;
		}

		if (FD_ISSET(gListenSocket, &readSet))
		{
			NetProc_Accept();
		}

		for (Session* session : gNetIoSessionBatch)
		{
			if (session == nullptr || session->disconnectFlag)
			{
				continue;
			}

			if (session->socket == INVALID_SOCKET)
			{
				continue;
			}

			SOCKET socket = session->socket;
			if (FD_ISSET(socket, &readSet))
			{
				if (!NetProc_Recv(session))
				{
					continue;
				}
			}
		}

		for (Session* session : gNetIoWritableBatch)
		{
			if (session == nullptr || session->disconnectFlag)
			{
				continue;
			}

			if (session->socket == INVALID_SOCKET)
			{
				continue;
			}

			SOCKET socket = session->socket;
			if (FD_ISSET(socket, &writeSet))
			{
				if (!NetProc_Send(session))
				{
					continue;
				}
			}
		}
	} while (readOffset < gActiveSessions.size() || writeOffset < gWritableSessions.size());
}

void NetProc_Accept() noexcept
{
	constexpr int kAcceptBudget = 256; // 한 루프에 너무 오래 머물지 않도록 상한

	for (int i = 0; i < kAcceptBudget; ++i)
	{
		SOCKADDR_IN clientAddr{};
		int addrlen = sizeof(clientAddr);

		SOCKET clientSocket =
			accept(gListenSocket, reinterpret_cast<SOCKADDR*>(&clientAddr), &addrlen);

		if (clientSocket == INVALID_SOCKET)
		{
			const int err = WSAGetLastError();
			if (err == WSAEWOULDBLOCK)
				break; // 지금 처리할 신규 접속 없음

			_LOG(LOG_LEVEL_ERROR, L"clientSocket accept fail # WSA:%d", err);
			break;
		}

		if (!ConfigureAcceptedSocket(clientSocket))
		{
			// ConfigureAcceptedSocket 내부에서 실패 시 close 처리
			continue;
		}

		Session* session = CreateSession(clientSocket, clientAddr);
		if (session == nullptr)
		{
			closesocket(clientSocket);
			_LOG(LOG_LEVEL_ERROR, L"session alloc fail");
			continue;
		}

		gServerMonitor.OnSessionAccepted();

		Character* character = SpawnCharacter(session);
		if (character == nullptr)
		{
			gSessionIdMap.erase(session->sessionID);
			gSessionMap.erase(session->socket);
			RemoveActiveSession(session);
			closesocket(session->socket);
			session->socket = INVALID_SOCKET;
			gServerMonitor.OnSessionReleased();
			FreeSession(session);
			_LOG(LOG_LEVEL_ERROR, L"character alloc fail");
			continue;
		}

		gServerMonitor.OnPlayerSpawned();
		SendMyCharacterState(session, character);
		SendNearbyCharactersToNewSession(session, character);
		BroadcastNewCharacter(session, character);
		gServerMonitor.OnAccept();
	}
}

bool NetProc_Recv(Session* pSession) noexcept
{
	bool receivedAnyData = false;
	while (true)
	{
		const std::size_t directEnqueueSize = pSession->recvQ.GetDirectEnqueueSize();
		if (directEnqueueSize == 0)
		{
			if (pSession->recvQ.GetFreeSize() == 0)
			{
				_LOG(LOG_LEVEL_ERROR, L"recvQ full");
				Disconnect(pSession, L"recvQ full");
				return false;
			}
			break;
		}

		const int recvCapacity = static_cast<int>(
			(directEnqueueSize < static_cast<std::size_t>(BUFFER_SIZE))
			? directEnqueueSize
			: static_cast<std::size_t>(BUFFER_SIZE));
		int recvRet = recv(pSession->socket, pSession->recvQ.GetRear(), recvCapacity, 0);
		if (recvRet == 0)
		{
			Disconnect(pSession, L"recv 0");
			return false;
		}
		else if (recvRet == SOCKET_ERROR)
		{
			int err = WSAGetLastError();
			if (err == WSAEWOULDBLOCK)
			{
				break;
			}

			if (err == WSAECONNRESET || err == WSAECONNABORTED ||
				err == WSAENETRESET || err == WSAESHUTDOWN || err == WSAENOTCONN)
			{
				// _LOG(LOG_LEVEL_ERROR, L"WSAGetLastError # SessionID:%d    WSA NUM:%d", pSession->sessionID, err);
				Disconnect(pSession, L"recv socket error");
				return false;
			}

			Disconnect(pSession, L"recv unknown socket error");
			return false;
		}

		if (!pSession->recvQ.MoveRear(static_cast<std::size_t>(recvRet)))
		{
			_LOG(LOG_LEVEL_ERROR, L"recvQ move rear fail");
			Disconnect(pSession, L"recvQ move rear fail");
			return false;
		}

		receivedAnyData = true;
		pSession->lastRecvTime = GetTickCount64();

		if (recvRet < recvCapacity)
		{
			break;
		}
	}

	if (!receivedAnyData)
	{
		return true;
	}
	gServerMonitor.OnRecv();

	while (true)
	{
		const size_t headerSize = sizeof(PacketHeader);
		const std::size_t usedSize = pSession->recvQ.GetUseSize();
		if (usedSize < headerSize)
		{
			break;
		}

		PacketHeader packetHeader{};
		const std::size_t directDequeueSize = pSession->recvQ.GetDirectDequeueSize();
		if (directDequeueSize >= headerSize)
		{
			std::memcpy(&packetHeader, pSession->recvQ.GetFront(), headerSize);
		}
		else if (!pSession->recvQ.Peek(reinterpret_cast<char*>(&packetHeader), headerSize))
		{
			Disconnect(pSession, L"packet header peek fail");
			return false;
		}

		if (packetHeader.code != dfNETWORK_PACKET_CODE)
		{
			_LOG(
				LOG_LEVEL_ERROR,
				L"InvalidPacketCode # SessionID:%u / Code:0x%02X / Size:%u / Type:%u",
				pSession->sessionID,
				packetHeader.code,
				packetHeader.size,
				packetHeader.type);
			LogRecvPacketHex(pSession, L"invalid packet code");
			Disconnect(pSession, L"invalid packet code");
			return false;
		}

		WORD expectedSize = GetExpectedBodySize(packetHeader.type);
		if (expectedSize == 0 || packetHeader.size != expectedSize)
		{
			_LOG(
				LOG_LEVEL_ERROR,
				L"InvalidPacketSizeType # SessionID:%u / Code:0x%02X / Size:%u / Type:%u / ExpectedSize:%u",
				pSession->sessionID,
				packetHeader.code,
				packetHeader.size,
				packetHeader.type,
				expectedSize);
			LogRecvPacketHex(pSession, L"invalid packet size/type");
			Disconnect(pSession, L"invalid packet size/type");
			return false;
		}

		const std::size_t totalSize = headerSize + packetHeader.size;
		if (usedSize < totalSize)
		{
			return true;
		}

		if (packetHeader.size > BUFFER_SIZE)
		{
			Disconnect(pSession, L"packet body too large");
			return false;
		}

		if (directDequeueSize >= totalSize)
		{
			const char* packetData = pSession->recvQ.GetFront() + headerSize;
			if (!PacketProc(pSession, packetHeader.type, packetData, packetHeader.size))
			{
				return false;
			}

			if (!pSession->recvQ.MoveFront(totalSize))
			{
				Disconnect(pSession, L"packet move front fail");
				return false;
			}
			continue;
		}

		if (!pSession->recvQ.MoveFront(headerSize))
		{
			Disconnect(pSession, L"packet header move front fail");
			return false;
		}

		char packetBuffer[BUFFER_SIZE]{};
		if (!pSession->recvQ.Dequeue(packetBuffer, packetHeader.size))
		{
			Disconnect(pSession, L"packet body dequeue fail");
			return false;
		}

		if (!PacketProc(pSession, packetHeader.type, packetBuffer, packetHeader.size))
		{
			return false;
		}
	}

	return true;
}

bool NetProc_Send(Session* pSession) noexcept
{
	bool sentAnyData = false;
	while (true)
	{
		const std::size_t useSize = pSession->sendQ.GetUseSize();
		if (useSize <= 0)
		{
			break;
		}

		const std::size_t directDequeueSize = pSession->sendQ.GetDirectDequeueSize();
		if (directDequeueSize == 0)
		{
			Disconnect(pSession, L"sendQ direct dequeue fail");
			return false;
		}

		int sendSize = static_cast<int>(directDequeueSize);
		if (sendSize > BUFFER_SIZE)
		{
			sendSize = BUFFER_SIZE;
		}

		int sendRet = send(pSession->socket, pSession->sendQ.GetFront(), sendSize, 0);
		if (sendRet == 0)
		{
			Disconnect(pSession, L"send 0");
			return false;
		}
		else if (sendRet == SOCKET_ERROR)
		{
			int err = WSAGetLastError();
			if (err == WSAEWOULDBLOCK)
			{
				if (sentAnyData)
				{
					gServerMonitor.OnSend();
				}
				return true;
			}

			if (err == WSAECONNRESET || err == WSAECONNABORTED ||
				err == WSAENETRESET || err == WSAESHUTDOWN || err == WSAENOTCONN)
			{
				Disconnect(pSession, L"send socket error");
				return false;
			}

			Disconnect(pSession, L"send unknown socket error");
			return false;
		}

		if (!pSession->sendQ.MoveFront(static_cast<std::size_t>(sendRet)))
		{
			Disconnect(pSession, L"sendQ move front fail");
			return false;
		}

		sentAnyData = true;
	}

	if (sentAnyData)
	{
		gServerMonitor.OnSend();
	}

	RemoveWritableSession(pSession);

	return true;
}
