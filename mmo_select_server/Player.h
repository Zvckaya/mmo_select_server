#pragma once
#include "GameConfig.h"

constexpr unsigned char PLAYER_ACTION_NONE = 0xFF;

class Player
{
public:
	Player();
	Player(short x, short y, unsigned char direction = 0);
	~Player();

	short         _x;
	short         _y;
	char          _hp;
	unsigned char _direction;   // 좌/우 facing (dfPACKET_MOVE_DIR_LL or RR)
	unsigned char _moveDir;     // 8방향 이동 방향
	unsigned char _action;      // 현재 행동 (이동 중: moveDir 값, 정지: PLAYER_ACTION_NONE)

	st_SECTOR_POS curSector;    // 현재 소속 섹터
	st_SECTOR_POS oldSector;    // 이전 섹터 (섹터 전환 시 사용)
	int           sectorIdx;    // g_Sector 벡터 내 자신의 인덱스 (O(1) 삭제용)
};
