#pragma once

#include <windows.h>

static_assert(sizeof(void*) == 8, "LockFreeStack tagging assumes x64 user-mode addresses.");

constexpr ULONG_PTR ADDR_MASK = 0x00007FFFFFFFFFFFULL;
constexpr ULONG_PTR IDENTIFIER_MASK = ~ADDR_MASK;
constexpr INT ZERO_BIT = 17;
constexpr INT NON_ZERO_BIT = 64 - ZERO_BIT;
constexpr ULONG_PTR MAX_IDENTIFIER = (1ULL << ZERO_BIT) - 1ULL;

inline ULONG_PTR CombineIdentAndAddr(ULONG_PTR ident, ULONG_PTR addr)
{
    const ULONG_PTR maskedIdent = (ident & MAX_IDENTIFIER) << NON_ZERO_BIT;
    const ULONG_PTR maskedAddr = addr & ADDR_MASK;
    return maskedIdent | maskedAddr;
}

inline ULONG_PTR GetIdentifier(ULONG_PTR combined)
{
    return (combined & IDENTIFIER_MASK) >> NON_ZERO_BIT;
}

inline ULONG_PTR GetAddress(ULONG_PTR combined)
{
    return combined & ADDR_MASK;
}
