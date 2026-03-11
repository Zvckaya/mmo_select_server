#include "PacketProc.h"
#include "Sector.h"
#include "Player.h"
#include "CRingBuffer.h"
#include <Windows.h>
#include <cstring>
#include <cstdlib>
#include <cmath>

//=================================================================
// 페이로드 직렬화 / 역직렬화 헬퍼
//=================================================================
static BYTE  unpackByte (const char* buf, int& off) { BYTE  v = (BYTE)(unsigned char)buf[off]; off += 1; return v; }
static short unpackShort(const char* buf, int& off) { short v; memcpy(&v, buf + off, 2); off += 2; return v; }
static DWORD unpackDword(const char* buf, int& off) { DWORD v; memcpy(&v, buf + off, 4); off += 4; return v; }

static int packByte (char* buf, int off, BYTE  v) { buf[off] = (char)v; return off + 1; }
static int packShort(char* buf, int off, short v) { memcpy(buf + off, &v, 2); return off + 2; }
static int packInt  (char* buf, int off, int   v) { memcpy(buf + off, &v, 4); return off + 4; }

//=================================================================
// 패킷 전송
//=================================================================

void SendPacket(Session* s, BYTE byType, const char* payload, BYTE payloadSize)
{
	// 이미 끊긴 세션에는 쓰지 않음
	if (s->isDisconnect) return;

	// 헤더+페이로드를 한 번에 넣을 공간이 없으면 패킷 드랍
	// (끊으면 DELETE 브로드캐스트 → 연쇄 오버플로 → 대량 강퇴 cascade 발생)
	int needed = (int)sizeof(st_PACKET_HEADER) + payloadSize;
	if (s->sendBuffer->GetFreeSize() < needed)
		return;

	st_PACKET_HEADER header;
	header.byCode = dfPACKET_CODE;
	header.bySize = payloadSize;
	header.byType = byType;

	s->sendBuffer->Enqueue((char*)&header, sizeof(header));
	if (payloadSize > 0)
		s->sendBuffer->Enqueue(payload, payloadSize);
}

void SendPacket_Around(Session* pMe, BYTE byType, const char* payload, BYTE payloadSize, bool bSendMe)
{
	st_SECTOR_AROUND around;
	GetSectorAround(pMe->p->curSector, &around);

	for (int i = 0; i < around.count; ++i)
	{
		for (auto* s : g_Sector[around.sector[i].y][around.sector[i].x])
		{
			if (s->isDisconnect) continue;
			if (s == pMe && !bSendMe) continue;
			SendPacket(s, byType, payload, payloadSize);
		}
	}
}

//=================================================================
// SC 페이로드 빌더 (mp_ 역할)
//=================================================================

// ID(4) Dir(1) X(2) Y(2) = 9 bytes
static BYTE buildMovePayload(char* buf, int id, BYTE dir, short x, short y)
{
	int off = 0;
	off = packInt  (buf, off, id);
	off = packByte (buf, off, dir);
	off = packShort(buf, off, x);
	off = packShort(buf, off, y);
	return (BYTE)off;
}

// ID(4) X(2) Y(2) = 8 bytes
static BYTE buildSyncPayload(char* buf, int id, short x, short y)
{
	int off = 0;
	off = packInt  (buf, off, id);
	off = packShort(buf, off, x);
	off = packShort(buf, off, y);
	return (BYTE)off;
}

// ID(4) Dir(1) X(2) Y(2) HP(1) = 10 bytes
static BYTE buildCreatePayload(char* buf, int id, BYTE dir, short x, short y, char hp)
{
	int off = 0;
	off = packInt  (buf, off, id);
	off = packByte (buf, off, dir);
	off = packShort(buf, off, x);
	off = packShort(buf, off, y);
	buf[off++] = hp;
	return (BYTE)off;
}

// ID(4) = 4 bytes
static BYTE buildDeletePayload(char* buf, int id)
{
	int off = 0;
	off = packInt(buf, off, id);
	return (BYTE)off;
}

// AttackID(4) DamageID(4) DamageHP(1) = 9 bytes
static BYTE buildDamagePayload(char* buf, int attackID, int damageID, char hp)
{
	int off = 0;
	off = packInt(buf, off, attackID);
	off = packInt(buf, off, damageID);
	buf[off++] = hp;
	return (BYTE)off;
}

//=================================================================
// 접속 / 해제 처리
//=================================================================

void OnSessionConnected(Session* newSession)
{
	Player* p = newSession->p;
	char buf[32];
	BYTE sz;

	// 1. 신규 플레이어 → 자신의 캐릭터 정보
	sz = buildCreatePayload(buf, newSession->id, p->_direction, p->_x, p->_y, p->_hp);
	SendPacket(newSession, dfPACKET_SC_CREATE_MY_CHARACTER, buf, sz);

	// 2. 섹터 등록
	Sector_AddSession(newSession);

	// 3. 주변 9섹터 세션들과 서로 CREATE_OTHER 교환
	st_SECTOR_AROUND around;
	GetSectorAround(p->curSector, &around);

	sz = buildCreatePayload(buf, newSession->id, p->_direction, p->_x, p->_y, p->_hp);

	for (int i = 0; i < around.count; ++i)
	{
		for (auto* s : g_Sector[around.sector[i].y][around.sector[i].x])
		{
			if (s == newSession || s->isDisconnect) continue;

			// 기존 플레이어에게 신규 캐릭터 알림
			SendPacket(s, dfPACKET_SC_CREATE_OTHER_CHARACTER, buf, sz);

			// 신규 플레이어에게 기존 캐릭터 알림
			char sBuf[32];
			BYTE sSz = buildCreatePayload(sBuf, s->id, s->p->_direction, s->p->_x, s->p->_y, s->p->_hp);
			SendPacket(newSession, dfPACKET_SC_CREATE_OTHER_CHARACTER, sBuf, sSz);
		}
	}
}

void OnSessionDisconnected(Session* s)
{
	// 중복 호출 방지 (SendPacket 버퍼오버 → cleanup 루프에서 재호출될 수 있음)
	if (s->notified) return;
	s->notified = true;

	char buf[8];
	BYTE sz = buildDeletePayload(buf, s->id);

	// 주변 9섹터 세션들에게 삭제 알림
	st_SECTOR_AROUND around;
	GetSectorAround(s->p->curSector, &around);

	for (int i = 0; i < around.count; ++i)
	{
		for (auto* target : g_Sector[around.sector[i].y][around.sector[i].x])
		{
			if (target == s || target->isDisconnect) continue;
			SendPacket(target, dfPACKET_SC_DELETE_CHARACTER, buf, sz);
		}
	}

	Sector_RemoveSession(s);
}

//=================================================================
// 방향 유틸
//=================================================================

// 8방향 이동 방향 → 좌/우 facing 업데이트
static void UpdateFacingDirection(Player* p, BYTE moveDir)
{
	switch (moveDir)
	{
	case dfPACKET_MOVE_DIR_RR:
	case dfPACKET_MOVE_DIR_RU:
	case dfPACKET_MOVE_DIR_RD:
		p->_direction = dfPACKET_MOVE_DIR_RR;
		break;
	case dfPACKET_MOVE_DIR_LL:
	case dfPACKET_MOVE_DIR_LU:
	case dfPACKET_MOVE_DIR_LD:
		p->_direction = dfPACKET_MOVE_DIR_LL;
		break;
	// UU, DD: facing 변경 없음
	}
}

// 공격 피격 범위 검사
static bool IsInAttackRange(Session* attacker, Session* target, int rangeX, int rangeY)
{
	short ax = attacker->p->_x, ay = attacker->p->_y;
	short tx = target->p->_x,   ty = target->p->_y;
	int   dx = tx - ax;

	if (attacker->p->_direction == dfPACKET_MOVE_DIR_RR)
	{
		if (dx < 0 || dx > rangeX) return false;
	}
	else // LL
	{
		if (dx > 0 || dx < -rangeX) return false;
	}

	return abs(ty - ay) <= rangeY;
}

//=================================================================
// CS 패킷 핸들러
//=================================================================

static bool PacketProc_MoveStart(Session* s, const char* payload, BYTE size)
{
	if (size < 5) return false;

	int   off = 0;
	BYTE  dir = unpackByte (payload, off);
	short x   = unpackShort(payload, off);
	short y   = unpackShort(payload, off);

	Player* p = s->p;

	// 위치 오차 검증: 서버 기준 위치와 클라이언트 보고 위치 차이가 허용 범위 초과 시 강제 보정
	if (abs(p->_x - x) > dfERROR_RANGE || abs(p->_y - y) > dfERROR_RANGE)
	{
		char buf[16];
		BYTE sz = buildSyncPayload(buf, s->id, p->_x, p->_y);
		SendPacket(s, dfPACKET_SC_SYNC, buf, sz);
		x = p->_x;
		y = p->_y;
	}

	// 방향 변경 여부 확인 (같은 방향 중복 MOVE_START는 브로드캐스트 생략)
	bool dirChanged = (p->_action != dir);

	// 상태 업데이트
	UpdateFacingDirection(p, dir);
	p->_moveDir = dir;
	p->_action  = dir;
	p->_x = x;
	p->_y = y;

	// SC_MOVE_START 브로드캐스트 (방향이 바뀐 경우에만)
	if (dirChanged)
	{
		char buf[16];
		BYTE sz = buildMovePayload(buf, s->id, dir, x, y);
		SendPacket_Around(s, dfPACKET_SC_MOVE_START, buf, sz, true);
	}

	return true;
}

static bool PacketProc_MoveStop(Session* s, const char* payload, BYTE size)
{
	if (size < 5) return false;

	int   off = 0;
	BYTE  dir = unpackByte (payload, off);
	short x   = unpackShort(payload, off);
	short y   = unpackShort(payload, off);

	Player* p = s->p;

	// 위치 오차 검증
	if (abs(p->_x - x) > dfERROR_RANGE || abs(p->_y - y) > dfERROR_RANGE)
	{
		char buf[16];
		BYTE sz = buildSyncPayload(buf, s->id, p->_x, p->_y);
		SendPacket(s, dfPACKET_SC_SYNC, buf, sz);
		x = p->_x;
		y = p->_y;
	}

	// 정지 상태로 전환
	p->_action = PLAYER_ACTION_NONE;
	p->_x = x;
	p->_y = y;

	// SC_MOVE_STOP 브로드캐스트 (자신 포함)
	char buf[16];
	BYTE sz = buildMovePayload(buf, s->id, dir, x, y);
	SendPacket_Around(s, dfPACKET_SC_MOVE_STOP, buf, sz, true);

	return true;
}

// Attack1~3 공통 처리
static bool HandleAttack(Session* s, const char* payload, BYTE size,
	BYTE scType, int rangeX, int rangeY, int8_t damage)
{
	if (size < 5) return false;

	int   off = 0;
	BYTE  dir = unpackByte (payload, off);
	short x   = unpackShort(payload, off);
	short y   = unpackShort(payload, off);

	Player* p = s->p;
	UpdateFacingDirection(p, dir);
	p->_x = x;
	p->_y = y;

	// SC_ATTACK 브로드캐스트
	char buf[32];
	BYTE sz = buildMovePayload(buf, s->id, p->_direction, x, y);
	SendPacket_Around(s, scType, buf, sz, true);

	// 피격 판정 (주변 9섹터 내 세션만 검사)
	st_SECTOR_AROUND atkAround;
	GetSectorAround(s->p->curSector, &atkAround);

	for (int si = 0; si < atkAround.count; ++si)
	for (auto* target : g_Sector[atkAround.sector[si].y][atkAround.sector[si].x])
	{
		if (target == s || target->isDisconnect) continue;
		if (!IsInAttackRange(s, target, rangeX, rangeY)) continue;

		target->p->_hp -= damage;

		// SC_DAMAGE 브로드캐스트
		sz = buildDamagePayload(buf, s->id, target->id, target->p->_hp);
		SendPacket_Around(s, dfPACKET_SC_DAMAGE, buf, sz, true);

		// HP <= 0: 마킹만, 실제 정리는 CleanupDeadSessions() 에서
		if (target->p->_hp <= 0)
		{
			target->isDisconnect = true;
			HardClose(target->socket);
		}
	}

	return true;
}

static bool PacketProc_Attack1(Session* s, const char* payload, BYTE size)
{
	return HandleAttack(s, payload, size,
		dfPACKET_SC_ATTACK1, dfATTACK1_RANGE_X, dfATTACK1_RANGE_Y, dfATTACK1_DAMAGE);
}

static bool PacketProc_Attack2(Session* s, const char* payload, BYTE size)
{
	return HandleAttack(s, payload, size,
		dfPACKET_SC_ATTACK2, dfATTACK2_RANGE_X, dfATTACK2_RANGE_Y, dfATTACK2_DAMAGE);
}

static bool PacketProc_Attack3(Session* s, const char* payload, BYTE size)
{
	return HandleAttack(s, payload, size,
		dfPACKET_SC_ATTACK3, dfATTACK3_RANGE_X, dfATTACK3_RANGE_Y, dfATTACK3_DAMAGE);
}

static bool PacketProc_Echo(Session* s, const char* payload, BYTE size)
{
	if (size < 4) return false;
	// Time(4) 그대로 돌려줌
	SendPacket(s, dfPACKET_SC_ECHO, payload, 4);
	return true;
}

//=================================================================
// 메인 디스패처
//=================================================================

bool PacketProc(Session* s, BYTE byType, const char* payload, BYTE payloadSize)
{
	s->lastRecvTime = GetTickCount64();

	switch (byType)
	{
	case dfPACKET_CS_MOVE_START: return PacketProc_MoveStart(s, payload, payloadSize);
	case dfPACKET_CS_MOVE_STOP:  return PacketProc_MoveStop (s, payload, payloadSize);
	case dfPACKET_CS_ATTACK1:    return PacketProc_Attack1  (s, payload, payloadSize);
	case dfPACKET_CS_ATTACK2:    return PacketProc_Attack2  (s, payload, payloadSize);
	case dfPACKET_CS_ATTACK3:    return PacketProc_Attack3  (s, payload, payloadSize);
	case dfPACKET_CS_ECHO:       return PacketProc_Echo     (s, payload, payloadSize);
	default:
		// 알 수 없는 패킷 타입 → 연결 해제
		return false;
	}
}
