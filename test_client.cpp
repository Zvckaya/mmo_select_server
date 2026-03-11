// test_client.cpp
// 테스트 항목:
//   [1] Echo/Heartbeat   - CS_ECHO(252) 전송 → SC_ECHO(253) 응답 확인
//   [2] Bad Packet Code  - 잘못된 코드(0x88) 전송 → 서버 즉시 끊음 확인
//   [3] Unknown Packet   - 알 수 없는 타입(0xFF) 전송 → 서버 즉시 끊음 확인
//   [4] MAX_SESSIONS     - 1001번째 접속 시 서버가 RST로 차단하는지 확인

#include <WinSock2.h>
#include <Windows.h>
#include <cstdio>
#include <cstring>
#include <vector>

#pragma comment(lib, "Ws2_32.lib")

#define SERVER_PORT    20000
#define SERVER_IP      "127.0.0.1"
#define PACKET_CODE    0x89
#define MAX_SESSIONS   1000

// ── 패킷 구조체 ──────────────────────────────────────────────
#pragma pack(push, 1)
struct PKT_HDR { BYTE code, size, type; };
#pragma pack(pop)

static SOCKET connectServer()
{
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(SERVER_PORT);
    addr.sin_addr.s_addr = inet_addr(SERVER_IP);
    if (connect(s, (SOCKADDR*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        closesocket(s);
        return INVALID_SOCKET;
    }
    return s;
}

// timeout ms 동안 recvBuf 에 누적, 실제 수신 바이트 수 반환
static int recvAll(SOCKET s, char* buf, int bufLen, DWORD timeoutMs)
{
    DWORD t = timeoutMs;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char*)&t, sizeof(t));
    int total = 0;
    int r;
    while (total < bufLen) {
        r = recv(s, buf + total, bufLen - total, 0);
        if (r <= 0) break;
        total += r;
    }
    return total;
}

// buf에서 type 의 첫 패킷 payload 를 out 에 복사, payloadSize 반환. -1=미발견
static int findPacket(const char* buf, int len, BYTE type, char* out, int outLen)
{
    int i = 0;
    while (i + 3 <= len) {
        const PKT_HDR* h = (const PKT_HDR*)(buf + i);
        int total = 3 + h->size;
        if (i + total > len) break;
        if (h->type == type) {
            int copy = (h->size < outLen) ? h->size : outLen;
            memcpy(out, buf + i + 3, copy);
            return copy;
        }
        i += total;
    }
    return -1;
}

static void sendPacket(SOCKET s, BYTE type, const char* payload, BYTE payloadSize)
{
    char buf[512];
    PKT_HDR h{ PACKET_CODE, payloadSize, type };
    memcpy(buf, &h, 3);
    if (payloadSize) memcpy(buf + 3, payload, payloadSize);
    send(s, buf, 3 + payloadSize, 0);
}

static void sendBadPacket(SOCKET s, BYTE badCode, BYTE type)
{
    PKT_HDR h{ badCode, 0, type };
    send(s, (char*)&h, 3, 0);
}

// ── 접속 후 초기 SC 패킷 전부 드레인 ─────────────────────────
static int drainWelcome(SOCKET s, char* buf, int bufLen)
{
    return recvAll(s, buf, bufLen, 500);
}

// ════════════════════════════════════════════════════════════
// [1] Echo / Heartbeat
// ════════════════════════════════════════════════════════════
static bool test_echo()
{
    printf("[Test 1] Echo/Heartbeat\n");
    SOCKET s = connectServer();
    if (s == INVALID_SOCKET) { printf("  FAIL: connect error\n"); return false; }

    char buf[512];
    drainWelcome(s, buf, sizeof(buf));

    DWORD sendTime = 0xDEADBEEF;
    sendPacket(s, 252, (char*)&sendTime, 4);

    memset(buf, 0, sizeof(buf));
    int got = recvAll(s, buf, sizeof(buf), 2000);

    char payload[4];
    int sz = findPacket(buf, got, 253, payload, 4);
    closesocket(s);

    if (sz == 4) {
        DWORD recvTime;
        memcpy(&recvTime, payload, 4);
        if (recvTime == sendTime) {
            printf("  PASS: SC_ECHO received, Time=0x%08X matched\n", recvTime);
            return true;
        }
        printf("  FAIL: SC_ECHO time mismatch (sent=0x%08X, got=0x%08X)\n", sendTime, recvTime);
    } else {
        printf("  FAIL: SC_ECHO not found in %d bytes received\n", got);
    }
    return false;
}

// ════════════════════════════════════════════════════════════
// [2] Bad Packet Code → 서버가 연결을 끊어야 함
// ════════════════════════════════════════════════════════════
static bool test_bad_code()
{
    printf("[Test 2] Bad Packet Code (0x88 instead of 0x89)\n");
    SOCKET s = connectServer();
    if (s == INVALID_SOCKET) { printf("  FAIL: connect error\n"); return false; }

    char buf[512];
    drainWelcome(s, buf, sizeof(buf));

    // 잘못된 코드로 패킷 전송
    sendBadPacket(s, 0x88, 252);

    DWORD t = 3000;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char*)&t, sizeof(t));
    int r = recv(s, buf, sizeof(buf), 0);
    int err = WSAGetLastError();
    closesocket(s);

    // 서버가 FIN 또는 RST 로 끊으면 r==0 또는 WSAECONNRESET
    if (r == 0 || err == WSAECONNRESET) {
        printf("  PASS: Server disconnected (recv=%d, err=%d)\n", r, err);
        return true;
    }
    printf("  FAIL: Server did NOT disconnect (recv=%d, err=%d)\n", r, err);
    return false;
}

// ════════════════════════════════════════════════════════════
// [3] Unknown Packet Type → 서버가 연결을 끊어야 함
// ════════════════════════════════════════════════════════════
static bool test_unknown_type()
{
    printf("[Test 3] Unknown Packet Type (0xFF)\n");
    SOCKET s = connectServer();
    if (s == INVALID_SOCKET) { printf("  FAIL: connect error\n"); return false; }

    char buf[512];
    drainWelcome(s, buf, sizeof(buf));

    // 알 수 없는 타입으로 전송
    sendBadPacket(s, PACKET_CODE, 0xFF);

    DWORD t = 3000;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char*)&t, sizeof(t));
    int r = recv(s, buf, sizeof(buf), 0);
    int err = WSAGetLastError();
    closesocket(s);

    if (r == 0 || err == WSAECONNRESET) {
        printf("  PASS: Server disconnected (recv=%d, err=%d)\n", r, err);
        return true;
    }
    printf("  FAIL: Server did NOT disconnect (recv=%d, err=%d)\n", r, err);
    return false;
}

// ════════════════════════════════════════════════════════════
// [4] MAX_SESSIONS 초과 시 RST 차단
// ════════════════════════════════════════════════════════════
// 소켓들의 recv 버퍼를 논블로킹으로 드레인
// 서버가 SC_CREATE_OTHER_CHARACTER 등을 보내면 클라이언트 TCP 수신 버퍼가 차고,
// 그러면 서버의 송신 버퍼(CRingBuffer)가 포화돼 세션이 강제 끊김 → 실제와 다른 상황
// 실제 게임 클라이언트는 recv 를 계속 하므로 여기서도 드레인해줌
static void drainAll(std::vector<SOCKET>& socks)
{
    char buf[4096];
    u_long nonblock = 1;
    for (auto s : socks)
        ioctlsocket(s, FIONBIO, &nonblock);

    // 50ms 동안 recv 루프
    DWORD start = GetTickCount();
    while (GetTickCount() - start < 50) {
        for (auto s : socks)
            while (recv(s, buf, sizeof(buf), 0) > 0);
    }

    u_long block = 0;
    for (auto s : socks)
        ioctlsocket(s, FIONBIO, &block);
}

static bool test_max_sessions()
{
    printf("[Test 4] MAX_SESSIONS overflow (%d sockets)\n", MAX_SESSIONS);
    printf("  Connecting %d clients... (may take a few seconds)\n", MAX_SESSIONS);

    std::vector<SOCKET> socks;
    socks.reserve(MAX_SESSIONS);

    for (int i = 0; i < MAX_SESSIONS; ++i) {
        SOCKET s = connectServer();
        if (s == INVALID_SOCKET) {
            printf("  WARN: Only %d/%d sockets connected\n", i, MAX_SESSIONS);
            break;
        }
        socks.push_back(s);
        if ((i + 1) % 200 == 0) {
            printf("  ... %d connected\n", i + 1);
            drainAll(socks);   // 서버 송신 버퍼가 포화되지 않도록 중간 드레인
        }
    }

    printf("  Draining recv buffers...\n");
    drainAll(socks);
    Sleep(300); // 서버가 CleanupDeadSessions 처리할 시간
    drainAll(socks);

    printf("  Held %zu connections. Trying %zu+1...\n", socks.size(), socks.size());

    SOCKET extra = connectServer();
    bool rejected = false;

    if (extra == INVALID_SOCKET) {
        printf("  PASS: TCP connect itself refused\n");
        rejected = true;
    } else {
        DWORD t = 3000;
        setsockopt(extra, SOL_SOCKET, SO_RCVTIMEO, (char*)&t, sizeof(t));
        char buf[32];
        int r = recv(extra, buf, sizeof(buf), 0);
        int err = WSAGetLastError();
        closesocket(extra);

        if (r == 0 || err == WSAECONNRESET) {
            printf("  PASS: Extra connection RST/closed by server (recv=%d, err=%d)\n", r, err);
            rejected = true;
        } else {
            printf("  FAIL: Extra connection NOT rejected! (recv=%d bytes, err=%d)\n", r, err);
        }
    }

    printf("  Closing %zu held sockets...\n", socks.size());
    for (auto s : socks) closesocket(s);
    return rejected;
}

// ════════════════════════════════════════════════════════════
// main
// ════════════════════════════════════════════════════════════
int main()
{
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    printf("╔══════════════════════════════════════════════╗\n");
    printf("║  MMO Select Server - Test Client             ║\n");
    printf("║  Target: %s:%d                        ║\n", SERVER_IP, SERVER_PORT);
    printf("╚══════════════════════════════════════════════╝\n\n");

    int pass = 0, fail = 0;

    auto run = [&](bool ok) {
        if (ok) { ++pass; printf("  → PASS\n\n"); }
        else    { ++fail; printf("  → FAIL\n\n"); }
    };

    run(test_echo());
    run(test_bad_code());
    run(test_unknown_type());
    run(test_max_sessions());

    printf("══════════════════════════════════════════\n");
    printf("Result: %d PASS / %d FAIL\n", pass, fail);
    printf("══════════════════════════════════════════\n");

    WSACleanup();
    return (fail == 0) ? 0 : 1;
}
