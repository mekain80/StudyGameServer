#pragma once

#include <cassert>
#include <windows.h>

#include "LFDefine.h"
#include "LFMemoryPool.h"

struct DebugInfo
{
    LONG64 Index;
    DWORD ThreadId;
    USHORT PushOrPop;
    USHORT Data;
    void* TopPtr;
    void* NewTopPtr;
};

constexpr int LOG_MAX = 200000;

extern DebugInfo logging[LOG_MAX];
extern LONG64 logIndex;

template<typename TData>
struct StackNode
{
    TData Data;
    ULONG_PTR Next = 0;
};

template<typename T, bool UseMemoryPool = TRUE>
class LFStack
{
};

template<typename T>
class LFStack<T, TRUE>
{
public:
    using Node = StackNode<T>;

    LFStack() = default;
    LFStack(const LFStack&) = delete;
    LFStack& operator=(const LFStack&) = delete;
    LFStack(LFStack&&) = delete;
    LFStack& operator=(LFStack&&) = delete;

    void Push(T data) noexcept;
    void Pop(T* outData) noexcept;

    LONG64 GetTop() const noexcept;
    LONG GetUseCount() const noexcept;
    bool IsEmpty() const noexcept;

private:
    ULONG_PTR nextIdentifier();

private:
    LONG64 mTop = 0;
    LONG64 mCurrentIdentifier = 0;
    LONG mUseCount = 0;
    LFMemoryPool<Node> mStackNodePool{ 1000, false };
};

template<typename T>
class LFStack<T, FALSE>
{
public:
    using Node = StackNode<T>;

    LFStack() = default;
    LFStack(const LFStack&) = delete;
    LFStack& operator=(const LFStack&) = delete;
    LFStack(LFStack&&) = delete;
    LFStack& operator=(LFStack&&) = delete;

    void Push(T data) noexcept;
    void Pop(T* outData);

    LONG64 GetTop() const noexcept;
    LONG GetUseCount() const noexcept;
    bool IsEmpty() const noexcept;

private:
    ULONG_PTR nextIdentifier();

private:
    LONG64 mTop = 0;
    LONG64 mCurrentIdentifier = 0;
    LONG mUseCount = 0;
};

template<typename T>
inline void LFStack<T, TRUE>::Push(T data) noexcept
{
    const ULONG_PTR ident = nextIdentifier();
    Node* newTop = mStackNodePool.Alloc();
    const ULONG_PTR combinedNewTop = CombineIdentAndAddr(ident, reinterpret_cast<ULONG_PTR>(newTop));
    ULONG_PTR readTop = 0;

    newTop->Data = data;

    do
    {
        readTop = static_cast<ULONG_PTR>(mTop);
        newTop->Next = readTop;
    } while (InterlockedCompareExchange64(
        &mTop,
        static_cast<LONG64>(combinedNewTop),
        static_cast<LONG64>(readTop)) != static_cast<LONG64>(readTop));

    InterlockedIncrement(&mUseCount);
}

template<typename T>
inline void LFStack<T, TRUE>::Pop(T* outData) noexcept
{
    assert(outData != nullptr);

    ULONG_PTR readTop = 0;
    ULONG_PTR newTop = 0;
    Node* readTopAddr = nullptr;

    do
    {
        readTop = static_cast<ULONG_PTR>(mTop);
        readTopAddr = reinterpret_cast<Node*>(GetAddress(readTop));
        newTop = readTopAddr->Next;
    } while (InterlockedCompareExchange64(
        &mTop,
        static_cast<LONG64>(newTop),
        static_cast<LONG64>(readTop)) != static_cast<LONG64>(readTop));

    InterlockedDecrement(&mUseCount);
    *outData = readTopAddr->Data;
    mStackNodePool.Free(readTopAddr);
}

template<typename T>
inline LONG64 LFStack<T, TRUE>::GetTop() const noexcept
{
    return mTop;
}

template<typename T>
inline LONG LFStack<T, TRUE>::GetUseCount() const noexcept
{
    return mUseCount;
}

template<typename T>
inline bool LFStack<T, TRUE>::IsEmpty() const noexcept
{
    return mTop == 0;
}

template<typename T>
inline ULONG_PTR LFStack<T, TRUE>::nextIdentifier()
{
    return static_cast<ULONG_PTR>(InterlockedIncrement64(&mCurrentIdentifier));
}

template<typename T>
inline void LFStack<T, FALSE>::Push(T data) noexcept
{
    const ULONG_PTR ident = nextIdentifier();
    Node* newTop = new Node;
    const ULONG_PTR combinedNewTop = CombineIdentAndAddr(ident, reinterpret_cast<ULONG_PTR>(newTop));
    ULONG_PTR readTop = 0;

    newTop->Data = data;

    do
    {
        readTop = static_cast<ULONG_PTR>(mTop);
        newTop->Next = readTop;
    } while (InterlockedCompareExchange64(
        &mTop,
        static_cast<LONG64>(combinedNewTop),
        static_cast<LONG64>(readTop)) != static_cast<LONG64>(readTop));

    InterlockedIncrement(&mUseCount);
}

template<typename T>
inline void LFStack<T, FALSE>::Pop(T* outData)
{
    assert(outData != nullptr);

    ULONG_PTR readTop = 0;
    ULONG_PTR newTop = 0;
    Node* readTopAddr = nullptr;

    do
    {
        readTop = static_cast<ULONG_PTR>(mTop);
        readTopAddr = reinterpret_cast<Node*>(GetAddress(readTop));

        __try
        {
            newTop = readTopAddr->Next;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            continue;
        }
    } while (InterlockedCompareExchange64(
        &mTop,
        static_cast<LONG64>(newTop),
        static_cast<LONG64>(readTop)) != static_cast<LONG64>(readTop));

    InterlockedDecrement(&mUseCount);
    *outData = readTopAddr->Data;
    delete readTopAddr;
}

template<typename T>
inline LONG64 LFStack<T, FALSE>::GetTop() const noexcept
{
    return mTop;
}

template<typename T>
inline LONG LFStack<T, FALSE>::GetUseCount() const noexcept
{
    return mUseCount;
}

template<typename T>
inline bool LFStack<T, FALSE>::IsEmpty() const noexcept
{
    return mTop == 0;
}

template<typename T>
inline ULONG_PTR LFStack<T, FALSE>::nextIdentifier()
{
    return static_cast<ULONG_PTR>(InterlockedIncrement64(&mCurrentIdentifier));
}
