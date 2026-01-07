#include "Session.h"
#include "CRingBuffer.h" // 여기서 실제 내용을 포함
#include "NetConfig.h"   // 버퍼 사이즈 상수를 위해 포함
#include "Player.h"

Session::Session() : socket(INVALID_SOCKET), id(-1), isDisconnect(false)
{
	recvBuffer = new CRingBuffer(DEFAULT_BUFFER_SIZE);
	sendBuffer = new CRingBuffer(DEFAULT_BUFFER_SIZE);
}
Session::Session(SOCKET s, int id) :socket(s), id(id),isDisconnect(false)
{
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