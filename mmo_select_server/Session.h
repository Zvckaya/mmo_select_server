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

	int   id;
	ULONGLONG lastRecvTime = 0;
	bool  notified = false;   // OnSessionDisconnected 호출 완료 여부

	CRingBuffer* recvBuffer;
	CRingBuffer* sendBuffer;
};