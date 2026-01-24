#pragma once
#include <cstdint>
#include "sentinel/net/protocol/snapshot.hpp"

// server + client
bool net_init(const char* host, uint16_t port);
void net_shutdown();

// server → network
void net_send_snapshot(const Snapshot& s);

// client ← network
bool net_poll_snapshot(Snapshot& out);
