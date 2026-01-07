#pragma once


class CRingBuffer {
public:
	CRingBuffer(void);
	CRingBuffer(int iBufferSize);
	~CRingBuffer(void);

	// 버퍼 크기 재조정 (기존 데이터 유지)
	void	Resize(int size);

	// 전체 버퍼 크기 (메모리 할당 크기)
	int		GetBufferSize(void);

	// 현재 사용중인(채워진) 용량 얻기
	int		GetUseSize(void);

	// 현재 남은(비어있는) 용량 얻기
	int		GetFreeSize(void);

	// 데이터 넣기 (일반적인 복사 방식)
	int		Enqueue(const char* chpData, int iSize);

	// 데이터 꺼내기 (복사 후 포인터 이동)
	int		Dequeue(char* chpDest, int iSize);

	// 데이터 확인하기 (포인터 이동 없음)
	int		Peek(char* chpDest, int iSize);

	// 버퍼 초기화
	void	ClearBuffer(void);

	// --- 직접 접근(Direct Access)을 위한 메서드 ---
	// recv()나 send() 시 메모리 복사 없이 소켓 버퍼로 직접 사용하기 위함

	// 끊기지 않고 한 번에 쓸 수 있는 최대 길이 (recv용)
	int		DirectEnqueueSize(void);

	// 끊기지 않고 한 번에 읽을 수 있는 최대 길이 (send용)
	int		DirectDequeueSize(void);

	// 쓰기 포인터 이동 (DirectEnqueue 후 호출)
	int		MoveRear(int iSize);

	// 읽기 포인터 이동 (DirectDequeue 후 호출)
	int		MoveFront(int iSize);

	// 현재 읽기 위치 포인터 (send에 사용)
	char* GetFrontBufferPtr(void);

	// 현재 쓰기 위치 포인터 (recv에 사용)
	char* GetRearBufferPtr(void);

private:
	char* m_pBuffer;		// 실제 버퍼 포인터
	int		m_iBufferSize;	// 전체 할당된 크기
	int		m_iFront;		// 읽기 인덱스 (데이터의 시작)
	int		m_iRear;		// 쓰기 인덱스 (데이터의 끝, 다음 들어갈 위치)

	// 링버퍼는 꽉 찬 상태와 빈 상태 구분을 위해 1바이트를 더미로 남겨두는 방식 사용
	// 실제 저장 가능 용량 = m_iBufferSize - 1
};