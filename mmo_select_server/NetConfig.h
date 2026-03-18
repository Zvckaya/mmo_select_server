#pragma once
#include <cstdint>

constexpr std::uint16_t PORT = 21401;
constexpr int MAX_SESSIONS = 12000;
constexpr int DEFAULT_BUFFER_SIZE = 65536;   // 64KB: 1000명 동시 섹터 전환 버스트 대응
constexpr int SERVER_FRAME_MS     = 1;     // 메인루프 최소 딜레이 (ms) - 25fps는 Update()가 자체 제어
constexpr int UPDATE_FRAME_MS     = 20;    // Update() 처리 주기 (ms) - 25fps

extern int playercnt;

