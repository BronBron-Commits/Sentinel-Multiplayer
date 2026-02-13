#pragma once

#include <openxr/openxr.h>
#include <string>

#include <vector>

typedef void (*XrRenderViewCallback)(int viewIndex, const XrView& view, int width, int height, unsigned int colorTex);

bool xr_initialize();
void xr_shutdown();

XrInstance xr_get_instance();

// Create/destroy a session bound to the current OpenGL context (WGL).
bool xr_create_session_for_current_opengl_context();
void xr_destroy_session();

// Poll events; returns false when the app should quit
bool xr_poll_events();

// Render a single frame. The provided callback will be invoked once per view
// with the swapchain texture to render into. Returns false on error or if
// session isn't running.
bool xr_render_frame(XrRenderViewCallback cb);

bool xr_is_session_running();

// Retrieve latest located hand poses (in app space). Returns false if not available.
bool xr_get_hand_poses(XrPosef& leftPose, XrPosef& rightPose);

bool xr_initialize();
void xr_shutdown();

XrInstance xr_get_instance();

std::string get_active_openxr_runtime();
