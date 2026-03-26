#pragma once

#include <atomic>
#include <cstdio>
#include <type_traits>
#include <vector>
#include <windows.h>

#include "SimpleMemoryPool.h"

extern std::vector<char[128]> debugging;
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
        Node* node = LoadHead().node;

        while (node != nullptr)
        {
            Node* next = node->next;
            _nodePool.Free(node);
            node = next;
        }
    }

    void Push(const T& value)
    {
        Node* newNode = _nodePool.Alloc(value);

        while (true)
        {
            const TaggedHead oldHead = LoadHead();
            WriteDebugTaggedHeadLog(oldHead, "Push head loaded");

            newNode->next = oldHead.node;
            WriteDebugPairValueLog(
                SnapshotNode(oldHead.node),
                SnapshotNode(newNode, newNode->next),
                newNode->data,
                "Push link old/new");

            const TaggedHead newHead = MakeHead(newNode, NextTag(oldHead.tag));
            const TaggedHead observedHead = CompareExchangeHead(newHead, oldHead);

            if (observedHead.raw == oldHead.raw)
            {
                WriteDebugTaggedHeadValueLog(
                    oldHead,
                    newHead,
                    newNode->data,
                    "Push CAS ok old/new");
                return;
            }

            WriteDebugTaggedHeadPairLog(oldHead, observedHead, "Push CAS retry exp/obs");
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
            const TaggedHead oldHead = LoadHead();
            WriteDebugTaggedHeadLog(oldHead, "Pop head loaded");

            if (oldHead.node == nullptr)
            {
                WriteDebugLog(SnapshotNode(nullptr, nullptr), "Pop empty");
                return false;
            }

            Node* next = oldHead.node->next;
            WriteDebugPairLog(SnapshotNode(oldHead.node, next), SnapshotNode(next), "Pop read old/next");

            const TaggedHead newHead = MakeHead(next, NextTag(oldHead.tag));
            const TaggedHead observedHead = CompareExchangeHead(newHead, oldHead);

            if (observedHead.raw == oldHead.raw)
            {
                const T poppedValue = oldHead.node->data;
                WriteDebugTaggedHeadValueLog(
                    oldHead,
                    newHead,
                    poppedValue,
                    "Pop CAS ok old/next");
                value = poppedValue;
                _nodePool.Free(oldHead.node);
                return true;
            }

            WriteDebugTaggedHeadPairLog(oldHead, observedHead, "Pop CAS retry exp/obs");
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
    static_assert(sizeof(void*) == 8, "ConcurrentStack tagged head requires x64.");

    struct DebugNodeSnapshot
    {
        Node* node;
        Node* next;
        bool hasNext;
    };

    struct TaggedHead
    {
        Node* node;
        unsigned int tag;
        ULONG64 raw;
    };

    static constexpr int kPointerBits = 47;
    static constexpr ULONG64 kPointerMask = (1ULL << kPointerBits) - 1ULL;
    static constexpr ULONG64 kTagMask = (1ULL << (64 - kPointerBits)) - 1ULL;

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

    static unsigned int NextTag(unsigned int tag) noexcept
    {
        return static_cast<unsigned int>((static_cast<ULONG64>(tag) + 1ULL) & kTagMask);
    }

    static ULONG64 EncodeHeadRaw(Node* node, unsigned int tag) noexcept
    {
        const ULONG64 pointerPart =
            static_cast<ULONG64>(reinterpret_cast<ULONG_PTR>(node)) & kPointerMask;
        const ULONG64 tagPart =
            (static_cast<ULONG64>(tag) & kTagMask) << kPointerBits;

        return tagPart | pointerPart;
    }

    static TaggedHead DecodeHead(ULONG64 raw) noexcept
    {
        return TaggedHead
        {
            reinterpret_cast<Node*>(static_cast<ULONG_PTR>(raw & kPointerMask)),
            static_cast<unsigned int>((raw >> kPointerBits) & kTagMask),
            raw
        };
    }

    static TaggedHead MakeHead(Node* node, unsigned int tag) noexcept
    {
        return DecodeHead(EncodeHeadRaw(node, tag));
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

    static void WriteDebugTaggedHeadLog(const TaggedHead& head, const char* stage)
    {
        const int index = debugCnt.fetch_add(1, std::memory_order_relaxed);

        if (index < 0 || index >= static_cast<int>(debugging.size()))
        {
            return;
        }

        const DWORD threadDigit = GetCurrentThreadId() % 10;

        sprintf_s(
            debugging[index],
            _countof(debugging[index]),
            "[T%lu] H=%06X[t=%u] %s",
            threadDigit,
            PointerSuffix(head.node),
            head.tag,
            stage);
    }

    static void WriteDebugTaggedHeadPairLog(
        const TaggedHead& first,
        const TaggedHead& second,
        const char* stage)
    {
        const int index = debugCnt.fetch_add(1, std::memory_order_relaxed);

        if (index < 0 || index >= static_cast<int>(debugging.size()))
        {
            return;
        }

        const DWORD threadDigit = GetCurrentThreadId() % 10;

        sprintf_s(
            debugging[index],
            _countof(debugging[index]),
            "[T%lu] A=%06X[t=%u] B=%06X[t=%u] %s",
            threadDigit,
            PointerSuffix(first.node),
            first.tag,
            PointerSuffix(second.node),
            second.tag,
            stage);
    }

    static void FormatValueText(const T& value, char* buffer, size_t bufferCount)
    {
        if constexpr (std::is_integral_v<T> || std::is_enum_v<T>)
        {
            sprintf_s(buffer, bufferCount, "%lld", static_cast<long long>(value));
        }
        else if constexpr (std::is_floating_point_v<T>)
        {
            sprintf_s(buffer, bufferCount, "%.3f", static_cast<double>(value));
        }
        else if constexpr (std::is_pointer_v<T>)
        {
            sprintf_s(buffer, bufferCount, "%p", value);
        }
        else
        {
            strcpy_s(buffer, bufferCount, "?");
        }
    }

    static void WriteDebugPairValueLog(
        const DebugNodeSnapshot& first,
        const DebugNodeSnapshot& second,
        const T& value,
        const char* stage)
    {
        const int index = debugCnt.fetch_add(1, std::memory_order_relaxed);

        if (index < 0 || index >= static_cast<int>(debugging.size()))
        {
            return;
        }

        char valueText[32] = {};
        FormatValueText(value, valueText, _countof(valueText));

        const DWORD threadDigit = GetCurrentThreadId() % 10;

        if (first.hasNext && second.hasNext)
        {
            sprintf_s(
                debugging[index],
                _countof(debugging[index]),
                "[T%lu] A=%06X(%06X) B=%06X(%06X) V=%s %s",
                threadDigit,
                PointerSuffix(first.node),
                PointerSuffix(first.next),
                PointerSuffix(second.node),
                PointerSuffix(second.next),
                valueText,
                stage);
            return;
        }

        if (first.hasNext)
        {
            sprintf_s(
                debugging[index],
                _countof(debugging[index]),
                "[T%lu] A=%06X(%06X) B=%06X(------) V=%s %s",
                threadDigit,
                PointerSuffix(first.node),
                PointerSuffix(first.next),
                PointerSuffix(second.node),
                valueText,
                stage);
            return;
        }

        if (second.hasNext)
        {
            sprintf_s(
                debugging[index],
                _countof(debugging[index]),
                "[T%lu] A=%06X(------) B=%06X(%06X) V=%s %s",
                threadDigit,
                PointerSuffix(first.node),
                PointerSuffix(second.node),
                PointerSuffix(second.next),
                valueText,
                stage);
            return;
        }

        sprintf_s(
            debugging[index],
            _countof(debugging[index]),
            "[T%lu] A=%06X(------) B=%06X(------) V=%s %s",
            threadDigit,
            PointerSuffix(first.node),
            PointerSuffix(second.node),
            valueText,
            stage);
    }

    static void WriteDebugTaggedHeadValueLog(
        const TaggedHead& first,
        const TaggedHead& second,
        const T& value,
        const char* stage)
    {
        const int index = debugCnt.fetch_add(1, std::memory_order_relaxed);

        if (index < 0 || index >= static_cast<int>(debugging.size()))
        {
            return;
        }

        char valueText[32] = {};
        FormatValueText(value, valueText, _countof(valueText));

        const DWORD threadDigit = GetCurrentThreadId() % 10;

        sprintf_s(
            debugging[index],
            _countof(debugging[index]),
            "[T%lu] A=%06X[t=%u] B=%06X[t=%u] V=%s %s",
            threadDigit,
            PointerSuffix(first.node),
            first.tag,
            PointerSuffix(second.node),
            second.tag,
            valueText,
            stage);
    }

    TaggedHead LoadHead() const noexcept
    {
        return DecodeHead(static_cast<ULONG64>(
            InterlockedCompareExchange64(const_cast<volatile LONG64*>(&_head), 0, 0)));
    }

    TaggedHead CompareExchangeHead(const TaggedHead& exchange, const TaggedHead& comparand) noexcept
    {
        return DecodeHead(static_cast<ULONG64>(
            InterlockedCompareExchange64(
                &_head,
                static_cast<LONG64>(exchange.raw),
                static_cast<LONG64>(comparand.raw))));
    }

    SimpleMemoryPool<Node> _nodePool;
    alignas(8) volatile LONG64 _head = 0;
};

template <typename T>
using LockFreeStack = ConcurrentStack<T>;
