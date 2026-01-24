#pragma once
#include <deque>
#include "sentinel/net/protocol/snapshot.hpp"

class SnapshotBuffer {
public:
    void push(const Snapshot& s);
    Snapshot sample(float alpha) const;

private:
    std::deque<Snapshot> buffer;
};
