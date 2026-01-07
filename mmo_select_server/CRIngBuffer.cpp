#include "CRingBuffer.h"
#include <string.h> // memcpy
#include <algorithm>

// 기본 생성자: 기본 크기 10000 바이트로 설정 (필요에 따라 조절)
CRingBuffer::CRingBuffer(void)
	: m_pBuffer(nullptr), m_iBufferSize(0), m_iFront(0), m_iRear(0)
{
	// 초기 사이즈 설정, 보통 소켓 버퍼보다는 크게 잡아야 함
	Resize(10000);
}

// 생성자: 지정된 크기로 생성
CRingBuffer::CRingBuffer(int iBufferSize)
	: m_pBuffer(nullptr), m_iBufferSize(0), m_iFront(0), m_iRear(0)
{
	Resize(iBufferSize);
}

CRingBuffer::~CRingBuffer(void)
{
	if (m_pBuffer != nullptr)
	{
		delete[] m_pBuffer;
		m_pBuffer = nullptr;
	}
}

void CRingBuffer::Resize(int size)
{
	// 기존보다 작아지거나 같은 경우, 혹은 너무 작은 경우는 예외처리 정책 필요
	// 여기서는 단순히 크기 재할당 로직만 구현
	if (size <= 0) return;

	// 1. 새 버퍼 할당
	char* newBuffer = new char[size];

	// 2. 기존 데이터가 있다면 복사
	// (Resize시에는 데이터가 꼬이지 않게 정렬해서 복사하는 것이 관리상 편함)
	if (m_pBuffer != nullptr)
	{
		int useSize = GetUseSize();

		// 새 버퍼가 기존 데이터를 다 담지 못하면 잘리는 데이터 발생 (정책 결정 필요)
		// 여기서는 최대한 담을 수 있는 만큼만 복사
		int copySize = (std::min)(useSize, size - 1);

		if (copySize > 0)
		{
			Peek(newBuffer, copySize); // Peek을 이용해 순서대로 복사
		}

		// 인덱스 재정렬
		m_iFront = 0;
		m_iRear = copySize;

		delete[] m_pBuffer;
	}
	else
	{
		m_iFront = 0;
		m_iRear = 0;
	}

	m_pBuffer = newBuffer;
	m_iBufferSize = size;
}

int CRingBuffer::GetBufferSize(void)
{
	return m_iBufferSize;
}

int CRingBuffer::GetUseSize(void)
{
	if (m_iRear >= m_iFront)
	{
		return m_iRear - m_iFront;
	}
	else
	{
		// 뒤집힌 경우: (전체크기 - Front) + Rear
		return (m_iBufferSize - m_iFront) + m_iRear;
	}
}

int CRingBuffer::GetFreeSize(void)
{
	// 꽉 찬 상태(Front == Rear)와 빈 상태(Front == Rear)를 구분하기 위해 1바이트는 사용하지 않음
	int useSize = GetUseSize();
	return (m_iBufferSize - 1) - useSize;
}

int CRingBuffer::Enqueue(const char* chpData, int iSize)
{
	int freeSize = GetFreeSize();

	// 넣으려는 데이터가 남은 공간보다 크다면, 넣을 수 있는 만큼만 넣음
	if (iSize > freeSize)
	{
		iSize = freeSize;
	}

	if (iSize <= 0) return 0;

	// 1. Rear가 버퍼 끝까지 가는 공간 확인
	int linearFreeSize = m_iBufferSize - m_iRear;

	if (iSize <= linearFreeSize)
	{
		// 한 번에 복사 가능
		memcpy(&m_pBuffer[m_iRear], chpData, iSize);
	}
	else
	{
		// 끊어서 복사해야 함 (뒷부분 채우고 + 앞부분 채우기)
		memcpy(&m_pBuffer[m_iRear], chpData, linearFreeSize);
		memcpy(&m_pBuffer[0], chpData + linearFreeSize, iSize - linearFreeSize);
	}

	// Rear 이동 (원형으로)
	m_iRear = (m_iRear + iSize) % m_iBufferSize;

	return iSize;
}

int CRingBuffer::Dequeue(char* chpDest, int iSize)
{
	int useSize = GetUseSize();

	// 요청한 크기가 현재 데이터보다 크면, 있는 만큼만 줌
	if (iSize > useSize)
	{
		iSize = useSize;
	}

	if (iSize <= 0) return 0;

	// 1. Front부터 버퍼 끝까지의 데이터 크기 확인
	int linearUseSize = m_iBufferSize - m_iFront;

	if (iSize <= linearUseSize)
	{
		// 한 번에 읽기 가능
		memcpy(chpDest, &m_pBuffer[m_iFront], iSize);
	}
	else
	{
		// 끊어서 읽어야 함 (뒷부분 읽고 + 앞부분 읽기)
		memcpy(chpDest, &m_pBuffer[m_iFront], linearUseSize);
		memcpy(chpDest + linearUseSize, &m_pBuffer[0], iSize - linearUseSize);
	}

	// Front 이동 (원형으로)
	m_iFront = (m_iFront + iSize) % m_iBufferSize;

	return iSize;
}

int CRingBuffer::Peek(char* chpDest, int iSize)
{
	int useSize = GetUseSize();

	if (iSize > useSize)
	{
		iSize = useSize;
	}

	if (iSize <= 0) return 0;

	int linearUseSize = m_iBufferSize - m_iFront;

	if (iSize <= linearUseSize)
	{
		memcpy(chpDest, &m_pBuffer[m_iFront], iSize);
	}
	else
	{
		memcpy(chpDest, &m_pBuffer[m_iFront], linearUseSize);
		memcpy(chpDest + linearUseSize, &m_pBuffer[0], iSize - linearUseSize);
	}

	// Front 이동 없음 (단순 확인용)
	return iSize;
}

void CRingBuffer::ClearBuffer(void)
{
	m_iFront = 0;
	m_iRear = 0;
}

int CRingBuffer::DirectEnqueueSize(void)
{
	// Rear 위치부터 버퍼 끝까지 혹은 Front 바로 전까지 끊기지 않고 쓸 수 있는 길이
	// recv() 호출 시 buffer 파라미터로 넘겨줄 수 있는 최대 길이

	// 1. Rear가 Front보다 뒤에 있거나 같은 경우 (일반적인 경우 or 빈 경우)
	// [ ... Front ... Rear ... ] 
	// Rear부터 끝까지 쓸 수 있음. 단, Front가 0이면 끝까지 쓰면 안됨(1칸 비움)
	if (m_iRear >= m_iFront)
	{
		int size = m_iBufferSize - m_iRear;
		// 예외: Front가 0인 경우, 맨 마지막 바이트는 비워야 함 (Full 구분)
		if (m_iFront == 0)
		{
			return size - 1;
		}
		return size;
	}
	// 2. Rear가 Front보다 앞에 있는 경우 (한 바퀴 돔)
	// [ ... Rear ... Front ... ]
	else
	{
		// Rear부터 Front 바로 전까지 쓸 수 있음
		return (m_iFront - m_iRear) - 1;
	}
}

int CRingBuffer::DirectDequeueSize(void)
{
	// Front 위치부터 버퍼 끝까지 끊기지 않고 읽을 수 있는 길이
	// send() 호출 시 buffer 파라미터로 넘겨줄 수 있는 최대 길이

	// 1. Rear가 Front보다 뒤에 있는 경우
	// [ ... Front ... Rear ... ]
	if (m_iRear >= m_iFront)
	{
		return m_iRear - m_iFront;
	}
	// 2. Rear가 Front보다 앞에 있는 경우 (데이터가 끝을 넘어 앞까지 이어짐)
	// [ ... Rear ... Front ... ]
	else
	{
		// Front부터 버퍼 끝까지만 1차로 리턴
		return m_iBufferSize - m_iFront;
	}
}

int CRingBuffer::MoveRear(int iSize)
{
	// DirectEnqueue(recv) 후 실제 들어온 바이트 수 만큼 포인터 이동
	m_iRear = (m_iRear + iSize) % m_iBufferSize;
	return iSize;
}

int CRingBuffer::MoveFront(int iSize)
{
	// DirectDequeue(send) 후 실제 보낸 바이트 수 만큼 포인터 이동
	m_iFront = (m_iFront + iSize) % m_iBufferSize;
	return iSize;
}

char* CRingBuffer::GetFrontBufferPtr(void)
{
	return &m_pBuffer[m_iFront];
}

char* CRingBuffer::GetRearBufferPtr(void)
{
	return &m_pBuffer[m_iRear];
}