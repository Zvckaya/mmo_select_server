#pragma once
#include <WinSock2.h>

class CRingBuffer;


class Session
{
	Session();
	~Session();
	SOCKET socket;
	int id;

	CRingBuffer* recvBuffer;
	CRingBuffer* sendBuffer;

	bool isDisconnect = false;
	
};