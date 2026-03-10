#include "stdafx.h"

#include "Packet.h"
#include <cstring>   // memcpy, memmove
#include <algorithm> // min

Packet::Packet()
    : m_iBufferSize(eBUFFER_DEFAULT),
    m_iDataSize(0)
{
    m_chpBuffer = new char[m_iBufferSize];
}


Packet::Packet(int iBUfferSize)
    : m_iBufferSize(iBUfferSize),
    m_iDataSize(0)
{
    m_chpBuffer = new char[m_iBufferSize];
}

Packet::~Packet()
{
    delete[] m_chpBuffer;
}

// 패킷 청소
void Packet::Clear(void)
{
    m_iDataSize = 0;
}

int Packet::MoveWritePos(int iSize)
{
    if (iSize <= 0)
        return 0;

    // 남은 공간보다 더 많이 이동하라고 하면 하지 않음
    int freeSize = m_iBufferSize - m_iDataSize;
    if (freeSize < iSize)
        return 0;

    m_iDataSize += iSize;
    return iSize;
}

int Packet::MoveReadPos(int iSize)
{
    if (iSize <= 0)
        return 0;

    int moveSize = min(iSize, m_iDataSize);

    // 사용된 데이터 앞으로 땡기기
    int remain = m_iDataSize - moveSize;
    if (remain > 0)
    {
        std::memmove(m_chpBuffer,
            m_chpBuffer + moveSize,
            remain);
    }

    m_iDataSize -= moveSize;
    return moveSize;
}

// 대입 연산자
Packet& Packet::operator=(Packet& clSrcPacket)
{
    if (this == &clSrcPacket)
        return *this;

    // 버퍼 크기가 다르면 다시 할당
    if (m_iBufferSize != clSrcPacket.m_iBufferSize)
    {
        delete[] m_chpBuffer;
        m_iBufferSize = clSrcPacket.m_iBufferSize;
        m_chpBuffer = new char[m_iBufferSize];
    }

    m_iDataSize = clSrcPacket.m_iDataSize;

    // 실제 들어있는 데이터만 복사
    if (m_iDataSize > 0)
    {
        std::memcpy(m_chpBuffer,
            clSrcPacket.m_chpBuffer,
            m_iDataSize);
    }

    return *this;
}

// ======================= Put 연산자 (<<) =======================

Packet& Packet::operator<<(unsigned char byValue)
{
    PutData(reinterpret_cast<char*>(&byValue), sizeof(byValue));
    return *this;
}

Packet& Packet::operator<<(char chValue)
{
    PutData(&chValue, sizeof(chValue));
    return *this;
}

Packet& Packet::operator<<(short shValue)
{
    PutData(reinterpret_cast<char*>(&shValue), sizeof(shValue));
    return *this;
}

Packet& Packet::operator<<(unsigned short wValue)
{
    PutData(reinterpret_cast<char*>(&wValue), sizeof(wValue));
    return *this;
}

Packet& Packet::operator<<(int iValue)
{
    PutData(reinterpret_cast<char*>(&iValue), sizeof(iValue));
    return *this;
}

Packet& Packet::operator<<(long lValue)
{
    PutData(reinterpret_cast<char*>(&lValue), sizeof(lValue));
    return *this;
}

Packet& Packet::operator<<(float fValue)
{
    PutData(reinterpret_cast<char*>(&fValue), sizeof(fValue));
    return *this;
}

Packet& Packet::operator<<(__int64 iValue)
{
    PutData(reinterpret_cast<char*>(&iValue), sizeof(iValue));
    return *this;
}

Packet& Packet::operator<<(double dValue)
{
    PutData(reinterpret_cast<char*>(&dValue), sizeof(dValue));
    return *this;
}

// ======================= Get 연산자 (>>) =======================

Packet& Packet::operator>>(BYTE& byValue)
{
    GetData(reinterpret_cast<char*>(&byValue), sizeof(byValue));
    return *this;
}

Packet& Packet::operator>>(char& chValue)
{
    GetData(&chValue, sizeof(chValue));
    return *this;
}

Packet& Packet::operator>>(short& shValue)
{
    GetData(reinterpret_cast<char*>(&shValue), sizeof(shValue));
    return *this;
}

Packet& Packet::operator>>(WORD& wValue)
{
    GetData(reinterpret_cast<char*>(&wValue), sizeof(wValue));
    return *this;
}

Packet& Packet::operator>>(int& iValue)
{
    GetData(reinterpret_cast<char*>(&iValue), sizeof(iValue));
    return *this;
}

Packet& Packet::operator>>(DWORD& dwValue)
{
    GetData(reinterpret_cast<char*>(&dwValue), sizeof(dwValue));
    return *this;
}

Packet& Packet::operator>>(float& fValue)
{
    GetData(reinterpret_cast<char*>(&fValue), sizeof(fValue));
    return *this;
}

Packet& Packet::operator>>(__int64& iValue)
{
    GetData(reinterpret_cast<char*>(&iValue), sizeof(iValue));
    return *this;
}

Packet& Packet::operator>>(double& dValue)
{
    GetData(reinterpret_cast<char*>(&dValue), sizeof(dValue));
    return *this;
}

// ======================= GetData / PutData =======================

int Packet::GetData(char* chpDest, int iSize)
{
    if (iSize <= 0 || chpDest == nullptr)
        return 0;

    int copySize = min(iSize, m_iDataSize);
    if (copySize <= 0)
        return 0;

    std::memcpy(chpDest, m_chpBuffer, copySize);

    int remain = m_iDataSize - copySize;
    if (remain > 0)
    {
        std::memmove(m_chpBuffer,
            m_chpBuffer + copySize,
            remain);
    }

    m_iDataSize -= copySize;
    return copySize;
}

int Packet::PutData(char* chpSrc, int iSrcSize)
{
    if (iSrcSize <= 0 || chpSrc == nullptr)
        return 0;

    int freeSize = m_iBufferSize - m_iDataSize;
    if (iSrcSize > freeSize)
        return 0;

    int copySize = min(iSrcSize, freeSize);

    std::memcpy(m_chpBuffer + m_iDataSize,
        chpSrc,
        copySize);

    m_iDataSize += copySize;
    return copySize;
}
