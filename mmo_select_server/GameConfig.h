#pragma once

//-----------------------------------------------------------------
// 수신 타임아웃 - 30초 이상 아무 메시지 수신 없으면 연결 끊음.
//-----------------------------------------------------------------
#define dfNETWORK_PACKET_RECV_TIMEOUT	30000


//-----------------------------------------------------------------
// 화면 이동 범위.
//-----------------------------------------------------------------
#define dfRANGE_MOVE_TOP	0
#define dfRANGE_MOVE_LEFT	0
#define dfRANGE_MOVE_RIGHT	6400
#define dfRANGE_MOVE_BOTTOM	6400


//---------------------------------------------------------------
// 공격범위.
// ※ dfSECTOR_SIZE(400) >> rangeX(100) 이므로 3x3 탐색 범위 내에 항상 포함
//---------------------------------------------------------------
#define dfATTACK1_RANGE_X		80
#define dfATTACK2_RANGE_X		90
#define dfATTACK3_RANGE_X		100
#define dfATTACK1_RANGE_Y		10
#define dfATTACK2_RANGE_Y		10
#define dfATTACK3_RANGE_Y		20


//---------------------------------------------------------------
// 공격 데미지.
//---------------------------------------------------------------
#define dfATTACK1_DAMAGE		1
#define dfATTACK2_DAMAGE		2
#define dfATTACK3_DAMAGE		3


//-----------------------------------------------------------------
// 캐릭터 이동 속도  (50fps 기준: 150px/sec, 100px/sec)
// 25fps 기준이었던 6, 4를 절반으로 줄여 실세계 속도 동일하게 유지
//-----------------------------------------------------------------
#define dfSPEED_PLAYER_X	3
#define dfSPEED_PLAYER_Y	2


//-----------------------------------------------------------------
// 이동 위치체크 오차
//-----------------------------------------------------------------
#define dfERROR_RANGE		50


//-----------------------------------------------------------------
// 플레이어 액션 상태
// 이동 액션은 dfPACKET_MOVE_DIR_* (0~7) 와 동일한 값을 사용
//-----------------------------------------------------------------
#define dfACTION_MOVE_LL	0
#define dfACTION_MOVE_LU	1
#define dfACTION_MOVE_UU	2
#define dfACTION_MOVE_RU	3
#define dfACTION_MOVE_RR	4
#define dfACTION_MOVE_RD	5
#define dfACTION_MOVE_DD	6
#define dfACTION_MOVE_LD	7


//-----------------------------------------------------------------
// 섹터 시스템 상수
// 6400 / 100 = 64 (64x64 섹터 격자로 6400x6400 맵 분할)
// ※ dfSECTOR_SIZE 하한 = dfATTACK3_RANGE_X(100): 3x3 탐색 내 공격 범위 보장
//-----------------------------------------------------------------
#define dfSECTOR_SIZE    100
#define dfSECTOR_MAX_X   64
#define dfSECTOR_MAX_Y   64

struct st_SECTOR_POS
{
	short x;
	short y;
};

struct st_SECTOR_AROUND
{
	st_SECTOR_POS sector[9];
	int           count;
};
