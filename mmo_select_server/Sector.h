#pragma once
#include <vector>
#include "Session.h"
#include "GameConfig.h"

// 섹터 배열: g_Sector[y][x]
extern std::vector<Session*> g_Sector[dfSECTOR_MAX_Y][dfSECTOR_MAX_X];

//---------------------------------------------------------------
// 섹터 유틸
//---------------------------------------------------------------
st_SECTOR_POS GetSectorPos(short worldX, short worldY);
void          GetSectorAround(st_SECTOR_POS pos, st_SECTOR_AROUND* out);

// oldPos → newPos 이동 시 생긴/없어진 섹터 차분을 구함
void GetUpdateSectorAround(st_SECTOR_POS oldPos, st_SECTOR_POS newPos,
	st_SECTOR_AROUND* outOldOnly, st_SECTOR_AROUND* outNewOnly);

//---------------------------------------------------------------
// 세션 섹터 관리
//---------------------------------------------------------------
void Sector_AddSession   (Session* s);       // 섹터 진입 (접속 시)
void Sector_RemoveSession(Session* s);       // 섹터 제거 (접속 해제 시)
bool Sector_UpdateSession(Session* s);       // 이동 후 섹터 변경 여부 갱신, true = 섹터 변경됨

//---------------------------------------------------------------
// 섹터 전환 시 CREATE / DELETE 패킷 전송
//---------------------------------------------------------------
void CharacterSectorUpdatePacket(Session* s);
