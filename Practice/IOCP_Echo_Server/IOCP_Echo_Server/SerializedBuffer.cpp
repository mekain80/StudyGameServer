#include "SerializedBuffer.h"

#include <cstring>

// SerializedBuffer owns an internal heap buffer, so we keep each pooled object
// constructed for its whole pool lifetime and only reset its cursors on reuse.
MemoryPool<SerializedBuffer> SerializedBuffer::sBufferPool(false, SERIALIZED_BUFFER_POOL_SIZE);

SerializedBuffer::SerializedBuffer()
	: SerializedBuffer(static_cast<int>(Constants::BufferDefault))
{
}

SerializedBuffer::SerializedBuffer(int bufferSize)
	: mBuffer(nullptr)
	, mHeaderFront(0)
	, mFront(static_cast<int>(Constants::HeaderSize))
	, mRear(static_cast<int>(Constants::HeaderSize))
	, mMaxSize(bufferSize)
	, mPoolAllocated(false)
{
	if (mMaxSize <= static_cast<int>(Constants::HeaderSize))
	{
		mMaxSize = static_cast<int>(Constants::BufferDefault);
	}

	mBuffer = new char[mMaxSize];
	Clear();
}

SerializedBuffer::SerializedBuffer(const SerializedBuffer& src)
	: mBuffer(nullptr)
	, mHeaderFront(src.mHeaderFront)
	, mFront(src.mFront)
	, mRear(src.mRear)
	, mMaxSize(src.mMaxSize)
	, mPoolAllocated(false)
{
	if (mMaxSize <= static_cast<int>(Constants::HeaderSize))
	{
		mMaxSize = static_cast<int>(Constants::BufferDefault);
	}

	mBuffer = new char[mMaxSize];
	if (src.mBuffer != nullptr && src.mRear > 0)
	{
		std::memcpy(mBuffer, src.mBuffer, static_cast<size_t>(src.mRear));
	}
}

SerializedBuffer::~SerializedBuffer() noexcept
{
	delete[] mBuffer;
	mBuffer = nullptr;
}

void SerializedBuffer::Clear() noexcept
{
	mHeaderFront = 0;
	mFront = static_cast<int>(Constants::HeaderSize);
	mRear = static_cast<int>(Constants::HeaderSize);
}

bool SerializedBuffer::EnqueueHeader(const char* buffer, int size) noexcept
{
	if (buffer == nullptr || size <= 0)
	{
		return false;
	}

	if (mHeaderFront + size > mFront)
	{
		return true;
	}

	std::memcpy(mBuffer + mHeaderFront, buffer, static_cast<size_t>(size));
	mHeaderFront += size;
	return true;
}

bool SerializedBuffer::Enqueue(const char* buffer, int size) noexcept
{
	if (buffer == nullptr || size <= 0)
	{
		return false;
	}

	if (GetFreeSize() < size)
	{
		return false;
	}

	std::memcpy(mBuffer + mRear, buffer, static_cast<size_t>(size));
	mRear += size;
	return true;
}

bool SerializedBuffer::Dequeue(char* buffer, int size) noexcept
{
	if (buffer == nullptr || size <= 0)
	{
		return false;
	}

	if (GetDataSize() < size)
	{
		return false;
	}

	std::memcpy(buffer, mBuffer + mFront, static_cast<size_t>(size));
	MoveReadPos(size);
	return true;
}

int SerializedBuffer::MoveWritePos(int size) noexcept
{
	if (size < 0 || mRear + size > mMaxSize)
	{
		return mRear;
	}

	mRear += size;
	return mRear;
}

int SerializedBuffer::MoveReadPos(int size) noexcept
{
	if (size < 0 || mFront + size > mRear)
	{
		return mFront;
	}

	mFront += size;
	return mFront;
}

SerializedBuffer& SerializedBuffer::operator=(const SerializedBuffer& src)
{
	if (this == &src)
	{
		return *this;
	}

	if (mMaxSize != src.mMaxSize)
	{
		delete[] mBuffer;
		mMaxSize = src.mMaxSize;
		if (mMaxSize <= static_cast<int>(Constants::HeaderSize))
		{
			mMaxSize = static_cast<int>(Constants::BufferDefault);
		}

		mBuffer = new char[mMaxSize];
	}

	mHeaderFront = src.mHeaderFront;
	mFront = src.mFront;
	mRear = src.mRear;
	mPoolAllocated = false;

	if (src.mBuffer != nullptr && src.mRear > 0)
	{
		std::memcpy(mBuffer, src.mBuffer, static_cast<size_t>(src.mRear));
	}

	return *this;
}

SerializedBuffer& SerializedBuffer::operator<<(unsigned char byteValue) noexcept
{
	Enqueue(reinterpret_cast<const char*>(&byteValue), sizeof(byteValue));
	return *this;
}

SerializedBuffer& SerializedBuffer::operator<<(char charValue) noexcept
{
	Enqueue(&charValue, sizeof(charValue));
	return *this;
}

SerializedBuffer& SerializedBuffer::operator<<(unsigned short ushortValue) noexcept
{
	Enqueue(reinterpret_cast<const char*>(&ushortValue), sizeof(ushortValue));
	return *this;
}

SerializedBuffer& SerializedBuffer::operator<<(short shortValue) noexcept
{
	Enqueue(reinterpret_cast<const char*>(&shortValue), sizeof(shortValue));
	return *this;
}

SerializedBuffer& SerializedBuffer::operator<<(int intValue) noexcept
{
	Enqueue(reinterpret_cast<const char*>(&intValue), sizeof(intValue));
	return *this;
}

SerializedBuffer& SerializedBuffer::operator<<(unsigned long ulongValue) noexcept
{
	Enqueue(reinterpret_cast<const char*>(&ulongValue), sizeof(ulongValue));
	return *this;
}

SerializedBuffer& SerializedBuffer::operator<<(long longValue) noexcept
{
	Enqueue(reinterpret_cast<const char*>(&longValue), sizeof(longValue));
	return *this;
}

SerializedBuffer& SerializedBuffer::operator<<(float floatValue) noexcept
{
	Enqueue(reinterpret_cast<const char*>(&floatValue), sizeof(floatValue));
	return *this;
}

SerializedBuffer& SerializedBuffer::operator<<(__int64 int64Value) noexcept
{
	Enqueue(reinterpret_cast<const char*>(&int64Value), sizeof(int64Value));
	return *this;
}

SerializedBuffer& SerializedBuffer::operator<<(double doubleValue) noexcept
{
	Enqueue(reinterpret_cast<const char*>(&doubleValue), sizeof(doubleValue));
	return *this;
}

SerializedBuffer& SerializedBuffer::operator>>(unsigned char& byteValue) noexcept
{
	Dequeue(reinterpret_cast<char*>(&byteValue), sizeof(byteValue));
	return *this;
}

SerializedBuffer& SerializedBuffer::operator>>(char& charValue) noexcept
{
	Dequeue(&charValue, sizeof(charValue));
	return *this;
}

SerializedBuffer& SerializedBuffer::operator>>(unsigned short& ushortValue) noexcept
{
	Dequeue(reinterpret_cast<char*>(&ushortValue), sizeof(ushortValue));
	return *this;
}

SerializedBuffer& SerializedBuffer::operator>>(short& shortValue) noexcept
{
	Dequeue(reinterpret_cast<char*>(&shortValue), sizeof(shortValue));
	return *this;
}

SerializedBuffer& SerializedBuffer::operator>>(int& intValue) noexcept
{
	Dequeue(reinterpret_cast<char*>(&intValue), sizeof(intValue));
	return *this;
}

SerializedBuffer& SerializedBuffer::operator>>(unsigned long& ulongValue) noexcept
{
	Dequeue(reinterpret_cast<char*>(&ulongValue), sizeof(ulongValue));
	return *this;
}

SerializedBuffer& SerializedBuffer::operator>>(long& longValue) noexcept
{
	Dequeue(reinterpret_cast<char*>(&longValue), sizeof(longValue));
	return *this;
}

SerializedBuffer& SerializedBuffer::operator>>(float& floatValue) noexcept
{
	Dequeue(reinterpret_cast<char*>(&floatValue), sizeof(floatValue));
	return *this;
}

SerializedBuffer& SerializedBuffer::operator>>(__int64& int64Value) noexcept
{
	Dequeue(reinterpret_cast<char*>(&int64Value), sizeof(int64Value));
	return *this;
}

SerializedBuffer& SerializedBuffer::operator>>(double& doubleValue) noexcept
{
	Dequeue(reinterpret_cast<char*>(&doubleValue), sizeof(doubleValue));
	return *this;
}

SerializedBuffer* SerializedBuffer::Alloc() noexcept
{
	SerializedBuffer* buffer = sBufferPool.Alloc();
	if (buffer != nullptr)
	{
		buffer->Clear();
		buffer->mPoolAllocated = true;
	}

	return buffer;
}

void SerializedBuffer::Free(SerializedBuffer* buffer) noexcept
{
	if (buffer == nullptr)
	{
		return;
	}

	if (buffer->mPoolAllocated == false)
	{
		return;
	}

	buffer->mPoolAllocated = false;
	buffer->Clear();
	sBufferPool.Free(buffer);
}
