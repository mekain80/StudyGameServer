#include "stdafx.h"

#include "Monitor.h"

#include "Log.h"

namespace
{
    constexpr LONG kExpectedServerFps = 50;
    constexpr LONG kLowFpsThreshold = 5;
}

ServerMonitor gServerMonitor;

void ServerMonitor::Initialize(HANDLE processHandle) noexcept
{
    m_processHandle = (processHandle == INVALID_HANDLE_VALUE) ? GetCurrentProcess() : processHandle;

    SYSTEM_INFO systemInfo{};
    GetSystemInfo(&systemInfo);
    m_processorCount = (systemInfo.dwNumberOfProcessors == 0) ? 1 : static_cast<int>(systemInfo.dwNumberOfProcessors);

    m_systemLastKernel.QuadPart = 0;
    m_systemLastUser.QuadPart = 0;
    m_systemLastIdle.QuadPart = 0;
    m_processLastKernel.QuadPart = 0;
    m_processLastUser.QuadPart = 0;
    m_processLastTime.QuadPart = 0;
    ZeroMemory(&m_memory, sizeof(m_memory));

    UpdateCpuTime();
    UpdateMemory();
}

LONG ServerMonitor::ReadCounter(volatile LONG* counter) noexcept
{
    return InterlockedCompareExchange(counter, 0, 0);
}

void ServerMonitor::Tick() noexcept
{
    if (m_processHandle == INVALID_HANDLE_VALUE)
    {
        Initialize();
    }

    UpdateCpuTime();
    UpdateMemory();

    MonitorSnapshot snapshot{};
    snapshot.sessionCount = ReadCounter(&m_sessionCount);
    snapshot.playerCount = ReadCounter(&m_playerCount);
    snapshot.loopCount = InterlockedExchange(&m_loopCount, 0);
    snapshot.fps = InterlockedExchange(&m_fps, 0);
    snapshot.acceptTps = InterlockedExchange(&m_acceptTps, 0);
    snapshot.recvTps = InterlockedExchange(&m_recvTps, 0);
    snapshot.recvPacketTps = InterlockedExchange(&m_recvPacketTps, 0);
    snapshot.sendTps = InterlockedExchange(&m_sendTps, 0);
    snapshot.sendPacketTps = InterlockedExchange(&m_sendPacketTps, 0);
    snapshot.privateMemoryMb = static_cast<DWORD>(m_memory.PrivateUsage / (1024 * 1024));
    snapshot.systemCpuTotal = m_systemCpuTotal;
    snapshot.systemCpuUser = m_systemCpuUser;
    snapshot.systemCpuKernel = m_systemCpuKernel;
    snapshot.processCpuTotal = m_processCpuTotal;
    snapshot.processCpuUser = m_processCpuUser;
    snapshot.processCpuKernel = m_processCpuKernel;

    PrintConsole(snapshot);

    if (snapshot.fps != kExpectedServerFps)
    {
        LogUnexpectedFps(snapshot);
    }

    if (snapshot.fps < kLowFpsThreshold)
    {
        LogLowFps(snapshot);
    }
}

void ServerMonitor::OnSessionAccepted() noexcept
{
    InterlockedIncrement(&m_sessionCount);
}

void ServerMonitor::OnSessionReleased() noexcept
{
    InterlockedDecrement(&m_sessionCount);
}

void ServerMonitor::OnPlayerSpawned() noexcept
{
    InterlockedIncrement(&m_playerCount);
}

void ServerMonitor::OnPlayerReleased() noexcept
{
    InterlockedDecrement(&m_playerCount);
}

void ServerMonitor::OnLoop() noexcept
{
    InterlockedIncrement(&m_loopCount);
}

void ServerMonitor::OnFrame(LONG frameCount) noexcept
{
    if (frameCount <= 0)
    {
        return;
    }

    InterlockedExchangeAdd(&m_fps, frameCount);
}

void ServerMonitor::OnAccept() noexcept
{
    InterlockedIncrement(&m_acceptTps);
}

void ServerMonitor::OnRecv() noexcept
{
    InterlockedIncrement(&m_recvTps);
}

void ServerMonitor::OnRecvPacket(LONG packetCount) noexcept
{
    if (packetCount <= 0)
    {
        return;
    }

    InterlockedExchangeAdd(&m_recvPacketTps, packetCount);
}

void ServerMonitor::OnSend() noexcept
{
    InterlockedIncrement(&m_sendTps);
}

void ServerMonitor::OnSendPacket(LONG packetCount) noexcept
{
    if (packetCount <= 0)
    {
        return;
    }

    InterlockedExchangeAdd(&m_sendPacketTps, packetCount);
}

void ServerMonitor::UpdateCpuTime() noexcept
{
    ULARGE_INTEGER idle{};
    ULARGE_INTEGER kernel{};
    ULARGE_INTEGER user{};

    if (!GetSystemTimes(reinterpret_cast<PFILETIME>(&idle), reinterpret_cast<PFILETIME>(&kernel), reinterpret_cast<PFILETIME>(&user)))
    {
        return;
    }

    if (m_systemLastKernel.QuadPart != 0 || m_systemLastUser.QuadPart != 0 || m_systemLastIdle.QuadPart != 0)
    {
        const ULONGLONG kernelDiff = kernel.QuadPart - m_systemLastKernel.QuadPart;
        const ULONGLONG userDiff = user.QuadPart - m_systemLastUser.QuadPart;
        const ULONGLONG idleDiff = idle.QuadPart - m_systemLastIdle.QuadPart;
        const ULONGLONG total = kernelDiff + userDiff;

        if (total > 0)
        {
            m_systemCpuTotal = static_cast<float>((static_cast<double>(total - idleDiff) / static_cast<double>(total)) * 100.0);
            m_systemCpuUser = static_cast<float>((static_cast<double>(userDiff) / static_cast<double>(total)) * 100.0);
            m_systemCpuKernel = static_cast<float>((static_cast<double>(kernelDiff - idleDiff) / static_cast<double>(total)) * 100.0);
        }
    }

    m_systemLastKernel = kernel;
    m_systemLastUser = user;
    m_systemLastIdle = idle;

    ULARGE_INTEGER processTime{};
    GetSystemTimeAsFileTime(reinterpret_cast<LPFILETIME>(&processTime));

    ULARGE_INTEGER processKernel{};
    ULARGE_INTEGER processUser{};
    ULARGE_INTEGER unused{};
    if (!GetProcessTimes(
        m_processHandle,
        reinterpret_cast<LPFILETIME>(&unused),
        reinterpret_cast<LPFILETIME>(&unused),
        reinterpret_cast<LPFILETIME>(&processKernel),
        reinterpret_cast<LPFILETIME>(&processUser)))
    {
        return;
    }

    if (m_processLastTime.QuadPart != 0)
    {
        const ULONGLONG timeDiff = processTime.QuadPart - m_processLastTime.QuadPart;
        const ULONGLONG userDiff = processUser.QuadPart - m_processLastUser.QuadPart;
        const ULONGLONG kernelDiff = processKernel.QuadPart - m_processLastKernel.QuadPart;
        const ULONGLONG total = kernelDiff + userDiff;

        if (timeDiff > 0)
        {
            const double divisor = static_cast<double>(m_processorCount) * static_cast<double>(timeDiff);
            m_processCpuTotal = static_cast<float>((static_cast<double>(total) / divisor) * 100.0);
            m_processCpuUser = static_cast<float>((static_cast<double>(userDiff) / divisor) * 100.0);
            m_processCpuKernel = static_cast<float>((static_cast<double>(kernelDiff) / divisor) * 100.0);
        }
    }

    m_processLastTime = processTime;
    m_processLastKernel = processKernel;
    m_processLastUser = processUser;
}

void ServerMonitor::UpdateMemory() noexcept
{
    GetProcessMemoryInfo(
        m_processHandle,
        reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&m_memory),
        sizeof(m_memory));
}

void ServerMonitor::PrintConsole(const MonitorSnapshot& snapshot) const noexcept
{
    _LOG_CONSOLE(
        LOG_LEVEL_SYSTEM,
        L"[Monitor] Session:%ld Player:%ld FPS:%ld Loop:%ld AcceptTPS:%ld RecvTPS(IO/Packet):%ld/%ld SendTPS(IO/Packet):%ld/%ld",
        snapshot.sessionCount,
        snapshot.playerCount,
        snapshot.fps,
        snapshot.loopCount,
        snapshot.acceptTps,
        snapshot.recvTps,
        snapshot.recvPacketTps,
        snapshot.sendTps,
        snapshot.sendPacketTps);
    _LOG_CONSOLE(
        LOG_LEVEL_SYSTEM,
        L"[Monitor] CPU(System T/U/K): %.2f / %.2f / %.2f",
        snapshot.systemCpuTotal,
        snapshot.systemCpuUser,
        snapshot.systemCpuKernel);
    _LOG_CONSOLE(
        LOG_LEVEL_SYSTEM,
        L"[Monitor] CPU(Process T/U/K): %.2f / %.2f / %.2f",
        snapshot.processCpuTotal,
        snapshot.processCpuUser,
        snapshot.processCpuKernel);
    _LOG_CONSOLE(
        LOG_LEVEL_SYSTEM,
        L"[Monitor] Memory(Private): %lu MB",
        static_cast<unsigned long>(snapshot.privateMemoryMb));
}

void ServerMonitor::LogUnexpectedFps(const MonitorSnapshot& snapshot) const noexcept
{
    _LOG_TYPE(
        L"ServerFPS",
        LOG_LEVEL_SYSTEM,
        L"FPS:%ld Loop:%ld Session:%ld Player:%ld AcceptTPS:%ld RecvTPS(IO/Packet):%ld/%ld SendTPS(IO/Packet):%ld/%ld",
        snapshot.fps,
        snapshot.loopCount,
        snapshot.sessionCount,
        snapshot.playerCount,
        snapshot.acceptTps,
        snapshot.recvTps,
        snapshot.recvPacketTps,
        snapshot.sendTps,
        snapshot.sendPacketTps);
}

void ServerMonitor::LogLowFps(const MonitorSnapshot& snapshot) const noexcept
{
    _LOG_TYPE(L"LowerFPS", LOG_LEVEL_SYSTEM, L"----------------------------------------");
    _LOG_TYPE(
        L"LowerFPS",
        LOG_LEVEL_SYSTEM,
        L"Session:%ld Player:%ld FPS:%ld Loop:%ld AcceptTPS:%ld RecvTPS(IO/Packet):%ld/%ld SendTPS(IO/Packet):%ld/%ld",
        snapshot.sessionCount,
        snapshot.playerCount,
        snapshot.fps,
        snapshot.loopCount,
        snapshot.acceptTps,
        snapshot.recvTps,
        snapshot.recvPacketTps,
        snapshot.sendTps,
        snapshot.sendPacketTps);
    _LOG_TYPE(
        L"LowerFPS",
        LOG_LEVEL_SYSTEM,
        L"System CPU T/U/K: %.2f / %.2f / %.2f",
        snapshot.systemCpuTotal,
        snapshot.systemCpuUser,
        snapshot.systemCpuKernel);
    _LOG_TYPE(
        L"LowerFPS",
        LOG_LEVEL_SYSTEM,
        L"Process CPU T/U/K: %.2f / %.2f / %.2f",
        snapshot.processCpuTotal,
        snapshot.processCpuUser,
        snapshot.processCpuKernel);
    _LOG_TYPE(
        L"LowerFPS",
        LOG_LEVEL_SYSTEM,
        L"Private Memory: %lu MB",
        static_cast<unsigned long>(snapshot.privateMemoryMb));
}
