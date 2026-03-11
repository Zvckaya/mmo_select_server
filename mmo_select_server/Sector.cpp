#include "Sector.h"
#include "GameConfig.h"
#include "PacketProc.h"
#include "Protocol.h"
#include "Player.h"
#include <cstring>

std::vector<Session*> g_Sector[dfSECTOR_MAX_Y][dfSECTOR_MAX_X];

//=================================================================
// 내부 페이로드 빌더 (PacketProc 의 static 버전과 동일)
//=================================================================

static int sPackByte (char* buf, int off, BYTE  v) { buf[off] = (char)v; return off + 1; }
static int sPackShort(char* buf, int off, short v) { memcpy(buf + off, &v, 2); return off + 2; }
static int sPackInt  (char* buf, int off, int   v) { memcpy(buf + off, &v, 4); return off + 4; }

// ID(4) Dir(1) X(2) Y(2) HP(1) = 10 bytes
static BYTE buildCreate(char* buf, int id, BYTE dir, short x, short y, char hp)
{
	int off = 0;
	off = sPackInt  (buf, off, id);
	off = sPackByte (buf, off, dir);
	off = sPackShort(buf, off, x);
	off = sPackShort(buf, off, y);
	buf[off++] = hp;
	return (BYTE)off;
}

// ID(4) = 4 bytes
static BYTE buildDelete(char* buf, int id)
{
	int off = 0;
	off = sPackInt(buf, off, id);
	return (BYTE)off;
}

// ID(4) Dir(1) X(2) Y(2) = 9 bytes
static BYTE buildMove(char* buf, int id, BYTE dir, short x, short y)
{
	int off = 0;
	off = sPackInt  (buf, off, id);
	off = sPackByte (buf, off, dir);
	off = sPackShort(buf, off, x);
	off = sPackShort(buf, off, y);
	return (BYTE)off;
}

//=================================================================
// 섹터 유틸
//=================================================================

st_SECTOR_POS GetSectorPos(short worldX, short worldY)
{
	st_SECTOR_POS pos;
	pos.x = worldX / dfSECTOR_SIZE;
	pos.y = worldY / dfSECTOR_SIZE;
	if (pos.x < 0)              pos.x = 0;
	if (pos.x >= dfSECTOR_MAX_X) pos.x = dfSECTOR_MAX_X - 1;
	if (pos.y < 0)              pos.y = 0;
	if (pos.y >= dfSECTOR_MAX_Y) pos.y = dfSECTOR_MAX_Y - 1;
	return pos;
}

void GetSectorAround(st_SECTOR_POS pos, st_SECTOR_AROUND* out)
{
	out->count = 0;
	for (short dy = -1; dy <= 1; ++dy)
	for (short dx = -1; dx <= 1; ++dx)
	{
		short nx = pos.x + dx;
		short ny = pos.y + dy;
		if (nx < 0 || nx >= dfSECTOR_MAX_X) continue;
		if (ny < 0 || ny >= dfSECTOR_MAX_Y) continue;
		out->sector[out->count++] = { nx, ny };
	}
}

void GetUpdateSectorAround(st_SECTOR_POS oldPos, st_SECTOR_POS newPos,
	st_SECTOR_AROUND* outOldOnly, st_SECTOR_AROUND* outNewOnly)
{
	st_SECTOR_AROUND oldAround, newAround;
	GetSectorAround(oldPos, &oldAround);
	GetSectorAround(newPos, &newAround);

	// old 에만 있는 섹터
	outOldOnly->count = 0;
	for (int i = 0; i < oldAround.count; ++i)
	{
		bool found = false;
		for (int j = 0; j < newAround.count; ++j)
		{
			if (oldAround.sector[i].x == newAround.sector[j].x &&
				oldAround.sector[i].y == newAround.sector[j].y)
			{ found = true; break; }
		}
		if (!found)
			outOldOnly->sector[outOldOnly->count++] = oldAround.sector[i];
	}

	// new 에만 있는 섹터
	outNewOnly->count = 0;
	for (int i = 0; i < newAround.count; ++i)
	{
		bool found = false;
		for (int j = 0; j < oldAround.count; ++j)
		{
			if (newAround.sector[i].x == oldAround.sector[j].x &&
				newAround.sector[i].y == oldAround.sector[j].y)
			{ found = true; break; }
		}
		if (!found)
			outNewOnly->sector[outNewOnly->count++] = newAround.sector[i];
	}
}

//=================================================================
// 세션 섹터 관리
//=================================================================

static void SectorVecRemove(std::vector<Session*>& vec, Session* s)
{
	int idx = s->p->sectorIdx;
	int last = (int)vec.size() - 1;
	if (idx != last)
	{
		vec[idx] = vec[last];
		vec[idx]->p->sectorIdx = idx;
	}
	vec.pop_back();
}

void Sector_AddSession(Session* s)
{
	st_SECTOR_POS pos = GetSectorPos(s->p->_x, s->p->_y);
	s->p->curSector = pos;
	s->p->oldSector = pos;
	auto& vec = g_Sector[pos.y][pos.x];
	s->p->sectorIdx = (int)vec.size();
	vec.push_back(s);
}

void Sector_RemoveSession(Session* s)
{
	st_SECTOR_POS pos = s->p->curSector;
	SectorVecRemove(g_Sector[pos.y][pos.x], s);
}

bool Sector_UpdateSession(Session* s)
{
	st_SECTOR_POS newPos = GetSectorPos(s->p->_x, s->p->_y);
	st_SECTOR_POS curPos = s->p->curSector;

	if (newPos.x == curPos.x && newPos.y == curPos.y)
		return false;

	SectorVecRemove(g_Sector[curPos.y][curPos.x], s);
	auto& vec = g_Sector[newPos.y][newPos.x];
	s->p->sectorIdx = (int)vec.size();
	vec.push_back(s);

	s->p->oldSector = curPos;
	s->p->curSector = newPos;
	return true;
}

//=================================================================
// 섹터 전환 시 CREATE / DELETE 패킷 전송
//=================================================================

void CharacterSectorUpdatePacket(Session* s)
{
	st_SECTOR_AROUND oldOnly, newOnly;
	GetUpdateSectorAround(s->p->oldSector, s->p->curSector, &oldOnly, &newOnly);

	// --- 내 삭제 패킷 (old-only 섹터에 있는 세션들에게)
	char delBufMe[8];
	BYTE delSzMe = buildDelete(delBufMe, s->id);

	// --- 내 생성 패킷 (new-only 섹터에 있는 세션들에게)
	char crBufMe[16];
	BYTE crSzMe = buildCreate(crBufMe, s->id, s->p->_direction, s->p->_x, s->p->_y, s->p->_hp);

	// old-only 섹터: 해당 섹터 세션들과 나 서로 DELETE
	for (int i = 0; i < oldOnly.count; ++i)
	{
		for (auto* target : g_Sector[oldOnly.sector[i].y][oldOnly.sector[i].x])
		{
			if (target == s || target->isDisconnect) continue;

			// 상대에게 내 삭제 알림
			SendPacket(target, dfPACKET_SC_DELETE_CHARACTER, delBufMe, delSzMe);

			// 나에게 상대 삭제 알림
			char tDelBuf[8];
			BYTE tDelSz = buildDelete(tDelBuf, target->id);
			SendPacket(s, dfPACKET_SC_DELETE_CHARACTER, tDelBuf, tDelSz);
		}
	}

	// new-only 섹터: 해당 섹터 세션들과 나 서로 CREATE (+이동중이면 MOVE_START)
	for (int i = 0; i < newOnly.count; ++i)
	{
		for (auto* target : g_Sector[newOnly.sector[i].y][newOnly.sector[i].x])
		{
			if (target == s || target->isDisconnect) continue;

			// 상대에게 내 생성 알림
			SendPacket(target, dfPACKET_SC_CREATE_OTHER_CHARACTER, crBufMe, crSzMe);

			// 내가 이동 중이면 상대에게 MOVE_START도 전송
			if (s->p->_action != PLAYER_ACTION_NONE)
			{
				char moveBuf[16];
				BYTE moveSz = buildMove(moveBuf, s->id, s->p->_action, s->p->_x, s->p->_y);
				SendPacket(target, dfPACKET_SC_MOVE_START, moveBuf, moveSz);
			}

			// 나에게 상대 생성 알림
			char tCrBuf[16];
			BYTE tCrSz = buildCreate(tCrBuf, target->id,
				target->p->_direction, target->p->_x, target->p->_y, target->p->_hp);
			SendPacket(s, dfPACKET_SC_CREATE_OTHER_CHARACTER, tCrBuf, tCrSz);

			// 상대가 이동 중이면 나에게 MOVE_START도 전송
			if (target->p->_action != PLAYER_ACTION_NONE)
			{
				char moveBuf[16];
				BYTE moveSz = buildMove(moveBuf, target->id, target->p->_action, target->p->_x, target->p->_y);
				SendPacket(s, dfPACKET_SC_MOVE_START, moveBuf, moveSz);
			}
		}
	}
}
