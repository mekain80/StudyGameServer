#include "CPacket.h"
#include <cstring>   // memcpy, memmove
#include <algorithm> // min

/// 패킷 청소
void CPacket::Clear(void)
{
    m_iDataSize = 0;
}

/// 버퍼 Pos 이동 (쓰기 위치 이동)
/// 현재 데이터 끝에서 iSize 만큼 "쓴 걸로" 간주.
/// 실제로 버퍼에 뭘 쓰는 건 외부에서 하고,
/// 여기서는 데이터 크기만 늘려준다.
int CPacket::MoveWritePos(int iSize)
{
    if (iSize <= 0)
        return 0;

    // 남은 공간보다 더 많이 이동하라고 하면 남은 만큼만
    int freeSize = m_iBufferSize - m_iDataSize;
    int moveSize = min(iSize, freeSize);

    m_iDataSize += moveSize;
    return moveSize;
}

/// 버퍼 Pos 이동 (읽기 위치 이동)
/// 앞에서 iSize 만큼 "읽어버린 것처럼" 처리.
/// 남은 데이터는 앞쪽으로 땡긴다.
int CPacket::MoveReadPos(int iSize)
{
    if (iSize <= 0)
        return 0;

    int moveSize = min(iSize, m_iDataSize);
    if (moveSize <= 0)
        return 0;

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

/// 대입 연산자
CPacket& CPacket::operator=(CPacket& clSrcPacket)
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

CPacket& CPacket::operator<<(unsigned char byValue)
{
    PutData(reinterpret_cast<char*>(&byValue), sizeof(byValue));
    return *this;
}

CPacket& CPacket::operator<<(char chValue)
{
    PutData(&chValue, sizeof(chValue));
    return *this;
}

CPacket& CPacket::operator<<(short shValue)
{
    PutData(reinterpret_cast<char*>(&shValue), sizeof(shValue));
    return *this;
}

CPacket& CPacket::operator<<(unsigned short wValue)
{
    PutData(reinterpret_cast<char*>(&wValue), sizeof(wValue));
    return *this;
}

CPacket& CPacket::operator<<(int iValue)
{
    PutData(reinterpret_cast<char*>(&iValue), sizeof(iValue));
    return *this;
}

CPacket& CPacket::operator<<(long lValue)
{
    PutData(reinterpret_cast<char*>(&lValue), sizeof(lValue));
    return *this;
}

CPacket& CPacket::operator<<(float fValue)
{
    PutData(reinterpret_cast<char*>(&fValue), sizeof(fValue));
    return *this;
}

CPacket& CPacket::operator<<(__int64 iValue)
{
    PutData(reinterpret_cast<char*>(&iValue), sizeof(iValue));
    return *this;
}

CPacket& CPacket::operator<<(double dValue)
{
    PutData(reinterpret_cast<char*>(&dValue), sizeof(dValue));
    return *this;
}

// ======================= Get 연산자 (>>) =======================

CPacket& CPacket::operator>>(BYTE& byValue)
{
    GetData(reinterpret_cast<char*>(&byValue), sizeof(byValue));
    return *this;
}

CPacket& CPacket::operator>>(char& chValue)
{
    GetData(&chValue, sizeof(chValue));
    return *this;
}

CPacket& CPacket::operator>>(short& shValue)
{
    GetData(reinterpret_cast<char*>(&shValue), sizeof(shValue));
    return *this;
}

CPacket& CPacket::operator>>(WORD& wValue)
{
    GetData(reinterpret_cast<char*>(&wValue), sizeof(wValue));
    return *this;
}

CPacket& CPacket::operator>>(int& iValue)
{
    GetData(reinterpret_cast<char*>(&iValue), sizeof(iValue));
    return *this;
}

CPacket& CPacket::operator>>(DWORD& dwValue)
{
    GetData(reinterpret_cast<char*>(&dwValue), sizeof(dwValue));
    return *this;
}

CPacket& CPacket::operator>>(float& fValue)
{
    GetData(reinterpret_cast<char*>(&fValue), sizeof(fValue));
    return *this;
}

CPacket& CPacket::operator>>(__int64& iValue)
{
    GetData(reinterpret_cast<char*>(&iValue), sizeof(iValue));
    return *this;
}

CPacket& CPacket::operator>>(double& dValue)
{
    GetData(reinterpret_cast<char*>(&dValue), sizeof(dValue));
    return *this;
}

// ======================= GetData / PutData =======================

int CPacket::GetData(char* chpDest, int iSize)
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

int CPacket::PutData(char* chpSrc, int iSrcSize)
{
    if (iSrcSize <= 0 || chpSrc == nullptr)
        return 0;

    int freeSize = m_iBufferSize - m_iDataSize;
    if (freeSize <= 0)
        return 0;

    int copySize = min(iSrcSize, freeSize);

    std::memcpy(m_chpBuffer + m_iDataSize,
        chpSrc,
        copySize);

    m_iDataSize += copySize;
    return copySize;
}
