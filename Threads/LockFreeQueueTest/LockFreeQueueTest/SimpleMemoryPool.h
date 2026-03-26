#pragma once

#include <cstddef>
#include <new>
#include <utility>
#include <windows.h>

template <typename T>
class SimpleMemoryPool
{
public:
    SimpleMemoryPool() noexcept = default;

    ~SimpleMemoryPool() noexcept
    {
        Slot* slot = _allSlots;

        while (slot != nullptr)
        {
            Slot* next = slot->allNext;
            ::operator delete(slot);
            slot = next;
        }
    }

    template <typename... Args>
    T* Alloc(Args&&... args)
    {
        Slot* slot = nullptr;

        AcquireSRWLockExclusive(&_lock);

        if (_freeHead != nullptr)
        {
            slot = _freeHead;
            _freeHead = _freeHead->freeNext;

            if (_freeHead == nullptr)
            {
                _freeTail = nullptr;
            }
        }
        else
        {
            slot = static_cast<Slot*>(::operator new(sizeof(Slot)));
            slot->allNext = _allSlots;
            slot->freeNext = nullptr;
            _allSlots = slot;
        }

        ReleaseSRWLockExclusive(&_lock);

        return new (slot->storage) T(std::forward<Args>(args)...);
    }

    void Free(T* data) noexcept
    {
        if (data == nullptr)
        {
            return;
        }

        Slot* slot = SlotFromValue(data);
        data->~T();

        AcquireSRWLockExclusive(&_lock);
        slot->freeNext = nullptr;

        if (_freeTail != nullptr)
        {
            _freeTail->freeNext = slot;
        }
        else
        {
            _freeHead = slot;
        }

        _freeTail = slot;
        ReleaseSRWLockExclusive(&_lock);
    }

    SimpleMemoryPool(const SimpleMemoryPool&) = delete;
    SimpleMemoryPool& operator=(const SimpleMemoryPool&) = delete;
    SimpleMemoryPool(SimpleMemoryPool&&) = delete;
    SimpleMemoryPool& operator=(SimpleMemoryPool&&) = delete;

private:
    struct Slot
    {
        Slot* allNext;
        Slot* freeNext;
        alignas(T) unsigned char storage[sizeof(T)];
    };

    static Slot* SlotFromValue(T* value) noexcept
    {
        return reinterpret_cast<Slot*>(
            reinterpret_cast<unsigned char*>(value) - offsetof(Slot, storage));
    }

    SRWLOCK _lock = SRWLOCK_INIT;
    Slot* _allSlots = nullptr;
    Slot* _freeHead = nullptr;
    Slot* _freeTail = nullptr;
};
