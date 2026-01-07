#pragma once
#include <WinSock2.h>

class CRingBuffer;
class Player;

class Session
{
public:
	Session();
	Session(SOCKET s, int id);
	~Session();

	SOCKET socket;
	Player* p;
	bool isDisconnect = false;

	int id;
	CRingBuffer* recvBuffer;
	CRingBuffer* sendBuffer;

	
};