#pragma once
#include <unordered_map>
#include "snapshot_buffer.hpp"

class ReplicationClient {
public:
    void ingest(const Snapshot& s);
    const SnapshotBuffer* get(uint32_t player_id) const;

private:
    std::unordered_map<uint32_t, SnapshotBuffer> players;
};
