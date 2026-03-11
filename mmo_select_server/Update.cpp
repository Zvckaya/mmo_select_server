#include "Update.h"
#include "NetConfig.h"
#include "GameConfig.h"
#include "Session.h"
#include "Player.h"
#include "PacketProc.h"
#include "Sector.h"
#include <Windows.h>
#include <vector>

extern std::vector<Session*> sessions;

//---------------------------------------------------------------
// 이동 후 맵 경계 내에 있는지 검사
//---------------------------------------------------------------
static bool CharacterMoveCheck(short x, short y)
{
	return (x >= dfRANGE_MOVE_LEFT  &&
	        x <= dfRANGE_MOVE_RIGHT &&
	        y >= dfRANGE_MOVE_TOP   &&
	        y <= dfRANGE_MOVE_BOTTOM);
}

//---------------------------------------------------------------
// 이동 액션 → 새 좌표 계산 및 경계 체크 적용
//---------------------------------------------------------------
static void ProcessMovement(Player* p)
{
	short newX = p->_x;
	short newY = p->_y;

	switch (p->_action)
	{
	case dfACTION_MOVE_LL: newX -= dfSPEED_PLAYER_X;                                   break;
	case dfACTION_MOVE_LU: newX -= dfSPEED_PLAYER_X; newY -= dfSPEED_PLAYER_Y;        break;
	case dfACTION_MOVE_UU:                            newY -= dfSPEED_PLAYER_Y;        break;
	case dfACTION_MOVE_RU: newX += dfSPEED_PLAYER_X; newY -= dfSPEED_PLAYER_Y;        break;
	case dfACTION_MOVE_RR: newX += dfSPEED_PLAYER_X;                                   break;
	case dfACTION_MOVE_RD: newX += dfSPEED_PLAYER_X; newY += dfSPEED_PLAYER_Y;        break;
	case dfACTION_MOVE_DD:                            newY += dfSPEED_PLAYER_Y;        break;
	case dfACTION_MOVE_LD: newX -= dfSPEED_PLAYER_X; newY += dfSPEED_PLAYER_Y;        break;
	default: return;
	}

	if (CharacterMoveCheck(newX, newY))
	{
		p->_x = newX;
		p->_y = newY;
	}
}

//---------------------------------------------------------------
// Update - 50fps 자체 제어
//---------------------------------------------------------------
void Update()
{
	static ULONGLONG dwPrevTick = GetTickCount64();
	ULONGLONG dwCurTick = GetTickCount64();

	// 프레임 시간이 아직 안 됐으면 즉시 반환
	if (dwCurTick - dwPrevTick < (ULONGLONG)UPDATE_FRAME_MS)
		return;

	// 누적 오차 방지: 실제 경과 시간이 아닌 고정 간격으로 전진
	dwPrevTick += UPDATE_FRAME_MS;

	//----------------------------------------------------------
	// 전체 세션 순회
	//----------------------------------------------------------
	for (auto* s : sessions)
	{
		if (s->isDisconnect)
			continue;

		Player* p = s->p;

		//------------------------------------------------------
		// 1. HP <= 0 → 사망 처리
		//------------------------------------------------------
		if (p->_hp <= 0)
		{
			s->isDisconnect = true;
			HardClose(s->socket);
			continue;
		}

		//------------------------------------------------------
		// 2. 수신 타임아웃 (30초)
		//------------------------------------------------------
		if (dwCurTick - s->lastRecvTime > dfNETWORK_PACKET_RECV_TIMEOUT)
		{
			s->isDisconnect = true;
			HardClose(s->socket);
			continue;
		}

		//------------------------------------------------------
		// 3. 이동 처리
		//------------------------------------------------------
		if (p->_action != PLAYER_ACTION_NONE)
		{
			ProcessMovement(p);

			// 이동 후 섹터 변경 여부 확인 및 CREATE/DELETE 전송
			if (Sector_UpdateSession(s))
				CharacterSectorUpdatePacket(s);
		}
	}
}
