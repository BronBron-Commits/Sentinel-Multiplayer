#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

#include "net/net_api.hpp"

#include <cstdio>
#include <cstring>
#include <queue>

// ------------------------------------------------------------
// Globals
// ------------------------------------------------------------
static SOCKET sock = INVALID_SOCKET;
static sockaddr_in server_addr{};
static bool initialized = false;

static std::queue<NetState> incoming_states;
static std::queue<NetEvent> incoming_events;

// ------------------------------------------------------------
// Helpers
// ------------------------------------------------------------
static void fatal(const char* msg) {
    std::fprintf(stderr, "[net] %s (err=%d)\n", msg, WSAGetLastError());
}

// ------------------------------------------------------------
// API
// ------------------------------------------------------------
bool net_init(const char* host, uint16_t port) {
    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        fatal("WSAStartup failed");
        return false;
    }

    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        fatal("socket() failed");
        return false;
    }

    u_long nonblock = 1;
    ioctlsocket(sock, FIONBIO, &nonblock);

    std::memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, host, &server_addr.sin_addr) != 1) {
        fatal("inet_pton failed");
        return false;
    }

    initialized = true;
    std::printf("[net] Windows UDP initialized\n");
    return true;
}

void net_shutdown() {
    if (sock != INVALID_SOCKET) {
        closesocket(sock);
        sock = INVALID_SOCKET;
    }
    WSACleanup();
    initialized = false;
}

bool net_send(const NetState& state) {
    if (!initialized) return false;

    int sent = sendto(
        sock,
        reinterpret_cast<const char*>(&state),
        sizeof(NetState),
        0,
        reinterpret_cast<sockaddr*>(&server_addr),
        sizeof(server_addr)
    );

    return sent == sizeof(NetState);
}

bool net_send_event(const NetEvent& ev) {
    if (!initialized) return false;

    int sent = sendto(
        sock,
        reinterpret_cast<const char*>(&ev),
        sizeof(NetEvent),
        0,
        reinterpret_cast<sockaddr*>(&server_addr),
        sizeof(server_addr)
    );

    return sent == sizeof(NetEvent);
}

bool net_tick(NetState& out) {
    if (!initialized) return false;

    if (!incoming_states.empty()) {
        out = incoming_states.front();
        incoming_states.pop();
        return true;
    }

    NetState tmp{};
    sockaddr_in from{};
    int from_len = sizeof(from);

    int ret = recvfrom(
        sock,
        reinterpret_cast<char*>(&tmp),
        sizeof(tmp),
        0,
        reinterpret_cast<sockaddr*>(&from),
        &from_len
    );

    if (ret == sizeof(NetState)) {
        incoming_states.push(tmp);
    }

    return false;
}

bool net_tick_event(NetEvent& out_event) {
    if (!initialized) return false;

    if (!incoming_events.empty()) {
        out_event = incoming_events.front();
        incoming_events.pop();
        return true;
    }

    NetEvent tmp{};
    sockaddr_in from{};
    int from_len = sizeof(from);

    int ret = recvfrom(
        sock,
        reinterpret_cast<char*>(&tmp),
        sizeof(tmp),
        0,
        reinterpret_cast<sockaddr*>(&from),
        &from_len
    );

    if (ret == sizeof(NetEvent)) {
        incoming_events.push(tmp);
    }

    return false;
}
