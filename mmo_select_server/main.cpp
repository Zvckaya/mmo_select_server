#include <iostream>
#include <WinSock2.h>
#include <Windows.h>
#include <mmsystem.h>
#include <vector>
#include <thread>
#include <chrono>
#include <algorithm>
#include <cstdlib>
#include <ctime>

#include "NetConfig.h"
#include "GameConfig.h"
#include "Session.h"
#include "CRingBuffer.h"
#include "Protocol.h"
#include "PacketProc.h"
#include "Update.h"
#include "Logger.h"

#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "Ws2_32.lib")

bool bServerFlag = true;
int playercnt = 1;
WSADATA wsaData;
SOCKET sListenSocket;
linger _linger;

fd_set listenSet;
fd_set readSet;
fd_set writeSet;

std::vector<Session*> sessions;


bool ServerInitailize()
{
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		Logger::get_instance().log(Logger::LogLevel::Error, "WSAStartup failed (WSAError=%d)", WSAGetLastError());
		return false;
	}

	sListenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sListenSocket == INVALID_SOCKET)
	{
		Logger::get_instance().log(Logger::LogLevel::Error, "listen socket creation failed (WSAError=%d)", WSAGetLastError());
		return false;
	}

	sockaddr_in serverAddr;
	serverAddr.sin_family      = AF_INET;
	serverAddr.sin_port        = htons(PORT);
	serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(sListenSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
	{
		Logger::get_instance().log(Logger::LogLevel::Error, "bind failed (port=%d, WSAError=%d)", (int)PORT, WSAGetLastError());
		return false;
	}

	_linger.l_onoff  = 1;
	_linger.l_linger = 0;

	Logger::get_instance().log(Logger::LogLevel::Info, "Server initialized");
	return true;
}

bool StartServer()
{
	if (listen(sListenSocket, SOMAXCONN_HINT(200, 65535)) == SOCKET_ERROR)
	{
		Logger::get_instance().log(Logger::LogLevel::Error, "listen failed (WSAError=%d)", WSAGetLastError());
		return false;
	}

	u_long mode = 1;
	ioctlsocket(sListenSocket, FIONBIO, &mode);

	Logger::get_instance().log(Logger::LogLevel::Info, "Server listening on port %d", (int)PORT);
	return true;
}

//------------------------------------------------------------------
// 지연 삭제: 한 프레임(NetworkProc + Update) 이 모두 끝난 뒤에만 호출
// - 루프 도중 isDisconnect=true 로 마킹만 해두고
// - 여기서 DELETE 브로드캐스트, 섹터 제거, 메모리 해제를 일괄 처리
//------------------------------------------------------------------
static void CleanupDeadSessions()
{
	for (auto* s : sessions)
	{
		if (s->isDisconnect && !s->notified)
			OnSessionDisconnected(s);
	}

	sessions.erase(
		std::remove_if(sessions.begin(), sessions.end(), [](Session* s) {
			if (s->isDisconnect) { delete s; return true; }
			return false;
		}),
		sessions.end()
	);
}

void NetworkProc()
{
	//------------------------------------------------------------------
	// 대기 중인 연결 모두 수락
	//------------------------------------------------------------------
	while (true)
	{
		sockaddr_in clientAddr;
		int addrLen = sizeof(clientAddr);
		SOCKET clientSocket = accept(sListenSocket, (SOCKADDR*)&clientAddr, &addrLen);

		if (clientSocket == INVALID_SOCKET)
		{
			// WSAEWOULDBLOCK: 대기 중인 연결 없음 (정상)
			// 그 외 에러: 리슨 소켓 이상 → 로그만 남기고 루프 탈출
			if (WSAGetLastError() != WSAEWOULDBLOCK)
				Logger::get_instance().log(Logger::LogLevel::Error,
					"accept failed (WSAError=%d)", WSAGetLastError());
			break;
		}

		if (sessions.size() >= MAX_SESSIONS)
		{
			Logger::get_instance().log(Logger::LogLevel::Warning,
				"Session limit reached (%zu/%d), rejected new connection", sessions.size(), (int)MAX_SESSIONS);
			HardClose(clientSocket);
			continue;
		}

		u_long mode = 1;
		ioctlsocket(clientSocket, FIONBIO, &mode);

		Session* newSession = new Session(clientSocket, playercnt);
		playercnt++;

		sessions.push_back(newSession);
		OnSessionConnected(newSession);
	}

	//------------------------------------------------------------------
	// 세션 I/O 처리 (64개 단위 select)
	// ※ 이 루프 안에서는 isDisconnect 마킹과 closesocket 만 수행
	//   OnSessionDisconnected / delete 는 CleanupDeadSessions() 에서 처리
	//------------------------------------------------------------------
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

			if (s->isDisconnect)
				continue;

			FD_SET(s->socket, &readSet);

			if (s->sendBuffer->GetUseSize() > 0)
				FD_SET(s->socket, &writeSet);

			count++;
		}

		if (count == 0)
			continue;

		timeval tv = { 0, 0 };
		int ret = select(0, &readSet, &writeSet, nullptr, &tv);

		if (ret <= 0)
			continue;

		for (int j = 0; j < 64; ++j)
		{
			if (i + j >= totalSessions)
				break;

			Session* s = sessions[i + j];

			if (s->isDisconnect)
				continue;

			//------------------------------------------------------
			// recv
			//------------------------------------------------------
			if (FD_ISSET(s->socket, &readSet))
			{
				int freeSize = s->recvBuffer->DirectEnqueueSize();
				if (freeSize > 0)
				{
					int recvBytes = recv(s->socket, s->recvBuffer->GetRearBufferPtr(), freeSize, 0);
					if (recvBytes == 0)
					{
						// 클라이언트 정상 종료 (FIN)
						Logger::get_instance().log(Logger::LogLevel::Info,
							"Session %d: client closed connection (FIN) -> disconnect", s->id);
						s->isDisconnect = true;
						HardClose(s->socket);
						continue;
					}
					if (recvBytes == SOCKET_ERROR)
					{
						if (WSAGetLastError() != WSAEWOULDBLOCK)
						{
							Logger::get_instance().log(Logger::LogLevel::Warning,
								"Session %d: recv error (WSAError=%d) -> disconnect",
								s->id, WSAGetLastError());
							s->isDisconnect = true;
							HardClose(s->socket);
						}
						continue;
					}

					s->recvBuffer->MoveRear(recvBytes);

					// 패킷 파싱 루프: 완성된 패킷이 있는 동안 처리
					while (s->recvBuffer->GetUseSize() >= (int)sizeof(st_PACKET_HEADER))
					{
						st_PACKET_HEADER header;
						s->recvBuffer->Peek((char*)&header, sizeof(st_PACKET_HEADER));

						// 패킷 코드 불일치 → 비정상 클라이언트
						if (header.byCode != dfPACKET_CODE)
						{
							Logger::get_instance().log(Logger::LogLevel::Warning,
								"Session %d: invalid packet code (0x%02X, expected 0x%02X) -> disconnect",
								s->id, (int)header.byCode, (int)dfPACKET_CODE);
							s->isDisconnect = true;
							HardClose(s->socket);
							break;
						}

						// 페이로드가 아직 덜 왔으면 다음 recv 대기
						if (s->recvBuffer->GetUseSize() < (int)(sizeof(st_PACKET_HEADER) + header.bySize))
							break;

						// 헤더 소비
						s->recvBuffer->Dequeue((char*)&header, sizeof(st_PACKET_HEADER));

						// 페이로드 소비
						char payload[256] = {};
						if (header.bySize > 0)
							s->recvBuffer->Dequeue(payload, header.bySize);

						// 패킷 처리 (false = 알 수 없는 타입 → 마킹)
						if (!PacketProc(s, header.byType, payload, header.bySize))
						{
							s->isDisconnect = true;
							HardClose(s->socket);
							break;
						}
					}
				}
			}

			//------------------------------------------------------
			// send
			//------------------------------------------------------
			if (!s->isDisconnect && FD_ISSET(s->socket, &writeSet))
			{
				int sendSize = s->sendBuffer->DirectDequeueSize();
				if (sendSize > 0)
				{
					int sentBytes = send(s->socket, s->sendBuffer->GetFrontBufferPtr(), sendSize, 0);
					if (sentBytes == SOCKET_ERROR)
					{
						// WSAEWOULDBLOCK: OS TCP 버퍼 포화, 다음 프레임에 재시도 (정상)
						if (WSAGetLastError() != WSAEWOULDBLOCK)
						{
							Logger::get_instance().log(Logger::LogLevel::Warning,
								"Session %d: send error (WSAError=%d) -> disconnect",
								s->id, WSAGetLastError());
							s->isDisconnect = true;
							HardClose(s->socket);
						}
					}
					else
					{
						s->sendBuffer->MoveFront(sentBytes);
					}
				}
			}
		}
	}
}


static void InitLogger()
{
	// 시작 시각으로 파일명 생성: logs/YYYY-MM-DD_HH-MM-SS.log
	std::time_t now = std::time(nullptr);
	std::tm     tm  = {};
	localtime_s(&tm, &now);

	::CreateDirectoryA("logs", nullptr);  // 이미 존재하면 무시됨

	char filename[64];
	std::strftime(filename, sizeof(filename), "logs/%Y-%m-%d_%H-%M-%S.log", &tm);

	Logger& logger = Logger::get_instance();
	logger.set_threshold(Logger::LogLevel::Warning);
	logger.set_backend(Logger::SinkBackend::WINDOWS);
	logger.set_target_file(filename);
}

int main()
{
	srand((unsigned)time(nullptr));
	timeBeginPeriod(1);

	InitLogger();

	if (!ServerInitailize())
	{
		std::cerr << "Server Init Failed" << std::endl;
		return 0;
	}
	if (!StartServer())
	{
		std::cerr << "Server Start Failed" << std::endl;
	}

	while (bServerFlag)
	{
		NetworkProc();
		Update();
		CleanupDeadSessions();   // 프레임 끝에서 일괄 정리
		std::this_thread::sleep_for(std::chrono::milliseconds(SERVER_FRAME_MS));
	}

	timeEndPeriod(1);
}
