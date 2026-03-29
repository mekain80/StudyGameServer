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

    void Push(T data) noexcept
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

    void Pop(T* outData) noexcept
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

    LONG64 GetTop() const noexcept
    {
        return mTop;
    }

    LONG GetUseCount() const noexcept
    {
        return mUseCount;
    }

    bool IsEmpty() const noexcept
    {
        return mTop == 0;
    }

private:
    __forceinline ULONG_PTR nextIdentifier()
    {
        return static_cast<ULONG_PTR>(InterlockedIncrement64(&mCurrentIdentifier));
    }

private:
    volatile LONG64 mTop = 0;
    volatile LONG64 mCurrentIdentifier = 0;
    LONG mUseCount = 0;
    LFMemoryPool<Node> mStackNodePool{ 1000, false };
};

template<typename T>
class LFStack<T, FALSE>
{
public:
    using Node = StackNode<T>;

    void Push(T data) noexcept
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

    void Pop(T* outData)
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

    LONG64 GetTop() const noexcept
    {
        return mTop;
    }

    LONG GetUseCount() const noexcept
    {
        return mUseCount;
    }

    bool IsEmpty() const noexcept
    {
        return mTop == 0;
    }

private:
    __forceinline ULONG_PTR nextIdentifier()
    {
        return static_cast<ULONG_PTR>(InterlockedIncrement64(&mCurrentIdentifier));
    }

private:
    volatile LONG64 mTop = 0;
    volatile LONG64 mCurrentIdentifier = 0;
    LONG mUseCount = 0;
};
