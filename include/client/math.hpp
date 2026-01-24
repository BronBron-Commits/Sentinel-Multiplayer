#pragma once
#include <cmath>

struct Vec3 {
    float x = 0, y = 0, z = 0;
};

inline Vec3 operator+(Vec3 a, Vec3 b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

inline Vec3 operator-(Vec3 a, Vec3 b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

inline Vec3 operator*(Vec3 a, float s) {
    return {a.x * s, a.y * s, a.z * s};
}
