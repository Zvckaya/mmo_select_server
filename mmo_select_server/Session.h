#pragma once
#include <WinSock2.h>

class CRingBuffer;


struct Session
{
	SOCKET socket;
	int id;

	CRingBuffer* recvBuffer;
	CRingBuffer* sendBuffer;

	bool isDisconnect = false;
	
};