#include "sentinel/net/replication/snapshot_buffer.hpp"

void SnapshotBuffer::push(const Snapshot& s) {
    if (!buffer.empty() && s.server_time <= buffer.back().server_time)
        return;

    buffer.push_back(s);

    while (buffer.size() > 64) {
        buffer.pop_front();
    }
}

bool SnapshotBuffer::sample(double t, Snapshot& a, Snapshot& b) const {
    if (buffer.size() < 2)
        return false;

    for (size_t i = 1; i < buffer.size(); ++i) {
        if (buffer[i].server_time >= t) {
            a = buffer[i - 1];
            b = buffer[i];
            return true;
        }
    }

    return false;
}
