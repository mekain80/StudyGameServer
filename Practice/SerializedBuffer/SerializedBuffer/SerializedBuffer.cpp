#include "SerializedBuffer.h"
#include <cstring> // memcpy, memmove

SerializedBuffer::SerializedBuffer()
	: SerializedBuffer(eBUFFER_DEFAULT)
{
}

SerializedBuffer::SerializedBuffer(int bufferSize)
	: mBufferSize(bufferSize),
	mDataSize(0),
	mBuffer(nullptr)
{
	if (mBufferSize <= 0)
		mBufferSize = eBUFFER_DEFAULT;

	mBuffer = new char[mBufferSize]; // new는 예외 가능
}

SerializedBuffer::SerializedBuffer(const SerializedBuffer& src)
	: mBufferSize(src.mBufferSize),
	mDataSize(src.mDataSize),
	mBuffer(nullptr)
{
	if (mBufferSize <= 0)
		mBufferSize = eBUFFER_DEFAULT;

	mBuffer = new char[mBufferSize];

	if (mDataSize > 0)
	{
		std::memcpy(mBuffer, src.mBuffer, static_cast<size_t>(mDataSize));
	}
}

SerializedBuffer::~SerializedBuffer() noexcept
{
	delete[] mBuffer;
	mBuffer = nullptr;
}

void SerializedBuffer::Clear() noexcept
{
	mDataSize = 0;
}

int SerializedBuffer::MoveWritePos(int size) noexcept
{
	if (size <= 0)
		return 0;

	const int freeSize = mBufferSize - mDataSize;
	if (size > freeSize)
		return 0;

	mDataSize += size;
	return size;
}

int SerializedBuffer::MoveReadPos(int size) noexcept
{
	if (size <= 0)
		return 0;

	int moveSize = size;
	if (moveSize > mDataSize)
		moveSize = mDataSize;

	const int remain = mDataSize - moveSize;
	if (remain > 0)
	{
		std::memmove(mBuffer, mBuffer + moveSize, static_cast<size_t>(remain));
	}

	mDataSize -= moveSize;
	return moveSize;
}

SerializedBuffer& SerializedBuffer::operator=(const SerializedBuffer& src)
{
	if (this == &src)
		return *this;

	if (mBufferSize != src.mBufferSize)
	{
		delete[] mBuffer;

		mBufferSize = src.mBufferSize;
		if (mBufferSize <= 0)
			mBufferSize = eBUFFER_DEFAULT;

		mBuffer = new char[mBufferSize]; // 예외 가능
	}

	mDataSize = src.mDataSize;

	if (mDataSize > 0)
	{
		std::memcpy(mBuffer, src.mBuffer, static_cast<size_t>(mDataSize));
	}

	return *this;
}

// ======================= Put 연산자 (<<) =======================

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

// ======================= Get 연산자 (>>) =======================
// GetData는 "가능한 만큼" 복사하는 성격이라,
// operator>>에서는 데이터가 충분할 때만 읽도록 가드.

SerializedBuffer& SerializedBuffer::operator>>(unsigned char& byteValue) noexcept
{
	if (mDataSize >= static_cast<int>(sizeof(byteValue)))
		GetData(reinterpret_cast<char*>(&byteValue), sizeof(byteValue));
	return *this;
}

SerializedBuffer& SerializedBuffer::operator>>(char& charValue) noexcept
{
	if (mDataSize >= static_cast<int>(sizeof(charValue)))
		GetData(&charValue, sizeof(charValue));
	return *this;
}

SerializedBuffer& SerializedBuffer::operator>>(unsigned short& ushortValue) noexcept
{
	if (mDataSize >= static_cast<int>(sizeof(ushortValue)))
		GetData(reinterpret_cast<char*>(&ushortValue), sizeof(ushortValue));
	return *this;
}

SerializedBuffer& SerializedBuffer::operator>>(short& shortValue) noexcept
{
	if (mDataSize >= static_cast<int>(sizeof(shortValue)))
		GetData(reinterpret_cast<char*>(&shortValue), sizeof(shortValue));
	return *this;
}

SerializedBuffer& SerializedBuffer::operator>>(int& intValue) noexcept
{
	if (mDataSize >= static_cast<int>(sizeof(intValue)))
		GetData(reinterpret_cast<char*>(&intValue), sizeof(intValue));
	return *this;
}

SerializedBuffer& SerializedBuffer::operator>>(unsigned long& ulongValue) noexcept
{
	if (mDataSize >= static_cast<int>(sizeof(ulongValue)))
		GetData(reinterpret_cast<char*>(&ulongValue), sizeof(ulongValue));
	return *this;
}

SerializedBuffer& SerializedBuffer::operator>>(long& longValue) noexcept
{
	if (mDataSize >= static_cast<int>(sizeof(longValue)))
		GetData(reinterpret_cast<char*>(&longValue), sizeof(longValue));
	return *this;
}

SerializedBuffer& SerializedBuffer::operator>>(float& floatValue) noexcept
{
	if (mDataSize >= static_cast<int>(sizeof(floatValue)))
		GetData(reinterpret_cast<char*>(&floatValue), sizeof(floatValue));
	return *this;
}

SerializedBuffer& SerializedBuffer::operator>>(__int64& int64Value) noexcept
{
	if (mDataSize >= static_cast<int>(sizeof(int64Value)))
		GetData(reinterpret_cast<char*>(&int64Value), sizeof(int64Value));
	return *this;
}

SerializedBuffer& SerializedBuffer::operator>>(double& doubleValue) noexcept
{
	if (mDataSize >= static_cast<int>(sizeof(doubleValue)))
		GetData(reinterpret_cast<char*>(&doubleValue), sizeof(doubleValue));
	return *this;
}

// ======================= GetData / PutData =======================

int SerializedBuffer::GetData(char* dest, int size) noexcept
{
	if (size <= 0 || dest == nullptr)
		return 0;

	int copySize = size;
	if (copySize > mDataSize)
		copySize = mDataSize;

	if (copySize <= 0)
		return 0;

	std::memcpy(dest, mBuffer, static_cast<size_t>(copySize));

	const int remain = mDataSize - copySize;
	if (remain > 0)
	{
		std::memmove(mBuffer, mBuffer + copySize, static_cast<size_t>(remain));
	}

	mDataSize -= copySize;
	return copySize;
}

int SerializedBuffer::PutData(const char* src, int srcSize) noexcept
{
	if (srcSize <= 0 || src == nullptr)
		return 0;

	const int freeSize = mBufferSize - mDataSize;
	if (srcSize > freeSize)
		return 0;

	std::memcpy(mBuffer + mDataSize, src, static_cast<size_t>(srcSize));
	mDataSize += srcSize;
	return srcSize;
}
