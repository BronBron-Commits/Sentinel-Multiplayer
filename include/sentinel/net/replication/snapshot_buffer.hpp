#pragma once
#include <deque>
#include "sentinel/net/protocol/snapshot.hpp"

class SnapshotBuffer {
public:
    void push(const Snapshot& s);
    bool sample(double render_time, Snapshot& a, Snapshot& b) const;

private:
    std::deque<Snapshot> buffer;
};
