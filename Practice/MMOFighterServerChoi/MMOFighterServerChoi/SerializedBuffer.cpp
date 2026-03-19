#include "stdafx.h"
#include "SerializedBuffer.h"
#include "MemoryPool.h"
#include <cstring>

namespace
{
	struct SerializedBufferPoolBlock
	{
		char data[SerializedBuffer::eBUFFER_DEFAULT];
	};

	constexpr unsigned int kSerializedBufferPoolInitCount = 1024;
	MemoryPool<SerializedBufferPoolBlock> gSerializedBufferPool(true, kSerializedBufferPoolInitCount);
}

SerializedBuffer::SerializedBuffer()
	: SerializedBuffer(eBUFFER_DEFAULT)
{
}

SerializedBuffer::SerializedBuffer(int bufferSize)
	: mBuffer(nullptr),
	mBegin(nullptr),
	mRead(nullptr),
	mWrite(nullptr),
	mEnd(nullptr),
	mBufferSize(0),
	mPoolBlock(nullptr)
{
	AllocateBuffer(bufferSize);
}

SerializedBuffer::SerializedBuffer(const SerializedBuffer& src)
	: mBuffer(nullptr),
	mBegin(nullptr),
	mRead(nullptr),
	mWrite(nullptr),
	mEnd(nullptr),
	mBufferSize(0),
	mPoolBlock(nullptr)
{
	AllocateBuffer(src.mBufferSize);

	const int readOffset = static_cast<int>(src.mRead - src.mBegin);
	const int writeOffset = static_cast<int>(src.mWrite - src.mBegin);
	if (writeOffset > 0)
	{
		std::memcpy(mBuffer, src.mBuffer, static_cast<size_t>(writeOffset));
		mRead = mBegin + readOffset;
		mWrite = mBegin + writeOffset;
	}
}

SerializedBuffer::~SerializedBuffer() noexcept
{
	ReleaseBuffer();
}

void SerializedBuffer::AllocateBuffer(int bufferSize)
{
	UNREFERENCED_PARAMETER(bufferSize);

	if (mPoolBlock != nullptr)
	{
		ReleaseBuffer();
	}

	mBufferSize = eBUFFER_DEFAULT;
	mPoolBlock = gSerializedBufferPool.Alloc();
	mBuffer = (mPoolBlock != nullptr)
		? static_cast<SerializedBufferPoolBlock*>(mPoolBlock)->data
		: nullptr;

	InitializePointers();
}

void SerializedBuffer::ReleaseBuffer() noexcept
{
	if (mBuffer == nullptr)
	{
		return;
	}

	gSerializedBufferPool.Free(static_cast<SerializedBufferPoolBlock*>(mPoolBlock));

	mBuffer = nullptr;
	mBegin = nullptr;
	mRead = nullptr;
	mWrite = nullptr;
	mEnd = nullptr;
	mBufferSize = 0;
	mPoolBlock = nullptr;
}

void SerializedBuffer::InitializePointers() noexcept
{
	mBegin = mBuffer;
	mRead = mBuffer;
	mWrite = mBuffer;
	mEnd = (mBuffer != nullptr) ? (mBuffer + mBufferSize) : nullptr;
}

void SerializedBuffer::ResetPointers() noexcept
{
	mRead = mBegin;
	mWrite = mBegin;
}

void SerializedBuffer::Clear() noexcept
{
	ResetPointers();
}

int SerializedBuffer::MoveWritePos(int size) noexcept
{
	if (size <= 0)
		return 0;

	if (size > GetFreeSize())
		return 0;

	mWrite += size;
	return size;
}

int SerializedBuffer::MoveReadPos(int size) noexcept
{
	if (size <= 0)
		return 0;

	int moveSize = size;
	const int dataSize = GetDataSize();
	if (moveSize > dataSize)
		moveSize = dataSize;

	if (moveSize <= 0)
		return 0;

	mRead += moveSize;
	if (mRead == mWrite)
		ResetPointers();

	return moveSize;
}

SerializedBuffer& SerializedBuffer::operator=(const SerializedBuffer& src)
{
	if (this == &src)
		return *this;

	if (mBufferSize != src.mBufferSize)
	{
		ReleaseBuffer();
		AllocateBuffer(src.mBufferSize);
	}

	InitializePointers();

	const int readOffset = static_cast<int>(src.mRead - src.mBegin);
	const int writeOffset = static_cast<int>(src.mWrite - src.mBegin);
	if (writeOffset > 0)
	{
		std::memcpy(mBuffer, src.mBuffer, static_cast<size_t>(writeOffset));
		mRead = mBegin + readOffset;
		mWrite = mBegin + writeOffset;
	}

	return *this;
}

SerializedBuffer& SerializedBuffer::operator<<(unsigned char byteValue) noexcept
{
	PutData(reinterpret_cast<const char*>(&byteValue), sizeof(byteValue));
	return *this;
}

SerializedBuffer& SerializedBuffer::operator<<(char charValue) noexcept
{
	PutData(&charValue, sizeof(charValue));
	return *this;
}

SerializedBuffer& SerializedBuffer::operator<<(unsigned short ushortValue) noexcept
{
	PutData(reinterpret_cast<const char*>(&ushortValue), sizeof(ushortValue));
	return *this;
}

SerializedBuffer& SerializedBuffer::operator<<(short shortValue) noexcept
{
	PutData(reinterpret_cast<const char*>(&shortValue), sizeof(shortValue));
	return *this;
}

SerializedBuffer& SerializedBuffer::operator<<(int intValue) noexcept
{
	PutData(reinterpret_cast<const char*>(&intValue), sizeof(intValue));
	return *this;
}

SerializedBuffer& SerializedBuffer::operator<<(unsigned long ulongValue) noexcept
{
	PutData(reinterpret_cast<const char*>(&ulongValue), sizeof(ulongValue));
	return *this;
}

SerializedBuffer& SerializedBuffer::operator<<(long longValue) noexcept
{
	PutData(reinterpret_cast<const char*>(&longValue), sizeof(longValue));
	return *this;
}

SerializedBuffer& SerializedBuffer::operator<<(float floatValue) noexcept
{
	PutData(reinterpret_cast<const char*>(&floatValue), sizeof(floatValue));
	return *this;
}

SerializedBuffer& SerializedBuffer::operator<<(__int64 int64Value) noexcept
{
	PutData(reinterpret_cast<const char*>(&int64Value), sizeof(int64Value));
	return *this;
}

SerializedBuffer& SerializedBuffer::operator<<(double doubleValue) noexcept
{
	PutData(reinterpret_cast<const char*>(&doubleValue), sizeof(doubleValue));
	return *this;
}

SerializedBuffer& SerializedBuffer::operator>>(unsigned char& byteValue) noexcept
{
	if (GetDataSize() >= static_cast<int>(sizeof(byteValue)))
		GetData(reinterpret_cast<char*>(&byteValue), sizeof(byteValue));
	return *this;
}

SerializedBuffer& SerializedBuffer::operator>>(char& charValue) noexcept
{
	if (GetDataSize() >= static_cast<int>(sizeof(charValue)))
		GetData(&charValue, sizeof(charValue));
	return *this;
}

SerializedBuffer& SerializedBuffer::operator>>(unsigned short& ushortValue) noexcept
{
	if (GetDataSize() >= static_cast<int>(sizeof(ushortValue)))
		GetData(reinterpret_cast<char*>(&ushortValue), sizeof(ushortValue));
	return *this;
}

SerializedBuffer& SerializedBuffer::operator>>(short& shortValue) noexcept
{
	if (GetDataSize() >= static_cast<int>(sizeof(shortValue)))
		GetData(reinterpret_cast<char*>(&shortValue), sizeof(shortValue));
	return *this;
}

SerializedBuffer& SerializedBuffer::operator>>(int& intValue) noexcept
{
	if (GetDataSize() >= static_cast<int>(sizeof(intValue)))
		GetData(reinterpret_cast<char*>(&intValue), sizeof(intValue));
	return *this;
}

SerializedBuffer& SerializedBuffer::operator>>(unsigned long& ulongValue) noexcept
{
	if (GetDataSize() >= static_cast<int>(sizeof(ulongValue)))
		GetData(reinterpret_cast<char*>(&ulongValue), sizeof(ulongValue));
	return *this;
}

SerializedBuffer& SerializedBuffer::operator>>(long& longValue) noexcept
{
	if (GetDataSize() >= static_cast<int>(sizeof(longValue)))
		GetData(reinterpret_cast<char*>(&longValue), sizeof(longValue));
	return *this;
}

SerializedBuffer& SerializedBuffer::operator>>(float& floatValue) noexcept
{
	if (GetDataSize() >= static_cast<int>(sizeof(floatValue)))
		GetData(reinterpret_cast<char*>(&floatValue), sizeof(floatValue));
	return *this;
}

SerializedBuffer& SerializedBuffer::operator>>(__int64& int64Value) noexcept
{
	if (GetDataSize() >= static_cast<int>(sizeof(int64Value)))
		GetData(reinterpret_cast<char*>(&int64Value), sizeof(int64Value));
	return *this;
}

SerializedBuffer& SerializedBuffer::operator>>(double& doubleValue) noexcept
{
	if (GetDataSize() >= static_cast<int>(sizeof(doubleValue)))
		GetData(reinterpret_cast<char*>(&doubleValue), sizeof(doubleValue));
	return *this;
}

int SerializedBuffer::GetData(char* dest, int size) noexcept
{
	if (size <= 0 || dest == nullptr)
		return 0;

	int copySize = size;
	const int dataSize = GetDataSize();
	if (copySize > dataSize)
		copySize = dataSize;

	if (copySize <= 0)
		return 0;

	std::memcpy(dest, mRead, static_cast<size_t>(copySize));
	MoveReadPos(copySize);
	return copySize;
}

int SerializedBuffer::PutData(const char* src, int srcSize) noexcept
{
	if (srcSize <= 0 || src == nullptr)
		return 0;

	if (srcSize > GetFreeSize())
		return 0;

	std::memcpy(mWrite, src, static_cast<size_t>(srcSize));
	mWrite += srcSize;
	return srcSize;
}
