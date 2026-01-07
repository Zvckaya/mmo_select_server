#include <iostream>
#include <WinSock2.h>
#include <Windows.h>
#include <mmsystem.h>
#include <vector>

#include "NetConfig.h"
#include "Session.h"      // Session 정의
#include "CRingBuffer.h"  // main에서 버퍼 함수들을 쓰려면 여기도 필요할 수 있음

#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "Ws2_32.lib")

bool bServerFlag = true;
WSADATA wsaData;
SOCKET sListenSocket;
linger _linger; 

fd_set listenSet;
fd_set readSet;
fd_set writeSet;

std::vector<Session*> sessions;


bool serverInitailize()
{
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		std::cerr << "서버 초기화 실패";
		return false;
	}

	sListenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sListenSocket == INVALID_SOCKET)
	{
		std::cerr << "리슨소켓 초기화 실패";
		return false;
	}

	sockaddr_in serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(PORT);
	serverAddr.sin_addr.s_addr = htonl(INADDR_ANY); //모든 NIC 카드 i/o 감지

	if (bind(sListenSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
	{
		std::cerr << "바인딩 실패";
		return false;
	}

	_linger.l_onoff = 1; //링거 구조체 초기화
	_linger.l_linger = 0;


	std::cout << "서버 초기화 성공\n";
	return true;
}

bool startServer()
{
	if (listen(sListenSocket, SOMAXCONN_HINT(200,65535))==SOCKET_ERROR) //절절한 값으로 조절 
	{
		std::cerr << "리스닝 실패";
		return false;
	}

	u_long mode = 1;
	ioctlsocket(sListenSocket, FIONBIO, &mode);

	std::cout << "리스닝...PORT:" << PORT << "\n";
	return true;

}

void NetworkProc()
{
	{
		FD_ZERO(&listenSet);
		FD_SET(sListenSocket, &listenSet);

		timeval tv = { 0,0 };

		int ret = select(0, &listenSet, nullptr, nullptr, &tv);
		

		if (ret > 0)
		{
			sockaddr_in clientAddr;
			int addrLen = sizeof(clientAddr);
			SOCKET clientSocket = accept(sListenSocket, (SOCKADDR*)&clientAddr,&addrLen);


			if (clientSocket != INVALID_SOCKET)
			{
				std::cerr << "소켓 연결에러 ";
			}
			
			if (sessions.size() >= MAX_SESSIONS)
			{
				setsockopt(clientSocket, SOL_SOCKET, SO_LINGER, (char*)&_linger, sizeof(_linger));
				closesocket(clientSocket);
				return;
			}
		
			u_long mode = 1;
			ioctlsocket(clientSocket, FIONBIO, &mode);

			Session* newSession = new Session(clientSocket,playercnt);
			playercnt++;

			sessions.push_back(newSession);

		}
	}

	int totalSessions = (int)sessions.size();

	for (int i = 0; i < totalSessions; i += 64)
	{
		FD_ZERO(&readSet);
		FD_ZERO(&writeSet);

		int count = 0;

		for (int j = 0; j < 64; ++j)
		{
			if (i + j >= totalSessions)
				break;

			Session* s = sessions[i + j];

			if (s->isDisconnect == true)
				continue;

			FD_SET(s->socket, &readSet);

			if (s->sendBuffer->GetUseSize() > 0)
			{
				FD_SET(s->socket, &writeSet);
			}
			count++;
		}

		if (count == 0)
			continue;

		timeval tv = { 0,0 };

		int ret = select(0, &readSet, &writeSet, nullptr,&tv);
	}
}



int main()
{
	timeBeginPeriod(1);

	if (serverInitailize()==false)
	{
		std::cerr << "서버 초기화 실패"<<std::endl;
		return 0;
	}

	if (startServer()==false)
	{
		std::cerr << "리스닝 실패" << std::endl;
	}
	
	

	while (bServerFlag)
	{

	}


	timeEndPeriod(1);
}