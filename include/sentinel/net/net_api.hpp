#pragma once
#include <cstddef>
#include <cstdint>

#if defined(_WIN32)
    using ssize_t = long long;
#else
    #include <sys/types.h>
#endif

#include "sentinel/net/protocol/snapshot.hpp"

// lifecycle
bool net_init(const char* host, uint16_t port);
void net_shutdown();

// raw transport
ssize_t net_send_raw(const void* data, size_t size);
ssize_t net_recv_raw(void* out, size_t max);

// snapshot helpers
bool net_send_snapshot(const Snapshot& s);
bool net_poll_snapshot(Snapshot& out);
