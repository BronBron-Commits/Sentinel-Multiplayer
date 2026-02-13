#pragma once
#include <atomic>
#include <glad/glad.h>

extern std::atomic<int> g_draw_call_count;

// Macro to wrap glBegin and increment draw call count
#define GL_BEGIN_WRAPPED(mode) (g_draw_call_count.fetch_add(1), glBegin(mode))
// Macro to wrap glDrawArrays and increment draw call count
#define GL_DRAWARRAYS_WRAPPED(mode, first, count) (g_draw_call_count.fetch_add(1), glDrawArrays(mode, first, count))
// Macro to wrap glDrawElements and increment draw call count
#define GL_DRAWELEMENTS_WRAPPED(mode, count, type, indices) (g_draw_call_count.fetch_add(1), glDrawElements(mode, count, type, indices))
