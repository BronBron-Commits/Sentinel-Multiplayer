#include "math_util.hpp"
#include <cstdlib>

float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

float clamp01(float v) {
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

float frand(float a, float b) {
    return a + (b - a) * (float(rand()) / RAND_MAX);
}
