#include "sentinel/net/net_api.hpp"
#include "sentinel/net/transport/udp_socket.hpp"

static UdpSocket* g_socket = nullptr;

bool net_init(const char* host, uint16_t port) {
    g_socket = UdpSocket::create(host, port);
    return g_socket != nullptr;
}

void net_shutdown() {
    delete g_socket;
    g_socket = nullptr;
}

void net_send_snapshot(const Snapshot& s) {
    if (!g_socket) return;
    g_socket->send(&s, sizeof(Snapshot));
}

bool net_poll_snapshot(Snapshot& out) {
    if (!g_socket) return false;
    return g_socket->recv(&out, sizeof(Snapshot)) == sizeof(Snapshot);
}
