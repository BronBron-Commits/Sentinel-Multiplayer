#include "net/net_api.hpp"
#include "net/net_event.hpp"

#include <cstring>
#include <cstdio>
#include <queue>

// ------------------------------------------------------------
// Platform sockets
// ------------------------------------------------------------

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    using socklen_t = int;
#else
    #include <sys/socket.h>
    #include <arpa/inet.h>
    #include <unistd.h>
#endif

// ------------------------------------------------------------
// Socket state
// ------------------------------------------------------------

static int sock_fd = -1;
static sockaddr_in peer_addr{};
static sockaddr_in last_sender{};
static socklen_t last_sender_len = sizeof(last_sender);
static bool has_peer = false;

// ------------------------------------------------------------
// EVENT QUEUE (local stub delivery)
// ------------------------------------------------------------

static std::queue<NetEvent> event_queue;

// ------------------------------------------------------------
// INIT / SHUTDOWN
// ------------------------------------------------------------

bool net_init(const char* addr, uint16_t port) {

#ifdef _WIN32
    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::fprintf(stderr, "WSAStartup failed\n");
        return false;
    }
#endif

    sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_fd < 0) {
        perror("socket");
        return false;
    }

    sockaddr_in local{};
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = INADDR_ANY;

    // Client: bind ephemeral port
    // Server: bind fixed port
    uint16_t bind_port =
        (std::strcmp(addr, "0.0.0.0") == 0) ? port : 0;

    local.sin_port = htons(bind_port);

    if (bind(sock_fd, (sockaddr*)&local, sizeof(local)) < 0) {
        perror("bind");

#ifdef _WIN32
        closesocket(sock_fd);
        WSACleanup();
#else
        close(sock_fd);
#endif
        sock_fd = -1;
        return false;
    }

    peer_addr.sin_family = AF_INET;
    peer_addr.sin_port = htons(port);
    inet_pton(AF_INET, addr, &peer_addr.sin_addr);

    has_peer = false;
    return true;
}

void net_shutdown() {
    if (sock_fd >= 0) {
#ifdef _WIN32
        closesocket(sock_fd);
        WSACleanup();
#else
        close(sock_fd);
#endif
        sock_fd = -1;
    }
}

// ------------------------------------------------------------
// STATE SYNC
// ------------------------------------------------------------

bool net_send(const NetState& state) {
    if (sock_fd < 0)
        return false;

    ssize_t sent = sendto(
        sock_fd,
        (const char*)&state,
        sizeof(NetState),
        0,
        (sockaddr*)&peer_addr,
        sizeof(peer_addr)
    );

    return sent == sizeof(NetState);
}

bool net_tick(NetState& out_state) {
    if (sock_fd < 0)
        return false;

    NetState incoming{};
    ssize_t r = recvfrom(
        sock_fd,
        (char*)&incoming,
        sizeof(NetState),
        MSG_DONTWAIT,
        (sockaddr*)&last_sender,
        &last_sender_len
    );

    if (r == sizeof(NetState)) {
        has_peer = true;
        out_state = incoming;
        return true;
    }

    return false;
}

// ------------------------------------------------------------
// EVENT API (STUB IMPLEMENTATION)
// ------------------------------------------------------------

bool net_send_event(const NetEvent& e) {
    // Stub behavior:
    // Immediately deliver locally (single-process testing)
    event_queue.push(e);
    return true;
}

bool net_tick_event(NetEvent& out_event) {
    if (event_queue.empty())
        return false;

    out_event = event_queue.front();
    event_queue.pop();
    return true;
}

