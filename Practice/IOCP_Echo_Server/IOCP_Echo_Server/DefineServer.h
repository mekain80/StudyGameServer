#pragma once

#include "Common.h"

enum class IOOperation
{
	RECV,
	SEND
};

struct OverlappedEx
{
	explicit OverlappedEx(IOOperation operation = IOOperation::RECV)
		: mOperation(operation)
	{
		ZeroMemory(&mOverlapped, sizeof(mOverlapped));
	}

	void Reset(IOOperation operation)
	{
		ZeroMemory(&mOverlapped, sizeof(mOverlapped));
		mOperation = operation;
	}

	OVERLAPPED mOverlapped;
	IOOperation mOperation;
};
