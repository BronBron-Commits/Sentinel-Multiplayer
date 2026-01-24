#include "sentinel/net/replication/snapshot_buffer.hpp"

void SnapshotBuffer::push(const Snapshot& s) {
    buffer.push_back(s);
    if (buffer.size() > 32)
        buffer.pop_front();
}

Snapshot SnapshotBuffer::sample(float) const {
    if (buffer.empty()) return {};
    return buffer.back();
}
