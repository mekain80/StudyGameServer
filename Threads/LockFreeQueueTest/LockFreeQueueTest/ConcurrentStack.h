#pragma once

#include <atomic>
#include <cstdio>
#include <vector>
#include <windows.h>

extern std::vector<char[64]> debugging;
extern std::atomic<int> debugCnt;

template <typename T>
class ConcurrentStack
{
    struct Node
    {
        explicit Node(const T& value)
            : data(value), next(nullptr)
        {
        }

        T data;
        Node* next;
    };

public:
    ConcurrentStack() noexcept = default;

    ~ConcurrentStack()
    {
        Node* node = LoadHead();

        while (node != nullptr)
        {
            Node* next = node->next;
            delete node;
            node = next;
        }
    }

    void Push(const T& value)
    {
        Node* newNode = new Node(value);

        while (true)
        {
            Node* oldHead = LoadHead();
            WriteDebugLog(SnapshotNode(oldHead), "Push head loaded");

            newNode->next = oldHead;
            WriteDebugPairLog(SnapshotNode(oldHead), SnapshotNode(newNode, newNode->next), "Push link old/new");

            Node* observedHead = CompareExchangeHead(newNode, oldHead);

            if (observedHead == oldHead)
            {
                WriteDebugPairLog(SnapshotNode(oldHead), SnapshotNode(newNode, newNode->next), "Push CAS ok old/new");
                return;
            }

            WriteDebugPairLog(SnapshotNode(oldHead), SnapshotNode(observedHead), "Push CAS retry exp/obs");
        }
    }

    void push(const T& value)
    {
        Push(value);
    }

    bool Pop(T& value)
    {
        while (true)
        {
            Node* oldHead = LoadHead();
            WriteDebugLog(SnapshotNode(oldHead), "Pop head loaded");

            if (oldHead == nullptr)
            {
                WriteDebugLog(SnapshotNode(nullptr, nullptr), "Pop empty");
                return false;
            }

            Node* next = oldHead->next;
            WriteDebugPairLog(SnapshotNode(oldHead, next), SnapshotNode(next), "Pop read old/next");

            Node* observedHead = CompareExchangeHead(next, oldHead);

            if (observedHead == oldHead)
            {
                WriteDebugPairLog(SnapshotNode(oldHead, next), SnapshotNode(next), "Pop CAS ok old/next");
                value = oldHead->data;
                delete oldHead;
                return true;
            }

            WriteDebugPairLog(SnapshotNode(oldHead, next), SnapshotNode(observedHead), "Pop CAS retry exp/obs");
        }
    }

    bool pop(T* value)
    {
        if (value == nullptr)
        {
            return false;
        }

        return Pop(*value);
    }

private:
    struct DebugNodeSnapshot
    {
        Node* node;
        Node* next;
        bool hasNext;
    };

    static DebugNodeSnapshot SnapshotNode(Node* node) noexcept
    {
        return DebugNodeSnapshot{ node, nullptr, false };
    }

    static DebugNodeSnapshot SnapshotNode(Node* node, Node* next) noexcept
    {
        return DebugNodeSnapshot{ node, next, true };
    }

    static unsigned int PointerSuffix(Node* node) noexcept
    {
        return static_cast<unsigned int>(reinterpret_cast<ULONG_PTR>(node) & 0xFFFFFFULL);
    }

    static void WriteDebugLog(const DebugNodeSnapshot& snapshot, const char* stage)
    {
        const int index = debugCnt.fetch_add(1, std::memory_order_relaxed);

        if (index < 0 || index >= static_cast<int>(debugging.size()))
        {
            return;
        }

        const DWORD threadDigit = GetCurrentThreadId() % 10;

        if (snapshot.hasNext)
        {
            sprintf_s(
                debugging[index],
                _countof(debugging[index]),
                "[T%lu] N=%06X(%06X) %s",
                threadDigit,
                PointerSuffix(snapshot.node),
                PointerSuffix(snapshot.next),
                stage);
            return;
        }

        sprintf_s(
            debugging[index],
            _countof(debugging[index]),
            "[T%lu] N=%06X(------) %s",
            threadDigit,
            PointerSuffix(snapshot.node),
            stage);
    }

    static void WriteDebugPairLog(const DebugNodeSnapshot& first, const DebugNodeSnapshot& second, const char* stage)
    {
        const int index = debugCnt.fetch_add(1, std::memory_order_relaxed);

        if (index < 0 || index >= static_cast<int>(debugging.size()))
        {
            return;
        }

        const DWORD threadDigit = GetCurrentThreadId() % 10;

        if (first.hasNext && second.hasNext)
        {
            sprintf_s(
                debugging[index],
                _countof(debugging[index]),
                "[T%lu] A=%06X(%06X) B=%06X(%06X) %s",
                threadDigit,
                PointerSuffix(first.node),
                PointerSuffix(first.next),
                PointerSuffix(second.node),
                PointerSuffix(second.next),
                stage);
            return;
        }

        if (first.hasNext)
        {
            sprintf_s(
                debugging[index],
                _countof(debugging[index]),
                "[T%lu] A=%06X(%06X) B=%06X(------) %s",
                threadDigit,
                PointerSuffix(first.node),
                PointerSuffix(first.next),
                PointerSuffix(second.node),
                stage);
            return;
        }

        if (second.hasNext)
        {
            sprintf_s(
                debugging[index],
                _countof(debugging[index]),
                "[T%lu] A=%06X(------) B=%06X(%06X) %s",
                threadDigit,
                PointerSuffix(first.node),
                PointerSuffix(second.node),
                PointerSuffix(second.next),
                stage);
            return;
        }

        sprintf_s(
            debugging[index],
            _countof(debugging[index]),
            "[T%lu] A=%06X(------) B=%06X(------) %s",
            threadDigit,
            PointerSuffix(first.node),
            PointerSuffix(second.node),
            stage);
    }

    static LONG64 EncodeNode(Node* node) noexcept
    {
        return static_cast<LONG64>(reinterpret_cast<LONG_PTR>(node));
    }

    static Node* DecodeNode(LONG64 value) noexcept
    {
        return reinterpret_cast<Node*>(static_cast<LONG_PTR>(value));
    }

    Node* LoadHead() const noexcept
    {
        return DecodeNode(InterlockedCompareExchange64(const_cast<volatile LONG64*>(&_head), 0, 0));
    }

    Node* CompareExchangeHead(Node* exchange, Node* comparand) noexcept
    {
        return DecodeNode(InterlockedCompareExchange64(&_head, EncodeNode(exchange), EncodeNode(comparand)));
    }

    volatile LONG64 _head = 0;
};

template <typename T>
using LockFreeStack = ConcurrentStack<T>;
