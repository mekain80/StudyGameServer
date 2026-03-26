#pragma once

#include <Windows.h>
#include <Psapi.h>

struct MonitorSnapshot
{
    LONG sessionCount = 0;
    LONG playerCount = 0;
    LONG loopCount = 0;
    LONG fps = 0;
    LONG acceptTps = 0;
    LONG recvTps = 0;
    LONG sendTps = 0;
    DWORD privateMemoryMb = 0;
    float systemCpuTotal = 0.0f;
    float systemCpuUser = 0.0f;
    float systemCpuKernel = 0.0f;
    float processCpuTotal = 0.0f;
    float processCpuUser = 0.0f;
    float processCpuKernel = 0.0f;
};

class ServerMonitor
{
public:
    void Initialize(HANDLE processHandle = INVALID_HANDLE_VALUE) noexcept;
    void Tick() noexcept;

    void OnSessionAccepted() noexcept;
    void OnSessionReleased() noexcept;
    void OnPlayerSpawned() noexcept;
    void OnPlayerReleased() noexcept;
    void OnLoop() noexcept;
    void OnFrame(LONG frameCount = 1) noexcept;
    void OnAccept() noexcept;
    void OnRecv() noexcept;
    void OnSend() noexcept;

private:
    static LONG ReadCounter(volatile LONG* counter) noexcept;
    void UpdateCpuTime() noexcept;
    void UpdateMemory() noexcept;
    void PrintConsole(const MonitorSnapshot& snapshot) const noexcept;
    void LogUnexpectedFps(const MonitorSnapshot& snapshot) const noexcept;
    void LogLowFps(const MonitorSnapshot& snapshot) const noexcept;

private:
    HANDLE m_processHandle = INVALID_HANDLE_VALUE;
    int m_processorCount = 1;
    PROCESS_MEMORY_COUNTERS_EX m_memory{};

    float m_systemCpuTotal = 0.0f;
    float m_systemCpuUser = 0.0f;
    float m_systemCpuKernel = 0.0f;
    float m_processCpuTotal = 0.0f;
    float m_processCpuUser = 0.0f;
    float m_processCpuKernel = 0.0f;

    ULARGE_INTEGER m_systemLastKernel{};
    ULARGE_INTEGER m_systemLastUser{};
    ULARGE_INTEGER m_systemLastIdle{};
    ULARGE_INTEGER m_processLastKernel{};
    ULARGE_INTEGER m_processLastUser{};
    ULARGE_INTEGER m_processLastTime{};

    volatile LONG m_sessionCount = 0;
    volatile LONG m_playerCount = 0;
    volatile LONG m_loopCount = 0;
    volatile LONG m_fps = 0;
    volatile LONG m_acceptTps = 0;
    volatile LONG m_recvTps = 0;
    volatile LONG m_sendTps = 0;
};

extern ServerMonitor gServerMonitor;
