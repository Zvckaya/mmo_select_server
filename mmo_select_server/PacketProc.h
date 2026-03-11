#pragma once
#include <WinSock2.h>
#include <vector>
#include "Session.h"
#include "Protocol.h"
#include "GameConfig.h"

extern std::vector<Session*> sessions;

// SO_LINGER(l_onoff=1, l_linger=0) 적용 후 즉시 RST 종료 (TIME_WAIT 방지)
inline void HardClose(SOCKET s)
{
	linger lg{ 1, 0 };
	setsockopt(s, SOL_SOCKET, SO_LINGER, (char*)&lg, sizeof(lg));
	closesocket(s);
}

//---------------------------------------------------------------
// 메인 패킷 디스패처
// false 반환 시 호출 측에서 세션을 끊어야 함
//---------------------------------------------------------------
bool PacketProc(Session* s, BYTE byType, const char* payload, BYTE payloadSize);

//---------------------------------------------------------------
// 패킷 전송 유틸리티
//---------------------------------------------------------------
void SendPacket(Session* s, BYTE byType, const char* payload, BYTE payloadSize);

// 섹터 도입 전: 전체 브로드캐스트 (이후 SendPacket_Around로 교체 예정)
void SendPacket_Around(Session* pMe, BYTE byType, const char* payload, BYTE payloadSize, bool bSendMe = false);

//---------------------------------------------------------------
// 접속 / 해제 처리
//---------------------------------------------------------------
void OnSessionConnected(Session* newSession);
void OnSessionDisconnected(Session* s);
