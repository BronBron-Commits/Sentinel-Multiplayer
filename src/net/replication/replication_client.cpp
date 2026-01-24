#include "sentinel/net/replication/replication_client.hpp"

void ReplicationClient::ingest(const Snapshot& s) {
    players[s.player_id].push(s);
}

const SnapshotBuffer*
ReplicationClient::get(uint32_t id) const {
    auto it = players.find(id);
    return it == players.end() ? nullptr : &it->second;
}
