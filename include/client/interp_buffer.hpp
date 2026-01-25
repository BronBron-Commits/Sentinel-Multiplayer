#pragma once
#include <deque>
#include "sentinel/net/protocol/snapshot.hpp"

struct InterpBuffer {
    std::deque<Snapshot> buf;
    static constexpr size_t MAX = 32;

    void push(const Snapshot& s) {
        buf.push_back(s);
        while (buf.size() > MAX)
            buf.pop_front();
    }

    bool sample(double t, Snapshot& a, Snapshot& b) const {
        if (buf.size() < 2) return false;

        for (size_t i = 1; i < buf.size(); ++i) {
            if (buf[i].server_time >= t) {
                a = buf[i - 1];
                b = buf[i];
                return true;
            }
        }
        return false;
    }
};
