#include "sentinel/net/replication/replication_client.hpp"

void ReplicationClient::ingest(const Snapshot& s) {
    players[s.player_id].push(s);
}

bool ReplicationClient::sample(uint32_t id, double t,
                               Snapshot& a, Snapshot& b) {
    auto it = players.find(id);
    if (it == players.end())
        return false;

    return it->second.sample(t, a, b);
}
