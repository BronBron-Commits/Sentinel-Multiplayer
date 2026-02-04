#pragma once

// Simple camera used by renderer and VFX
struct Camera
{
    struct {
        float x, y, z;
    } pos;

    struct {
        float x, y, z;
    } target;
};
