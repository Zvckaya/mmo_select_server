#include <iostream>
#include <WinSock2.h>
#include <Windows.h>
#include <mmsystem.h>

#include "NetConfig.h"
#include "Session.h"      // Session 정의
#include "CRingBuffer.h"  // main에서 버퍼 함수들을 쓰려면 여기도 필요할 수 있음

#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "Ws2_32.lib")

bool bServerFlag = true;
WSADATA wsaData;
SOCKET sListenSocket;
linger _linger; 

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