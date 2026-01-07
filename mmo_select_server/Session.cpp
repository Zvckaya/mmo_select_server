#include "Session.h"
#include "CRingBuffer.h" // 여기서 실제 내용을 포함
#include "NetConfig.h"   // 버퍼 사이즈 상수를 위해 포함

Session::Session() : socket(INVALID_SOCKET), id(-1), isDisconnect(false)
{
	// 포인터이므로 직접 동적 할당 해야 함
	// 생성자에서 초기화
	recvBuffer = new CRingBuffer(DEFAULT_BUFFER_SIZE);
	sendBuffer = new CRingBuffer(DEFAULT_BUFFER_SIZE);
}

Session::~Session()
{
	// 소멸자에서 메모리 해제 필수
	if (recvBuffer != nullptr)
	{
		delete recvBuffer;
		recvBuffer = nullptr;
	}

	if (sendBuffer != nullptr)
	{
		delete sendBuffer;
		sendBuffer = nullptr;
	}
}