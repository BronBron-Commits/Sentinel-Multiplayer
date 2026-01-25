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

ssize_t net_send_raw_to(const void* data, size_t size,
                        const sockaddr_in& addr) {
    if (!sock) return -1;
    return sock->send_to(data, size, addr);
}

ssize_t net_recv_raw_from(void* out, size_t max,
                          sockaddr_in& from) {
    if (!sock) return -1;
    return sock->recv_from(out, max, from);
}

bool net_send_snapshot_to(const Snapshot& s,
                           const sockaddr_in& addr) {
    return net_send_raw_to(&s, sizeof(s), addr) == sizeof(s);
}

bool net_poll_snapshot_from(Snapshot& out,
                             sockaddr_in& from) {
    ssize_t n = net_recv_raw_from(&out, sizeof(out), from);
    return n == sizeof(Snapshot);
}
