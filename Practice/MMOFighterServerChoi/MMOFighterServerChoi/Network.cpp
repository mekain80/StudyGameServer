#include "stdafx.h"

#include <WS2tcpip.h>

#include "Network.h"
#include "Game.h"
#include "GameInfo.h"
#include "Log.h"
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
	std::vector<Session*> gNetIoSessions;
	std::vector<Session*> gNetIoSessionBatch;

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

	void SendVisibleCharacterState(Session* session, const Character* character) noexcept
	{
		SerializedBuffer createPacket(dfPACKET_HEADER_SIZE + dfPACKET_SC_CREATE_OTHER_CHARACTER_SIZE);
		MakePacket_CreateOtherCharacter(
			&createPacket,
			character->direction,
			character->sessionID,
			character->x,
			character->y,
			character->HP);
		SendUnicast(session, &createPacket);

		if (character->action == dfACTION_STOP)
		{
			return;
		}

		SerializedBuffer movePacket(dfPACKET_HEADER_SIZE + dfPACKET_SC_MOVE_START_SIZE);
		MakePacket_MoveStart(
			&movePacket,
			character->sessionID,
			character->action,
			character->x,
			character->y);
		SendUnicast(session, &movePacket);
	}

	void SendMyCharacterState(Session* session, const Character* character) noexcept
	{
		SerializedBuffer createPacket(dfPACKET_HEADER_SIZE + dfPACKET_SC_CREATE_MY_CHARACTER_SIZE);
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
		SectorAround nearbySectors{};
		GetSectorAroundBySector(&character->sector, &nearbySectors);
		for (int sectorIndex = 0; sectorIndex < nearbySectors.count; ++sectorIndex)
		{
			const SectorPos sectorPos = nearbySectors.around[sectorIndex];
			std::list<Character*>& sectorCharacters = gSector[sectorPos.y][sectorPos.x];
			for (Character* otherCharacter : sectorCharacters)
			{
				if (otherCharacter == character || FindActiveSession(otherCharacter) == nullptr)
				{
					continue;
				}

				SendVisibleCharacterState(session, otherCharacter);
			}
		}
	}

	void BroadcastNewCharacter(Session* session, const Character* character) noexcept
	{
		SerializedBuffer createPacket(dfPACKET_HEADER_SIZE + dfPACKET_SC_CREATE_OTHER_CHARACTER_SIZE);
		MakePacket_CreateOtherCharacter(
			&createPacket,
			character->direction,
			character->sessionID,
			character->x,
			character->y,
			character->HP);
		SendPacket_Around(session, &createPacket);

		if (character->action == dfACTION_STOP)
		{
			return;
		}

		SerializedBuffer movePacket(dfPACKET_HEADER_SIZE + dfPACKET_SC_MOVE_START_SIZE);
		MakePacket_MoveStart(
			&movePacket,
			character->sessionID,
			character->action,
			character->x,
			character->y);
		SendPacket_Around(session, &movePacket);
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
	gNetIoSessions.clear();
	if (gNetIoSessions.capacity() < gSessionMap.size())
	{
		gNetIoSessions.reserve(gSessionMap.size());
	}

	for (const auto& sessionPair : gSessionMap)
	{
		Session* session = sessionPair.second;
		if (session == nullptr || session->disconnectFlag)
		{
			continue;
		}

		if (session->socket == INVALID_SOCKET)
		{
			continue;
		}

		gNetIoSessions.push_back(session);
	}

	size_t maxBatchSize = static_cast<size_t>(FD_SETSIZE - 1);
	size_t offset = 0;
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

		const size_t batchEnd = min(offset + maxBatchSize, gNetIoSessions.size());
		for (; offset < batchEnd; ++offset)
		{
			Session* session = gNetIoSessions[offset];

			gNetIoSessionBatch.push_back(session);
			FD_SET(session->socket, &readSet);
			if (session->sendQ.GetUseSize() > 0)
			{
				FD_SET(session->socket, &writeSet);
			}
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

			if (FD_ISSET(socket, &writeSet))
			{
				if (!NetProc_Send(session))
				{
					continue;
				}
			}
		}
	} while (offset < gNetIoSessions.size());
}

void NetProc_Accept() noexcept
{
	SOCKADDR_IN clientAddr{};
	int addrlen = sizeof(clientAddr);

	SOCKET clientSocket = accept(gListenSocket, reinterpret_cast<SOCKADDR*>(&clientAddr), &addrlen);
	if (clientSocket == INVALID_SOCKET)
	{
		_LOG(LOG_LEVEL_ERROR, L"clientSocket accept fail");
		return;
	}

	if (!ConfigureAcceptedSocket(clientSocket))
	{
		return;
	}

	Session* session = CreateSession(clientSocket, clientAddr);
	if (session == nullptr)
	{
		closesocket(clientSocket);
		_LOG(LOG_LEVEL_ERROR, L"session alloc fail");
		return;
	}

	Character* character = SpawnCharacter(session);
	if (character == nullptr)
	{
		gSessionIdMap.erase(session->sessionID);
		gSessionMap.erase(session->socket);
		closesocket(session->socket);
		session->socket = INVALID_SOCKET;
		FreeSession(session);
		_LOG(LOG_LEVEL_ERROR, L"character alloc fail");
		return;
	}

	SendMyCharacterState(session, character);
	SendNearbyCharactersToNewSession(session, character);
	BroadcastNewCharacter(session, character);
}

bool NetProc_Recv(Session* pSession) noexcept
{
	char buffer[BUFFER_SIZE] = { 0 };

	int recvRet = recv(pSession->socket, buffer, sizeof(buffer), 0);
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
			return true;
		}

		if (err == WSAECONNRESET || err == WSAECONNABORTED ||
			err == WSAENETRESET || err == WSAESHUTDOWN || err == WSAENOTCONN)
		{
			_LOG(LOG_LEVEL_ERROR, L"WSAGetLastError # SessionID:%d    WSA NUM:%d", pSession->sessionID, err);
			Disconnect(pSession, L"recv socket error");
			return false;
		}

		Disconnect(pSession, L"recv unknown socket error");
		return false;
	}

	if (!pSession->recvQ.Enqueue(buffer, recvRet))
	{
		_LOG(LOG_LEVEL_ERROR, L"recvQ enqueue fail");
		Disconnect(pSession, L"recvQ enqueue fail");
		return false;
	}

	pSession->lastRecvTime = GetTickCount64();

	while (true)
	{
		const size_t headerSize = sizeof(PacketHeader);
		if (pSession->recvQ.GetUseSize() < headerSize)
		{
			break;
		}

		PacketHeader packetHeader;
		if (!pSession->recvQ.Peek(reinterpret_cast<char*>(&packetHeader), static_cast<int>(headerSize)))
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

		int totalSize = static_cast<int>(headerSize) + packetHeader.size;
		if (pSession->recvQ.GetUseSize() < totalSize)
		{
			return true;
		}

		pSession->recvQ.MoveFront(static_cast<int>(headerSize));

		if (packetHeader.size > BUFFER_SIZE)
		{
			Disconnect(pSession, L"packet body too large");
			return false;
		}

		char packetBuffer[BUFFER_SIZE];
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
	char buffer[BUFFER_SIZE];

	while (true)
	{
		int useSize = static_cast<int>(pSession->sendQ.GetUseSize());
		if (useSize <= 0)
		{
			break;
		}

		int sendSize = useSize;
		if (sendSize > BUFFER_SIZE)
		{
			sendSize = BUFFER_SIZE;
		}

		if (!pSession->sendQ.Peek(buffer, sendSize))
		{
			Disconnect(pSession, L"sendQ peek fail");
			return false;
		}

		int sendRet = send(pSession->socket, buffer, sendSize, 0);
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

		pSession->sendQ.MoveFront(sendRet);
	}

	return true;
}
