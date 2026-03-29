#pragma once

#include <cstdlib>
#include <new>
#include <windows.h>

#include "LFDefine.h"

template<typename DATA>
struct MemoryPoolNode
{
    DATA Data;
    ULONG_PTR Next = 0;
};

template<typename DATA>
class LFMemoryPool
{
    using Node = MemoryPoolNode<DATA>;

public:
    __forceinline LFMemoryPool(int initCount, bool placementNewFlag)
        : mPlacementNewFlag(placementNewFlag)
    {
        for (int i = 0; i < initCount; ++i)
        {
            const ULONG_PTR ident = nextIdentifier();
            Node* newTop = mPlacementNewFlag ? placementNewAlloc() : newAlloc();
            const ULONG_PTR combinedNewTop = CombineIdentAndAddr(ident, reinterpret_cast<ULONG_PTR>(newTop));

            newTop->Next = static_cast<ULONG_PTR>(mTop);
            mTop = static_cast<LONG64>(combinedNewTop);
        }
    }

    __forceinline ~LFMemoryPool()
    {
        while (mTop != 0)
        {
            Node* delNode = reinterpret_cast<Node*>(GetAddress(static_cast<ULONG_PTR>(mTop)));

            if (mPlacementNewFlag)
            {
                delNode->Data.~DATA();
            }

            mTop = static_cast<LONG64>(delNode->Next);
            free(delNode);
        }
    }

    __forceinline DATA* Alloc()
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
                newTop = node->Next;
            } while (InterlockedCompareExchange64(
                &mTop,
                static_cast<LONG64>(newTop),
                static_cast<LONG64>(readTop)) != static_cast<LONG64>(readTop));

            if (mPlacementNewFlag && readTop != 0)
            {
                node = new (node) Node;
            }
        }

        return &node->Data;
    }

    __forceinline void Free(DATA* ptr)
    {
        if (ptr == nullptr)
        {
            return;
        }

        Node* newTop = reinterpret_cast<Node*>(ptr);

        if (mPlacementNewFlag)
        {
            newTop->Data.~DATA();
        }

        const ULONG_PTR ident = nextIdentifier();
        const ULONG_PTR combinedNewTop = CombineIdentAndAddr(ident, reinterpret_cast<ULONG_PTR>(newTop));
        ULONG_PTR readTop = 0;

        do
        {
            readTop = static_cast<ULONG_PTR>(mTop);
            newTop->Next = readTop;
        } while (InterlockedCompareExchange64(
            &mTop,
            static_cast<LONG64>(combinedNewTop),
            static_cast<LONG64>(readTop)) != static_cast<LONG64>(readTop));
    }

private:
    __forceinline ULONG_PTR nextIdentifier()
    {
        return static_cast<ULONG_PTR>(InterlockedIncrement64(&mCurrentIdentifier));
    }

    __forceinline Node* placementNewAlloc()
    {
        Node* mallocNode = reinterpret_cast<Node*>(malloc(sizeof(Node)));
        Node* newNode = new (mallocNode) Node;
        InterlockedIncrement(&mSize);
        return newNode;
    }

    __forceinline Node* newAlloc()
    {
        Node* newNode = new Node;
        InterlockedIncrement(&mSize);
        return newNode;
    }

private:
    bool mPlacementNewFlag = false;
    LONG mSize = 0;
    volatile LONG64 mCurrentIdentifier = 0;
    volatile LONG64 mTop = 0;
};
