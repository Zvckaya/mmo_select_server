#include "Session.h"
#include "CRingBuffer.h"
#include "NetConfig.h"
#include "GameConfig.h"
#include "Player.h"
#include <Windows.h>
#include <cstdlib>

Session::Session() : socket(INVALID_SOCKET), id(-1), isDisconnect(false), lastRecvTime(0)
{
	recvBuffer = new CRingBuffer(DEFAULT_BUFFER_SIZE);
	sendBuffer = new CRingBuffer(DEFAULT_BUFFER_SIZE);
	p = new Player();
}

Session::Session(SOCKET s, int id) : socket(s), id(id), isDisconnect(false), lastRecvTime(GetTickCount64())
{
	recvBuffer = new CRingBuffer(DEFAULT_BUFFER_SIZE);
	sendBuffer = new CRingBuffer(DEFAULT_BUFFER_SIZE);
	short x = (short)(rand() % (dfRANGE_MOVE_RIGHT - dfRANGE_MOVE_LEFT + 1) + dfRANGE_MOVE_LEFT);
	short y = (short)(rand() % (dfRANGE_MOVE_BOTTOM - dfRANGE_MOVE_TOP + 1) + dfRANGE_MOVE_TOP);
	p = new Player(x, y);
}

Session::~Session()
{
	if (p != nullptr)
	{
		delete p;
		p = nullptr;
	}

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
