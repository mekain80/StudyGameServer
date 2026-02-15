#include "SerializedBuffer.h"
#include <cstring>   // memcpy, memmove
#include <algorithm> // min

SerializedBuffer::SerializedBuffer() : SerializedBuffer(eBUFFER_DEFAULT)
{
}


SerializedBuffer::SerializedBuffer(int iBUfferSize)
	: mBufferSize(iBUfferSize),
	mDataSize(0)
{
	mBuffer = new char[mBufferSize];
}

SerializedBuffer::~SerializedBuffer()
{
	delete[] mBuffer;
}

void SerializedBuffer::Clear()
{
	mDataSize = 0;
}

int SerializedBuffer::MoveWritePos(int iSize)
{
	if (iSize <= 0)
		return 0;

	// 남은 공간보다 더 많이 이동하라고 하면 하지 않음
	int freeSize = mBufferSize - mDataSize;
	if (freeSize < iSize)
		return 0;

	mDataSize += iSize;
	return iSize;
}

int SerializedBuffer::MoveReadPos(int iSize)
{
	if (iSize <= 0)
		return 0;

	int moveSize = min(iSize, mDataSize);

	// 사용된 데이터 앞으로 땡기기
	int remain = mDataSize - moveSize;
	if (remain > 0)
	{
		std::memmove(mBuffer,
			mBuffer + moveSize,
			remain);
	}

	mDataSize -= moveSize;
	return moveSize;
}

/// 대입 연산자
SerializedBuffer& SerializedBuffer::operator=(SerializedBuffer& clSrcPacket)
{
	if (this == &clSrcPacket)
		return *this;

	// 버퍼 크기가 다르면 다시 할당
	if (mBufferSize != clSrcPacket.mBufferSize)
	{
		delete[] mBuffer;
		mBufferSize = clSrcPacket.mBufferSize;
		mBuffer = new char[mBufferSize];
	}

	mDataSize = clSrcPacket.mDataSize;

	// 실제 들어있는 데이터만 복사
	if (mDataSize > 0)
	{
		std::memcpy(mBuffer,
			clSrcPacket.mBuffer,
			mDataSize);
	}

	return *this;
}

// ======================= Put 연산자 (<<) =======================

SerializedBuffer& SerializedBuffer::operator<<(unsigned char byValue)
{
	PutData(reinterpret_cast<char*>(&byValue), sizeof(byValue));
	return *this;
}

SerializedBuffer& SerializedBuffer::operator<<(char chValue)
{
	PutData(&chValue, sizeof(chValue));
	return *this;
}

SerializedBuffer& SerializedBuffer::operator<<(short shValue)
{
	PutData(reinterpret_cast<char*>(&shValue), sizeof(shValue));
	return *this;
}

SerializedBuffer& SerializedBuffer::operator<<(unsigned short wValue)
{
	PutData(reinterpret_cast<char*>(&wValue), sizeof(wValue));
	return *this;
}

SerializedBuffer& SerializedBuffer::operator<<(int iValue)
{
	PutData(reinterpret_cast<char*>(&iValue), sizeof(iValue));
	return *this;
}

SerializedBuffer& SerializedBuffer::operator<<(long lValue)
{
	PutData(reinterpret_cast<char*>(&lValue), sizeof(lValue));
	return *this;
}

SerializedBuffer& SerializedBuffer::operator<<(float fValue)
{
	PutData(reinterpret_cast<char*>(&fValue), sizeof(fValue));
	return *this;
}

SerializedBuffer& SerializedBuffer::operator<<(__int64 iValue)
{
	PutData(reinterpret_cast<char*>(&iValue), sizeof(iValue));
	return *this;
}

SerializedBuffer& SerializedBuffer::operator<<(double dValue)
{
	PutData(reinterpret_cast<char*>(&dValue), sizeof(dValue));
	return *this;
}

// ======================= Get 연산자 (>>) =======================

SerializedBuffer& SerializedBuffer::operator>>(BYTE& byValue)
{
	GetData(reinterpret_cast<char*>(&byValue), sizeof(byValue));
	return *this;
}

SerializedBuffer& SerializedBuffer::operator>>(char& chValue)
{
	GetData(&chValue, sizeof(chValue));
	return *this;
}

SerializedBuffer& SerializedBuffer::operator>>(short& shValue)
{
	GetData(reinterpret_cast<char*>(&shValue), sizeof(shValue));
	return *this;
}

SerializedBuffer& SerializedBuffer::operator>>(WORD& wValue)
{
	GetData(reinterpret_cast<char*>(&wValue), sizeof(wValue));
	return *this;
}

SerializedBuffer& SerializedBuffer::operator>>(int& iValue)
{
	GetData(reinterpret_cast<char*>(&iValue), sizeof(iValue));
	return *this;
}

SerializedBuffer& SerializedBuffer::operator>>(DWORD& dwValue)
{
	GetData(reinterpret_cast<char*>(&dwValue), sizeof(dwValue));
	return *this;
}

SerializedBuffer& SerializedBuffer::operator>>(float& fValue)
{
	GetData(reinterpret_cast<char*>(&fValue), sizeof(fValue));
	return *this;
}

SerializedBuffer& SerializedBuffer::operator>>(__int64& iValue)
{
	GetData(reinterpret_cast<char*>(&iValue), sizeof(iValue));
	return *this;
}

SerializedBuffer& SerializedBuffer::operator>>(double& dValue)
{
	GetData(reinterpret_cast<char*>(&dValue), sizeof(dValue));
	return *this;
}

// ======================= GetData / PutData =======================

int SerializedBuffer::GetData(char* chpDest, int iSize)
{
	if (iSize <= 0 || chpDest == nullptr)
		return 0;

	int copySize = min(iSize, mDataSize);
	if (copySize <= 0)
		return 0;

	std::memcpy(chpDest, mBuffer, copySize);

	int remain = mDataSize - copySize;
	if (remain > 0)
	{
		std::memmove(mBuffer,
			mBuffer + copySize,
			remain);
	}

	mDataSize -= copySize;
	return copySize;
}

int SerializedBuffer::PutData(char* chpSrc, int iSrcSize)
{
	if (iSrcSize <= 0 || chpSrc == nullptr)
		return 0;

	int freeSize = mBufferSize - mDataSize;
	if (iSrcSize > freeSize)
		return 0;

	int copySize = min(iSrcSize, freeSize);

	std::memcpy(mBuffer + mDataSize,
		chpSrc,
		copySize);

	mDataSize += copySize;
	return copySize;
}
