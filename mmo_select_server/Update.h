#pragma once

// 게임 로직 업데이트 (25fps 자체 제어)
// - HP <= 0 사망 처리
// - 수신 타임아웃 연결 해제
// - 이동 처리 (action -> x, y 갱신)
void Update();
