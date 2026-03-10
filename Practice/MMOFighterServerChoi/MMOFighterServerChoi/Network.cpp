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

bool gShutdown = false;
DWORD gAllocID = 1;
SOCKET gListenSocket = INVALID_SOCKET;

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
	_LOG(LOG_LEVEL_SYSTEM, L"WSAStartup #");

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

	_LOG(LOG_LEVEL_SYSTEM, L"Listen OK #");
}

void NetEnd() noexcept
{
	closesocket(gListenSocket);
	WSACleanup();
}

void NetIOProcess() noexcept
{
	FD_SET readSet;
	FD_SET writeSet;

	int counter = 0;
	auto beginIter = gSessionMap.begin();
	auto endIter = gSessionMap.begin();
	do
	{
		FD_ZERO(&readSet);
		FD_ZERO(&writeSet);

		FD_SET(gListenSocket, &readSet);
		for (auto it = endIter; it != gSessionMap.end(); ++it)
		{
			if (it == endIter)
			{
				beginIter = it;
			}

			Session* session = it->second;
			FD_SET(session->socket, &readSet);
			if (session->sendQ.GetUseSize() > 0)
			{
				FD_SET(session->socket, &writeSet);
			}

			endIter = it;
			counter++;
			if (counter % 64 == 0)
			{
				break;
			}
		}

		timeval time{};

		int selectResult = select(0, &readSet, &writeSet, nullptr, &time);
		if (selectResult == SOCKET_ERROR)
		{
			_LOG(LOG_LEVEL_ERROR, L"Select fail");
		}

		if (FD_ISSET(gListenSocket, &readSet))
		{
			NetProc_Accept();
		}

		for (auto it = beginIter; it != endIter; it++)
		{
			Session* session = it->second;
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
	} while (endIter != gSessionMap.end());
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

	u_long on = 1;
	if (ioctlsocket(clientSocket, FIONBIO, &on) == SOCKET_ERROR)
	{
		closesocket(clientSocket);
		_LOG(LOG_LEVEL_ERROR, L"clientSocket ioctlsocket fail");
		return;
	}

	Session* pSession = new Session;
	pSession->socket = clientSocket;
	pSession->addr = clientAddr;

	InetNtopW(AF_INET, &clientAddr.sin_addr, pSession->ipStr, _countof(pSession->ipStr));
	pSession->port = ntohs(clientAddr.sin_port);

	pSession->sessionID = gAllocID++;
	gSessionMap[clientSocket] = pSession;

	Character* pCharacter = new Character;
	pCharacter->session = pSession;
	pCharacter->sessionID = pSession->sessionID;
	pCharacter->y = static_cast<short>(dfRANGE_MOVE_TOP + rand() % (dfRANGE_MOVE_BOTTOM - dfRANGE_MOVE_TOP + 1));
	pCharacter->x = static_cast<short>(dfRANGE_MOVE_LEFT + rand() % (dfRANGE_MOVE_RIGHT - dfRANGE_MOVE_LEFT + 1));
	pCharacter->direction = dfPACKET_MOVE_DIR_LL;
	pCharacter->action = dfACTION_STOP;
	pCharacter->HP = MAX_HP;
	gCharacterMap[pSession->sessionID] = pCharacter;

	PacketHeader packetHeader;
	PacketSCCreateMyCharacter createMyCharacter;
	MakePacket_CreateMyCharacter(&packetHeader, &createMyCharacter, pCharacter->direction, pCharacter->sessionID, pCharacter->x, pCharacter->y, pCharacter->HP);

	wprintf(L"[Server] CreateMyCharacter header : code=%u size=%u type=%u\n", packetHeader.code, packetHeader.size, packetHeader.type);
	wprintf(
		L"[Server] CreateMyCharacter body   : ID=%u Dir=%u X=%d Y=%d HP=%d\n",
		createMyCharacter.ID,
		createMyCharacter.direction,
		createMyCharacter.x,
		createMyCharacter.y,
		createMyCharacter.HP);

	SendUnicast(pSession, &packetHeader, reinterpret_cast<char*>(&createMyCharacter));

	for (auto& other : gSessionMap)
	{
		Session* otherSession = other.second;
		if (otherSession == pSession)
		{
			continue;
		}
		
		PacketHeader otherHeader;
		PacketSCCreateOtherCharacter otherPacket;
		Character* pCharacter = FindCharacter(otherSession->sessionID); // nullptr이 없다고 가정
		MakePacket_CreateOtherCharacter(&otherHeader, &otherPacket, pCharacter->direction, pCharacter->sessionID, pCharacter->x, pCharacter->y, pCharacter->HP);
		SendUnicast(pSession, &otherHeader, reinterpret_cast<char*>(&otherPacket));
	}

	PacketHeader broadHeader;
	PacketSCCreateOtherCharacter broadPacket;
	MakePacket_CreateOtherCharacter(&broadHeader, &broadPacket, pCharacter->direction, pCharacter->sessionID, pCharacter->x, pCharacter->y, pCharacter->HP);
	SendBroadcast(pSession, &broadHeader, reinterpret_cast<char*>(&broadPacket));

	_LOG(LOG_LEVEL_SYSTEM, L"Connect # IP:%s / SessionID:%d", pSession->ipStr, pSession->sessionID);
	_LOG(LOG_LEVEL_DEBUG, L"# PACKET_CONNECT # SessionID:%d", pSession->sessionID);
	_LOG(LOG_LEVEL_DEBUG, L"Create Character # SessionID:%d    X:%d    Y:%d", pCharacter->sessionID, pCharacter->x, pCharacter->y);
}

bool NetProc_Recv(Session* pSession) noexcept
{
	char buffer[BUFFER_SIZE] = { 0 };

	int recvRet = recv(pSession->socket, buffer, sizeof(buffer), 0);
	if (recvRet == 0)
	{
		Disconnect(pSession);
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
			Disconnect(pSession);
			return false;
		}

		Disconnect(pSession);
		return false;
	}

	if (!pSession->recvQ.Enqueue(buffer, recvRet))
	{
		_LOG(LOG_LEVEL_ERROR, L"recvQ enqueue fail");
		Disconnect(pSession);
		return false;
	}

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
			Disconnect(pSession);
			return false;
		}

		if (packetHeader.code != dfNETWORK_PACKET_CODE)
		{
			Disconnect(pSession);
			return false;
		}

		WORD expectedSize = GetExpectedBodySize(packetHeader.type);
		if (expectedSize == 0 || packetHeader.size != expectedSize)
		{
			Disconnect(pSession);
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
			Disconnect(pSession);
			return false;
		}

		char packetBuffer[BUFFER_SIZE];
		if (!pSession->recvQ.Dequeue(packetBuffer, packetHeader.size))
		{
			Disconnect(pSession);
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
			Disconnect(pSession);
			return false;
		}

		int sendRet = send(pSession->socket, buffer, sendSize, 0);
		if (sendRet == 0)
		{
			Disconnect(pSession);
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
				Disconnect(pSession);
				return false;
			}

			Disconnect(pSession);
			return false;
		}

		pSession->sendQ.MoveFront(sendRet);
	}

	return true;
}
