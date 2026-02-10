#include "openxr_helper.hpp"
#include <cstring>
#include <vector>
#include <iostream>

#include <windows.h>
#include <glad/glad.h>
#include <GL/gl.h>
#ifndef XR_USE_PLATFORM_WIN32
#define XR_USE_PLATFORM_WIN32 1
#endif
#ifndef XR_USE_GRAPHICS_API_OPENGL
#define XR_USE_GRAPHICS_API_OPENGL 1
#endif
#include <openxr/openxr_platform.h>

// Provide fallback definitions for the OpenXR OpenGL KHR extension types
// in case the installed OpenXR headers do not expose them.
#ifndef XR_KHR_opengl_enable
typedef struct XrGraphicsBindingOpenGLWin32KHR {
    XrStructureType type;
    void* next;
    HDC hDC;
    HGLRC hGLRC;
} XrGraphicsBindingOpenGLWin32KHR;

typedef struct XrSwapchainImageOpenGLKHR {
    XrStructureType type;
    void* next;
    GLuint image;
} XrSwapchainImageOpenGLKHR;
#endif

static XrInstance g_instance = XR_NULL_HANDLE;
static XrSystemId g_system = XR_NULL_SYSTEM_ID;
static XrSession g_session = XR_NULL_HANDLE;
static XrSpace g_appSpace = XR_NULL_HANDLE;
static bool g_sessionRunning = false;

struct SwapchainState {
    XrSwapchain handle{XR_NULL_HANDLE};
    int width{0}, height{0};
    std::vector<XrSwapchainImageOpenGLKHR> images;
    std::vector<uint32_t> imageIndices;
};

static std::vector<SwapchainState> g_swapchains;
static uint32_t g_viewCount = 0;
static GLuint g_fbo = 0;
// Action / input state
static XrActionSet g_actionSet = XR_NULL_HANDLE;
static XrAction g_leftHandAction = XR_NULL_HANDLE;
static XrAction g_rightHandAction = XR_NULL_HANDLE;
static XrSpace g_leftHandSpace = XR_NULL_HANDLE;
static XrSpace g_rightHandSpace = XR_NULL_HANDLE;
static XrPosef g_leftHandPose{ {0,0,0,1}, {0,0,0} };
static XrPosef g_rightHandPose{ {0,0,0,1}, {0,0,0} };

bool xr_initialize()
{
    if (g_instance != XR_NULL_HANDLE)
        return true;

    // Query available extensions and enable XR_KHR_opengl_enable if present
    uint32_t extCount = 0;
    xrEnumerateInstanceExtensionProperties(nullptr, 0, &extCount, nullptr);
    std::vector<XrExtensionProperties> exts(extCount, {XR_TYPE_EXTENSION_PROPERTIES});
    xrEnumerateInstanceExtensionProperties(nullptr, extCount, &extCount, exts.data());

    std::vector<const char*> enabledExts;
    for (const auto &e : exts) {
        if (std::strcmp(e.extensionName, "XR_KHR_opengl_enable") == 0) {
            enabledExts.push_back("XR_KHR_opengl_enable");
            break;
        }
    }

    XrInstanceCreateInfo createInfo{XR_TYPE_INSTANCE_CREATE_INFO};
    XrApplicationInfo appInfo{};
    std::strncpy(appInfo.applicationName, "sentinel", XR_MAX_APPLICATION_NAME_SIZE);
    appInfo.applicationVersion = 1;
    std::strncpy(appInfo.engineName, "sentinel_engine", XR_MAX_ENGINE_NAME_SIZE);
    appInfo.engineVersion = 1;
    appInfo.apiVersion = XR_CURRENT_API_VERSION;
    createInfo.applicationInfo = appInfo;

    if (!enabledExts.empty()) {
        createInfo.enabledExtensionCount = (uint32_t)enabledExts.size();
        createInfo.enabledExtensionNames = enabledExts.data();
    }

XrResult res = xrCreateInstance(&createInfo, &g_instance);

if (!XR_SUCCEEDED(res)) {
    std::cerr << "[XR] No OpenXR runtime installed — desktop mode\n";
    g_instance = XR_NULL_HANDLE;
    return true; // desktop mode continues, NOT fatal
}




    return true;
}

void xr_shutdown()
{
    if (g_instance != XR_NULL_HANDLE) {
        xrDestroyInstance(g_instance);
        g_instance = XR_NULL_HANDLE;
    }
}

XrInstance xr_get_instance()
{
    return g_instance;
}

static bool ensure_fbo()
{
    if (g_fbo == 0) {
        glGenFramebuffers(1, &g_fbo);
    }
    return g_fbo != 0;
}

bool xr_create_session_for_current_opengl_context()
{
// XR not available → desktop mode
if (g_instance == XR_NULL_HANDLE) {
    std::cerr << "[XR] OpenXR unavailable — desktop mode\n";
    g_system = XR_NULL_SYSTEM_ID;
    g_session = XR_NULL_HANDLE;
    return true; // continue in desktop mode
}



XrSystemGetInfo sysInfo{XR_TYPE_SYSTEM_GET_INFO};
sysInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
XrResult r = xrGetSystem(g_instance, &sysInfo, &g_system);

if (!XR_SUCCEEDED(r)) {
    std::cerr << "[XR] No HMD detected — desktop mode\n";
    g_system = XR_NULL_SYSTEM_ID;
    return true; // continue without VR
}

if (!XR_SUCCEEDED(r)) {
    std::cerr << "[XR] xrGetSystem failed: " << r << std::endl;
    return false;
}

    // Create session with WGL binding
    HDC hdc = wglGetCurrentDC();
    HGLRC hglrc = wglGetCurrentContext();
    if (!hdc || !hglrc) {
        std::cerr << "No current WGL context found" << std::endl;
        return false;
    }

    XrGraphicsBindingOpenGLWin32KHR glBinding{XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR};
    glBinding.hDC = hdc;
    glBinding.hGLRC = hglrc;

    // Per OpenXR spec, call the OpenGL graphics requirements query when the
    // XR_KHR_opengl_enable extension is present. xrCreateSession will fail
    // with XR_ERROR_GRAPHICS_REQUIREMENTS_CALL_MISSING (-50) if this is omitted.
    PFN_xrGetOpenGLGraphicsRequirementsKHR pfnGetOpenGLGraphicsRequirementsKHR = nullptr;
    xrGetInstanceProcAddr(g_instance, "xrGetOpenGLGraphicsRequirementsKHR", reinterpret_cast<PFN_xrVoidFunction*>(&pfnGetOpenGLGraphicsRequirementsKHR));
    if (pfnGetOpenGLGraphicsRequirementsKHR) {
        XrGraphicsRequirementsOpenGLKHR gr{XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_KHR};
        XrResult grr = pfnGetOpenGLGraphicsRequirementsKHR(g_instance, g_system, &gr);
        if (!XR_SUCCEEDED(grr)) {
            std::cerr << "xrGetOpenGLGraphicsRequirementsKHR failed: " << grr << std::endl;
            return false;
        }
        // Optionally inspect gr.minApiVersionSupported / gr.maxApiVersionSupported
        // to verify the current OpenGL context version is compatible.
    } else {
        // If the function isn't available, the runtime may not expose the
        // extension function even if the extension name exists; log and continue
        // — xrCreateSession will still return XR_ERROR_GRAPHICS_REQUIREMENTS_CALL_MISSING
        // if the call was required by the runtime.
        std::cerr << "xrGetOpenGLGraphicsRequirementsKHR not available" << std::endl;
    }

    XrSessionCreateInfo sci{XR_TYPE_SESSION_CREATE_INFO};
    sci.next = &glBinding;
    sci.systemId = g_system;

r = xrCreateSession(g_instance, &sci, &g_session);
if (!XR_SUCCEEDED(r)) {
    std::cerr << "[XR] Could not create VR session — desktop mode\n";
    g_session = XR_NULL_HANDLE;
    return true; // continue without VR
}



    // Create reference space (local)
    XrReferenceSpaceCreateInfo rc{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
    rc.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
    rc.poseInReferenceSpace = { {0,0,0,1}, {0,0,0} };
    r = xrCreateReferenceSpace(g_session, &rc, &g_appSpace);
    if (!XR_SUCCEEDED(r)) {
        std::cerr << "xrCreateReferenceSpace failed: " << r << std::endl;
        // not fatal; continue without app space
        g_appSpace = XR_NULL_HANDLE;
    }

    // Query view configuration to determine swapchain sizes
    uint32_t viewCount = 0;
    r = xrEnumerateViewConfigurationViews(g_instance, g_system, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0, &viewCount, nullptr);
    if (!XR_SUCCEEDED(r) || viewCount == 0) {
        std::cerr << "xrEnumerateViewConfigurationViews failed: " << r << std::endl;
        return false;
    }

    std::vector<XrViewConfigurationView> viewConfigs(viewCount, {XR_TYPE_VIEW_CONFIGURATION_VIEW});
    r = xrEnumerateViewConfigurationViews(g_instance, g_system, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, viewCount, &viewCount, viewConfigs.data());
    if (!XR_SUCCEEDED(r)) {
        std::cerr << "xrEnumerateViewConfigurationViews2 failed: " << r << std::endl;
        return false;
    }

    g_viewCount = viewCount;
    g_swapchains.clear();
    g_swapchains.resize(viewCount);

    for (uint32_t i = 0; i < viewCount; ++i) {
        int width = (int)viewConfigs[i].recommendedImageRectWidth;
        int height = (int)viewConfigs[i].recommendedImageRectHeight;

        XrSwapchainCreateInfo sci2{XR_TYPE_SWAPCHAIN_CREATE_INFO};
        sci2.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT;
        sci2.format = GL_RGBA8; // prefer RGBA8
        sci2.sampleCount = viewConfigs[i].recommendedSwapchainSampleCount;
        sci2.width = width;
        sci2.height = height;
        sci2.faceCount = 1;
        sci2.arraySize = 1;
        sci2.mipCount = 1;

        XrSwapchain swapchain = XR_NULL_HANDLE;
        r = xrCreateSwapchain(g_session, &sci2, &swapchain);
        if (!XR_SUCCEEDED(r)) {
            std::cerr << "xrCreateSwapchain failed: " << r << std::endl;
            return false;
        }

        // enumerate images
        uint32_t imgCount = 0;
        xrEnumerateSwapchainImages(swapchain, 0, &imgCount, nullptr);
        std::vector<XrSwapchainImageOpenGLKHR> images(imgCount, {XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR});
        xrEnumerateSwapchainImages(swapchain, imgCount, &imgCount, reinterpret_cast<XrSwapchainImageBaseHeader*>(images.data()));

        g_swapchains[i].handle = swapchain;
        g_swapchains[i].width = width;
        g_swapchains[i].height = height;
        g_swapchains[i].images = std::move(images);
        g_swapchains[i].imageIndices.resize(g_swapchains[i].images.size());
    }

    ensure_fbo();

    // Create a simple action set and hand pose actions
    XrActionSetCreateInfo asc{XR_TYPE_ACTION_SET_CREATE_INFO};
    std::strncpy(asc.actionSetName, "gameplay", XR_MAX_ACTION_SET_NAME_SIZE);
    std::strncpy(asc.localizedActionSetName, "Gameplay", XR_MAX_LOCALIZED_ACTION_SET_NAME_SIZE);
    asc.priority = 0;
    xrCreateActionSet(g_instance, &asc, &g_actionSet);

    XrActionCreateInfo aci{XR_TYPE_ACTION_CREATE_INFO};
    aci.actionType = XR_ACTION_TYPE_POSE_INPUT;
    std::strncpy(aci.actionName, "left_hand_pose", XR_MAX_ACTION_NAME_SIZE);
    std::strncpy(aci.localizedActionName, "Left Hand Pose", XR_MAX_LOCALIZED_ACTION_NAME_SIZE);
    xrCreateAction(g_actionSet, &aci, &g_leftHandAction);

    std::strncpy(aci.actionName, "right_hand_pose", XR_MAX_ACTION_NAME_SIZE);
    std::strncpy(aci.localizedActionName, "Right Hand Pose", XR_MAX_LOCALIZED_ACTION_NAME_SIZE);
    xrCreateAction(g_actionSet, &aci, &g_rightHandAction);

    // Attach action set to session
    XrSessionActionSetsAttachInfo attachInfo{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
    attachInfo.countActionSets = 1;
    attachInfo.actionSets = &g_actionSet;
    xrAttachSessionActionSets(g_session, &attachInfo);

    // Create action spaces for left/right hands
    XrPath leftPath = XR_NULL_PATH;
    XrPath rightPath = XR_NULL_PATH;
    xrStringToPath(g_instance, "/user/hand/left", &leftPath);
    xrStringToPath(g_instance, "/user/hand/right", &rightPath);

    XrActionSpaceCreateInfo asci{XR_TYPE_ACTION_SPACE_CREATE_INFO};
    asci.action = g_leftHandAction;
    asci.subactionPath = leftPath;
    asci.poseInActionSpace = { {0,0,0,1}, {0,0,0} };
    xrCreateActionSpace(g_session, &asci, &g_leftHandSpace);

    asci.action = g_rightHandAction;
    asci.subactionPath = rightPath;
    xrCreateActionSpace(g_session, &asci, &g_rightHandSpace);

    return true;
}

void xr_destroy_session()
{
    if (g_session != XR_NULL_HANDLE) {
        xrDestroySession(g_session);
        g_session = XR_NULL_HANDLE;
    }
    if (g_appSpace != XR_NULL_HANDLE) {
        xrDestroySpace(g_appSpace);
        g_appSpace = XR_NULL_HANDLE;
    }
    for (auto &s : g_swapchains) {
        if (s.handle != XR_NULL_HANDLE) xrDestroySwapchain(s.handle);
        s.handle = XR_NULL_HANDLE;
    }
    g_swapchains.clear();
    if (g_fbo) { glDeleteFramebuffers(1, &g_fbo); g_fbo = 0; }
    g_sessionRunning = false;
}

bool xr_poll_events()
{
    if (g_instance == XR_NULL_HANDLE) return false;

    XrEventDataBuffer event{XR_TYPE_EVENT_DATA_BUFFER};
    while (xrPollEvent(g_instance, &event) == XR_SUCCESS) {
        switch (event.type) {
        case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING:
            return false;
        case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED: {
            auto ev = *reinterpret_cast<XrEventDataSessionStateChanged*>(&event);
            if (ev.session == g_session) {
                if (ev.state == XR_SESSION_STATE_READY) {
                    XrSessionBeginInfo bi{XR_TYPE_SESSION_BEGIN_INFO};
                    bi.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                    xrBeginSession(g_session, &bi);
                    g_sessionRunning = true;
                } else if (ev.state == XR_SESSION_STATE_STOPPING) {
                    xrEndSession(g_session);
                    g_sessionRunning = false;
                } else if (ev.state == XR_SESSION_STATE_EXITING || ev.state == XR_SESSION_STATE_LOSS_PENDING) {
                    return false;
                }
            }
        } break;
        default:
            break;
        }
        event.type = XR_TYPE_EVENT_DATA_BUFFER; // reset for next
    }
    return true;
}

bool xr_get_hand_poses(XrPosef& leftPose, XrPosef& rightPose)
{
if (!g_sessionRunning || g_session == XR_NULL_HANDLE) {
    // XR inactive → desktop mode
    return false;
}

    leftPose = g_leftHandPose;
    rightPose = g_rightHandPose;
    return true;
}

bool xr_is_session_running()
{
    return g_sessionRunning;
}

bool xr_render_frame(XrRenderViewCallback cb)
{
    if (!g_sessionRunning || g_session == XR_NULL_HANDLE) return false;

    XrFrameState frameState{XR_TYPE_FRAME_STATE};
    XrFrameWaitInfo waitInfo{XR_TYPE_FRAME_WAIT_INFO};
    XrResult r = xrWaitFrame(g_session, &waitInfo, &frameState);
    if (!XR_SUCCEEDED(r)) return false;

    XrFrameBeginInfo bi{XR_TYPE_FRAME_BEGIN_INFO};
    xrBeginFrame(g_session, &bi);

    // locate views
    std::vector<XrView> views(g_viewCount, {XR_TYPE_VIEW});
    XrViewState viewState{XR_TYPE_VIEW_STATE};
    XrViewLocateInfo locateInfo{XR_TYPE_VIEW_LOCATE_INFO};
    locateInfo.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
    locateInfo.displayTime = frameState.predictedDisplayTime;
    locateInfo.space = g_appSpace == XR_NULL_HANDLE ? XR_NULL_PATH : g_appSpace;

    xrLocateViews(g_session, &locateInfo, &viewState, (uint32_t)views.size(), &g_viewCount, views.data());

    // Sync actions to update action states (hand poses)
    if (g_actionSet != XR_NULL_HANDLE) {
        XrActionsSyncInfo syncInfo{XR_TYPE_ACTIONS_SYNC_INFO};
        syncInfo.countActiveActionSets = 1;
        XrActiveActionSet aas{};
        aas.actionSet = g_actionSet;
        aas.subactionPath = XR_NULL_PATH;
        syncInfo.activeActionSets = &aas;
        xrSyncActions(g_session, &syncInfo);

        // locate left/right hand spaces in app space
        if (g_leftHandSpace != XR_NULL_HANDLE && g_appSpace != XR_NULL_HANDLE) {
            XrSpaceLocation loc{XR_TYPE_SPACE_LOCATION};
            xrLocateSpace(g_leftHandSpace, g_appSpace, frameState.predictedDisplayTime, &loc);
            if (loc.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) g_leftHandPose.position = loc.pose.position;
            if (loc.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) g_leftHandPose.orientation = loc.pose.orientation;
        }
        if (g_rightHandSpace != XR_NULL_HANDLE && g_appSpace != XR_NULL_HANDLE) {
            XrSpaceLocation loc{XR_TYPE_SPACE_LOCATION};
            xrLocateSpace(g_rightHandSpace, g_appSpace, frameState.predictedDisplayTime, &loc);
            if (loc.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) g_rightHandPose.position = loc.pose.position;
            if (loc.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) g_rightHandPose.orientation = loc.pose.orientation;
        }
    }

    // For each view, acquire, wait, render, release
    std::vector<XrCompositionLayerProjectionView> layerViews(g_viewCount, {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW});

    for (uint32_t i = 0; i < g_viewCount; ++i) {
        auto &sc = g_swapchains[i];
        uint32_t imgIndex = 0;
        XrSwapchainImageAcquireInfo ai{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
        xrAcquireSwapchainImage(sc.handle, &ai, &imgIndex);

        XrSwapchainImageWaitInfo wi{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
        wi.timeout = XR_INFINITE_DURATION;
        xrWaitSwapchainImage(sc.handle, &wi);

        // Bind texture to FBO and call user render callback
        if (ensure_fbo()) {
            glBindFramebuffer(GL_FRAMEBUFFER, g_fbo);
            GLuint tex = sc.images[imgIndex].image;
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
            GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
            if (status == GL_FRAMEBUFFER_COMPLETE) {
                glViewport(0, 0, sc.width, sc.height);
                if (cb) cb((int)i, views[i], sc.width, sc.height, tex);
            }
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }

        XrSwapchainImageReleaseInfo ri{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
        xrReleaseSwapchainImage(sc.handle, &ri);

        // compose projection view
        layerViews[i].pose = views[i].pose;
        layerViews[i].fov = views[i].fov;
        layerViews[i].subImage.swapchain = sc.handle;
        layerViews[i].subImage.imageRect.offset = {0,0};
        layerViews[i].subImage.imageRect.extent = { (int32_t)sc.width, (int32_t)sc.height };
        layerViews[i].subImage.imageArrayIndex = 0;
    }

    XrCompositionLayerProjection projLayer{XR_TYPE_COMPOSITION_LAYER_PROJECTION};
    projLayer.space = g_appSpace == XR_NULL_HANDLE ? XR_NULL_PATH : g_appSpace;
    projLayer.viewCount = (uint32_t)layerViews.size();
    projLayer.views = layerViews.data();

    const XrCompositionLayerBaseHeader* layers[] = { reinterpret_cast<const XrCompositionLayerBaseHeader*>(&projLayer) };

    XrFrameEndInfo endInfo{XR_TYPE_FRAME_END_INFO};
    endInfo.displayTime = frameState.predictedDisplayTime;
    endInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
    endInfo.layerCount = 1;
    endInfo.layers = layers;

    xrEndFrame(g_session, &endInfo);

    return true;
}
