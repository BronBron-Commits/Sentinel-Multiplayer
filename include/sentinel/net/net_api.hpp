#pragma once
#include "sentinel/net/protocol/snapshot.hpp"

bool net_init(const char* host, uint16_t port);
void net_shutdown();

void net_send_snapshot(const Snapshot& s);
bool net_poll_snapshot(Snapshot& out);
