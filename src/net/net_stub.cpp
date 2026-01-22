#include "net/net_api.hpp"

bool net_init(const char*, uint16_t) {
    return true;
}

void net_shutdown() {}

bool net_tick(NetState&) {
    return false; // no remote update yet
}

bool net_send(const NetState&) {
    return true;
}
