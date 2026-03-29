#pragma once

#include <cstdlib>
#include <new>
#include <windows.h>

#include "LFDefine.h"

template<typename T>
struct MemoryPoolNode
{
    T data;
    ULONG_PTR next = 0;
};

template<typename T>
class LFMemoryPool
{
    using Node = MemoryPoolNode<T>;

public:
    LFMemoryPool(int initCount, bool placementNewFlag);
    ~LFMemoryPool();

    T* Alloc();
    void Free(T* ptr);

    LFMemoryPool(const LFMemoryPool&) = delete;
    LFMemoryPool& operator=(const LFMemoryPool&) = delete;
    LFMemoryPool(LFMemoryPool&&) = delete;
    LFMemoryPool& operator=(LFMemoryPool&&) = delete;

private:
    ULONG_PTR nextIdentifier();
    Node* placementNewAlloc();
    Node* newAlloc();

private:
    bool mPlacementNewFlag = false;
    LONG mSize = 0;
    LONG64 mCurrentIdentifier = 0;
    LONG64 mTop = 0;
};

template<typename T>
inline LFMemoryPool<T>::LFMemoryPool(int initCount, bool placementNewFlag)
    : mPlacementNewFlag(placementNewFlag)
{
    for (int i = 0; i < initCount; ++i)
    {
        const ULONG_PTR ident = nextIdentifier();
        Node* newTop = mPlacementNewFlag ? placementNewAlloc() : newAlloc();
        const ULONG_PTR combinedNewTop = CombineIdentAndAddr(ident, reinterpret_cast<ULONG_PTR>(newTop));

        newTop->next = static_cast<ULONG_PTR>(mTop);
        mTop = static_cast<LONG64>(combinedNewTop);
    }
}

template<typename T>
inline LFMemoryPool<T>::~LFMemoryPool()
{
    while (mTop != 0)
    {
        Node* delNode = reinterpret_cast<Node*>(GetAddress(static_cast<ULONG_PTR>(mTop)));

        if (mPlacementNewFlag)
        {
            delNode->data.~T();
        }

        mTop = static_cast<LONG64>(delNode->next);
        std::free(delNode);
    }
}

template<typename T>
inline T* LFMemoryPool<T>::Alloc()
{
    Node* node = nullptr;

    if (mTop == 0)
    {
        node = mPlacementNewFlag ? placementNewAlloc() : newAlloc();
    }
    else
    {
        ULONG_PTR readTop = 0;
        ULONG_PTR newTop = 0;

        do
        {
            readTop = static_cast<ULONG_PTR>(mTop);

            if (readTop == 0)
            {
                node = mPlacementNewFlag ? placementNewAlloc() : newAlloc();
                break;
            }

            node = reinterpret_cast<Node*>(GetAddress(readTop));
            newTop = node->next;
        } while (InterlockedCompareExchange64(
            &mTop,
            static_cast<LONG64>(newTop),
            static_cast<LONG64>(readTop)) != static_cast<LONG64>(readTop));

        if (mPlacementNewFlag && readTop != 0)
        {
            node = new (node) Node;
        }
    }

    return &node->data;
}

template<typename T>
inline void LFMemoryPool<T>::Free(T* ptr)
{
    if (ptr == nullptr)
    {
        return;
    }

    Node* newTop = reinterpret_cast<Node*>(ptr);

    if (mPlacementNewFlag)
    {
        newTop->data.~T();
    }

    const ULONG_PTR ident = nextIdentifier();
    const ULONG_PTR combinedNewTop = CombineIdentAndAddr(ident, reinterpret_cast<ULONG_PTR>(newTop));
    ULONG_PTR readTop = 0;

    do
    {
        readTop = static_cast<ULONG_PTR>(mTop);
        newTop->next = readTop;
    } while (InterlockedCompareExchange64(
        &mTop,
        static_cast<LONG64>(combinedNewTop),
        static_cast<LONG64>(readTop)) != static_cast<LONG64>(readTop));
}

template<typename T>
inline ULONG_PTR LFMemoryPool<T>::nextIdentifier()
{
    return static_cast<ULONG_PTR>(InterlockedIncrement64(&mCurrentIdentifier));
}

template<typename T>
inline typename LFMemoryPool<T>::Node* LFMemoryPool<T>::placementNewAlloc()
{
    Node* mallocNode = reinterpret_cast<Node*>(std::malloc(sizeof(Node)));
    Node* newNode = new (mallocNode) Node;
    InterlockedIncrement(&mSize);
    return newNode;
}

template<typename T>
inline typename LFMemoryPool<T>::Node* LFMemoryPool<T>::newAlloc()
{
    Node* newNode = new Node;
    InterlockedIncrement(&mSize);
    return newNode;
}
