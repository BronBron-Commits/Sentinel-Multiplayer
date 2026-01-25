#pragma once
#include <cstddef>
#include <cstdint>

#if defined(_WIN32)
    #define WIN32_LEAN_AND_MEAN
    #include <winsock2.h>
    using ssize_t = long long;
#else
    #include <sys/types.h>
    #include <netinet/in.h>
#endif

#include "sentinel/net/protocol/snapshot.hpp"

// lifecycle
bool net_init(const char* host, uint16_t port);
void net_shutdown();

// raw transport
ssize_t net_send_raw_to(const void* data, size_t size,
                        const sockaddr_in& addr);
ssize_t net_recv_raw_from(void* out, size_t max,
                          sockaddr_in& from);

// snapshot helpers
bool net_send_snapshot_to(const Snapshot& s,
                           const sockaddr_in& addr);
bool net_poll_snapshot_from(Snapshot& out,
                             sockaddr_in& from);
