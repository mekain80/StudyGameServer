#include "Common.h"

#include "CrashDump.h"
#include "Logger.h"
#include "EchoServer.h"

int main()
{
	CrashDump crashDump;

	g_Logger = Logger::GetInstance();
	g_Logger->SetDirectory(L"LogFile");
	g_Logger->SetLogLevel(LOG_LEVEL::DEBUG);

	EchoServer server;
	gServer = &server;

	if (server.Start(
		SERVER_IP,
		SERVER_PORT,
		WORKER_THREAD_CREATE_COUNT,
		WORKER_THREAD_MAX_COUNT,
		MAX_SESSION_COUNT) == false)
	{
		g_Logger->WriteLog(L"ERROR", LOG_LEVEL::ERR, L"server.Start() failed");
		return 1;
	}

	g_Logger->WriteLogConsole(LOG_LEVEL::SYSTEM, L"[SYSTEM] IOCP_Echo_Server running..");

	while (true)
	{
		Sleep(1000);
	}

	return 0;
}
