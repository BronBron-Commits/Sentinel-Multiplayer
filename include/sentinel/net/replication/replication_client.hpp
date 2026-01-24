#pragma once
#include <unordered_map>
#include "sentinel/net/replication/snapshot_buffer.hpp"

class ReplicationClient {
public:
    void ingest(const Snapshot& s);
    bool sample(uint32_t player_id, double render_time,
                Snapshot& a, Snapshot& b);

private:
    std::unordered_map<uint32_t, SnapshotBuffer> players;
};
