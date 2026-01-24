#include "sentinel/net/net_api.hpp"
#include "sentinel/net/transport/udp_socket.hpp"

#include <memory>
#include <cstring>

static std::unique_ptr<UdpSocket> sock;

bool net_init(const char* host, uint16_t port) {
    sock.reset(UdpSocket::create(host, port));
    return sock != nullptr;
}

void net_shutdown() {
    sock.reset();
}

// ---------------- RAW ----------------

ssize_t net_send_raw(const void* data, size_t size) {
    if (!sock) return -1;
    return sock->send(data, size);
}

ssize_t net_recv_raw(void* out, size_t max) {
    if (!sock) return -1;
    return sock->recv(out, max);
}

// ------------- SNAPSHOT --------------

bool net_send_snapshot(const Snapshot& s) {
    return net_send_raw(&s, sizeof(s)) == sizeof(s);
}

bool net_poll_snapshot(Snapshot& out) {
    uint8_t buf[256];
    ssize_t n = net_recv_raw(buf, sizeof(buf));
    if (n != sizeof(Snapshot))
        return false;

    std::memcpy(&out, buf, sizeof(Snapshot));
    return true;
}
